#include "Format.h"

#include <QCoreApplication>
#include <QLocale>
#include <QStringList>

namespace claudedial::core::format {
namespace {

QTime truncateToMinute(QTime time)
{
    return QTime(time.hour(), time.minute());
}

/// "Mon 09:00" inside the coming week, "14 Sep 09:00" beyond it.
///
/// Rounded to the nearest minute: the 7-day window is rolling, so its reset
/// timestamp drifts by fractions of a second between calls - enough to flip
/// 03:59:59.6 to 04:00:00.4 and back, which would show up as the displayed time
/// flickering between two adjacent minutes.
QString absoluteWhen(const QDateTime& resetAt, const QDateTime& now)
{
    const QDateTime local = resetAt.addSecs(30).toLocalTime();
    const QLocale locale;

    // Inside a week a weekday reads faster than a date; beyond it, a date is clearer.
    const qint64 days = now.toLocalTime().date().daysTo(local.date());
    if (days >= 0 && days < 7) {
        return QStringLiteral("%1 %2").arg(
            locale.dayName(local.date().dayOfWeek(), QLocale::ShortFormat),
            locale.toString(truncateToMinute(local.time()), QStringLiteral("HH:mm")));
    }
    return locale.toString(local, QStringLiteral("d MMM HH:mm"));
}

/// "37m", "1h 52m", "3d 5h" - two units at most, and never a zero component:
/// "2h" rather than "2h 0m". The day tier exists because the seven-day window
/// is shown as a countdown too, and "76h 30m" is not a duration anyone reads.
QString durationText(qint64 totalMinutes)
{
    const qint64 days = totalMinutes / (24 * 60);
    if (days > 0) {
        const qint64 hours = (totalMinutes % (24 * 60)) / 60;
        return hours == 0 ? QStringLiteral("%1d").arg(days)
                          : QStringLiteral("%1d %2h").arg(days).arg(hours);
    }

    const qint64 hours = totalMinutes / 60;
    const qint64 minutes = totalMinutes % 60;
    if (hours == 0)
        return QStringLiteral("%1m").arg(minutes);
    if (minutes == 0)
        return QStringLiteral("%1h").arg(hours);
    return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
}

} // namespace

QString resetRelative(const QDateTime& resetAt, const QDateTime& now)
{
    const qint64 seconds = now.secsTo(resetAt);
    if (seconds <= 30)
        return QCoreApplication::translate("format", "resets now");
    return QCoreApplication::translate("format", "resets in %1").arg(durationText((seconds + 30) / 60));
}

QString resetAbsolute(const QDateTime& resetAt, const QDateTime& now)
{
    return QCoreApplication::translate("format", "resets %1").arg(absoluteWhen(resetAt, now));
}

QString resetSentence(PeriodKind, const UsagePeriod& period, const QDateTime& now)
{
    if (!period.resetAt)
        return {};

    // A countdown for both windows, which is what Claude Code itself shows, so
    // the two readings do not need translating between each other.
    const qint64 seconds = now.secsTo(*period.resetAt);
    if (seconds <= 30)
        return QCoreApplication::translate("format", "Resets now");
    return QCoreApplication::translate("format", "Resets in %1")
        .arg(durationText((seconds + 30) / 60));
}

QString resetFor(PeriodKind, const UsagePeriod& period, const QDateTime& now)
{
    if (!period.resetAt)
        return {};
    // Both windows read as a countdown now; the kind is kept in the signature
    // because callers pass it and the two may diverge again.
    return resetRelative(*period.resetAt, now);
}

QString pace(PeriodKind kind, const UsagePeriod& period, const QDateTime& now)
{
    const auto through = windowProgress(kind, period, now);
    if (!through)
        return {};

    return QCoreApplication::translate("format", "Usage %1% · window %2%")
        .arg(qRound(period.percentage))
        .arg(qRound(*through));
}

QString updatedAgo(const QDateTime& updatedAt, const QDateTime& now)
{
    if (!updatedAt.isValid())
        return QCoreApplication::translate("format", "Never updated");

    const qint64 seconds = updatedAt.secsTo(now);
    if (seconds < 60)
        return QCoreApplication::translate("format", "Updated just now");
    if (seconds < 3600)
        return QCoreApplication::translate("format", "Updated %1 min ago").arg(seconds / 60);
    if (seconds < 24 * 3600)
        return QCoreApplication::translate("format", "Updated %1h ago").arg(seconds / 3600);
    return QCoreApplication::translate("format", "Updated %1d ago").arg(seconds / (24 * 3600));
}

QString tooltip(const UsageState& state, const QDateTime& now)
{
    QStringList lines { QStringLiteral("ClaudeDial") };

    const auto line = [&](PeriodKind kind, const QString& label) {
        const auto& period = state.period(kind);
        if (!period)
            return;
        const int percent = qRound(period->percentage);
        QStringList bits { QStringLiteral("%1%").arg(percent) };
        if (const auto through = windowProgress(kind, *period, now))
            bits << QCoreApplication::translate("format", "%1% through").arg(qRound(*through));
        if (const QString reset = resetFor(kind, *period, now); !reset.isEmpty())
            bits << reset;
        lines << QStringLiteral("%1  %2").arg(label, bits.join(QStringLiteral(" · ")));
    };

    line(PeriodKind::FiveHour, QStringLiteral("5h"));
    line(PeriodKind::SevenDay, QStringLiteral("7d"));

    if (!state.isValid())
        lines << QCoreApplication::translate("format", "no data");
    else if (state.stale)
        lines << QCoreApplication::translate("format", "stale · %1").arg(updatedAgo(state.updatedAt, now));

    return lines.join(QLatin1Char('\n'));
}

} // namespace claudedial::core::format
