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

std::optional<UsagePeriod> parsePeriod(const QJsonValue& value)
{
    if (!value.isObject())
        return std::nullopt;
    const QJsonObject object = value.toObject();
    if (!object.value(QStringLiteral("usage")).isDouble())
        return std::nullopt;

    UsagePeriod period;
    period.percentage = object.value(QStringLiteral("usage")).toDouble();
    if (const auto reset = object.value(QStringLiteral("reset_at")); reset.isString()) {
        if (auto parsed = QDateTime::fromString(reset.toString(), Qt::ISODate); parsed.isValid())
            period.resetAt = parsed.toUTC();
    }
    return period;
}

std::optional<UsageState> parseStatus(const QByteArray& payload)
{
    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;

    const QJsonObject root = doc.object();

    UsageState state;
    state.fiveHour = parsePeriod(root.value(QStringLiteral("five_hour")));
    state.sevenDay = parsePeriod(root.value(QStringLiteral("seven_day")));
    state.stale = root.value(QStringLiteral("stale")).toBool();
    if (const auto updated = root.value(QStringLiteral("updated_at")); updated.isString())
        state.updatedAt = QDateTime::fromString(updated.toString(), Qt::ISODate).toUTC();

    if (!state.isValid())
        return std::nullopt;
    return state;
}

} // namespace claudometer::core::json
