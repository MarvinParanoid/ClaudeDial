#include "UsageJson.h"

#include "Format.h"
#include "UsageLevel.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace claudometer::core::json {
namespace {

QJsonObject periodObject(const UsagePeriod& period)
{
    QJsonObject object;
    object[QStringLiteral("usage")] = qRound(period.percentage);
    object[QStringLiteral("reset_at")] = period.resetAt
        ? QJsonValue(period.resetAt->toUTC().toString(Qt::ISODate))
        : QJsonValue(QJsonValue::Null);
    return object;
}

/// Waybar styles a module by this class name.
QString cssClass(const UsageState& state, int warning, int critical)
{
    if (!state.isValid())
        return QStringLiteral("unavailable");
    if (state.stale)
        return QStringLiteral("stale");
    if (!state.fiveHour)
        return QStringLiteral("normal");

    return levelName(levelFor(state.fiveHour->percentage, warning, critical));
}

} // namespace

QByteArray status(const UsageState& state, int warningThreshold, int criticalThreshold,
                  const QDateTime& now)
{
    QJsonObject root;

    if (state.fiveHour)
        root[QStringLiteral("five_hour")] = periodObject(*state.fiveHour);
    else
        root[QStringLiteral("five_hour")] = QJsonValue(QJsonValue::Null);

    if (state.sevenDay)
        root[QStringLiteral("seven_day")] = periodObject(*state.sevenDay);
    else
        root[QStringLiteral("seven_day")] = QJsonValue(QJsonValue::Null);

    root[QStringLiteral("updated_at")] = state.updatedAt.isValid()
        ? QJsonValue(state.updatedAt.toUTC().toString(Qt::ISODate))
        : QJsonValue(QJsonValue::Null);
    root[QStringLiteral("stale")] = state.stale;

    // Waybar convenience keys.
    root[QStringLiteral("text")] = state.fiveHour
        ? QStringLiteral("%1%").arg(qRound(state.fiveHour->percentage))
        : QStringLiteral("--");
    root[QStringLiteral("tooltip")] = format::tooltip(state, now);
    root[QStringLiteral("class")] = cssClass(state, warningThreshold, criticalThreshold);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QByteArray unavailable(const QString& reason)
{
    QJsonObject root;
    root[QStringLiteral("five_hour")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("seven_day")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("updated_at")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("stale")] = true;
    root[QStringLiteral("text")] = QStringLiteral("--");
    root[QStringLiteral("tooltip")] = reason;
    root[QStringLiteral("class")] = QStringLiteral("unavailable");
    root[QStringLiteral("error")] = reason;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace claudometer::core::json
