#include "Format.h"

#include "UsageLevel.h"

#include <QCoreApplication>
#include <QStringList>

namespace claudedial::core::format {
namespace {

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

QString menuEntry(PeriodKind kind, const UsageState& state, const QDateTime& now)
{
    const auto& period = state.period(kind);
    if (!period)
        return {};

    // The same words the popup and the notifications use. Someone reading this
    // in a menu and then opening the popup should not have to match up two
    // namings of the same window.
    const QString label = kind == PeriodKind::FiveHour
        ? QCoreApplication::translate("format", "Session")
        : QCoreApplication::translate("format", "Weekly");

    QString line = QStringLiteral("%1 %2%").arg(label).arg(qRound(period->percentage));
    if (const QString reset = resetFor(kind, *period, now); !reset.isEmpty())
        line += QStringLiteral(" · %1").arg(reset);
    return line;
}

QString thresholdTitle(int threshold, int criticalThreshold)
{
    if (threshold >= kLimitThreshold)
        return QCoreApplication::translate("format", "Limit reached");
    if (threshold >= kSevereThreshold)
        return QCoreApplication::translate("format", "Almost at the limit");
    // The configured critical threshold reads as "high", the warning one as a
    // warning; there is no third wording to invent between them.
    if (threshold >= criticalThreshold)
        return QCoreApplication::translate("format", "High usage");
    return QCoreApplication::translate("format", "Usage warning");
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
