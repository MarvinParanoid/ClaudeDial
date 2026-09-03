#include "core/Config.h"
#include "core/Format.h"
#include "core/UsageClient.h"
#include "core/UsageJson.h"
#include "core/UsageLevel.h"
#include "core/UsageState.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>
#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

using namespace claudometer::core;

class CoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void parsesRealResponse();
    void ignoresUnknownKeys();
    void toleratesNullWindows();
    void toleratesMissingResetTimestamp();
    void clampsOutOfRangeUtilization();
    void rejectsGarbage();
    void rejectsResponseWithNoUsableWindow();

    void formatsRelativeReset();
    void formatsResetPerWindowKind();
    void roundsAbsoluteResetToNearestMinute();
    void buildsTooltip();
    void formatsUpdatedAgoWithoutPluralPlaceholders();
    void capitalisesResetForItsOwnLine();
    void rampsLevelsMonotonically();

    void emitsDocumentedJsonShape();
    void emitsValidJsonWhenUnavailable();
    void roundTripsStatusJson();

    void clampsRefreshIntervalToFloor();
    void storesSettingsAtTheDocumentedPath();
    void roundTripsSettings();
    void defaultsSuitALinuxTray();
    void remembersAnnouncedThresholdsAcrossRestarts();
};

void CoreTest::initTestCase()
{
    // Never touch the real ~/.config while testing Config.
    QStandardPaths::setTestModeEnabled(true);
}

namespace {

/// The shape observed live, trimmed. See docs/usage-api.md section 3.
QByteArray realResponse()
{
    return R"({
      "five_hour": {
        "utilization": 6.0,
        "resets_at": "2026-09-03T21:30:00.382338+00:00",
        "limit_dollars": null,
        "locked_reason": null
      },
      "seven_day": {
        "utilization": 26.0,
        "resets_at": "2026-09-08T04:00:00.382358+00:00"
      },
      "seven_day_opus": null,
      "nimbus_quill": { "utilization": 0.0, "resets_at": null },
      "extra_usage": { "is_enabled": false },
      "limits": [ { "kind": "session", "percent": 6 } ],
      "member_dashboard_available": false
    })";
}

} // namespace

void CoreTest::parsesRealResponse()
{
    const auto state = UsageClient::parseResponse(realResponse());
    QVERIFY(state.has_value());

    QVERIFY(state->fiveHour.has_value());
    QCOMPARE(state->fiveHour->percentage, 6.0);
    QVERIFY(state->fiveHour->resetAt.has_value());
    QCOMPARE(state->fiveHour->resetAt->toUTC(),
             QDateTime(QDate(2026, 9, 3), QTime(21, 30, 0, 382), QTimeZone::UTC));

    QVERIFY(state->sevenDay.has_value());
    QCOMPARE(state->sevenDay->percentage, 26.0);
    QVERIFY(state->sevenDay->resetAt.has_value());
}

void CoreTest::ignoresUnknownKeys()
{
    // New quota buckets appear without notice; they must be non-events.
    const QByteArray body = R"({
      "five_hour": { "utilization": 10, "resets_at": null },
      "a_brand_new_bucket": { "utilization": 99, "resets_at": null },
      "something_else": [1, 2, 3]
    })";

    const auto state = UsageClient::parseResponse(body);
    QVERIFY(state.has_value());
    QCOMPARE(state->fiveHour->percentage, 10.0);
    QVERIFY(!state->sevenDay.has_value());
}

void CoreTest::toleratesNullWindows()
{
    QVERIFY(!UsageClient::parseWindow(QJsonValue(QJsonValue::Null)).has_value());
    QVERIFY(!UsageClient::parseWindow(QJsonValue()).has_value());
    QVERIFY(!UsageClient::parseWindow(QJsonValue(QStringLiteral("nonsense"))).has_value());

    // Present but with no usable utilization.
    QJsonObject object;
    object[QStringLiteral("resets_at")] = QStringLiteral("2026-09-03T21:30:00+00:00");
    QVERIFY(!UsageClient::parseWindow(object).has_value());
}

void CoreTest::toleratesMissingResetTimestamp()
{
    // Observed on real buckets: a valid percentage with a null reset time. The
    // percentage must survive and the reset must stay absent rather than
    // becoming a bogus timestamp.
    QJsonObject object;
    object[QStringLiteral("utilization")] = 42.0;
    object[QStringLiteral("resets_at")] = QJsonValue(QJsonValue::Null);

    const auto period = UsageClient::parseWindow(object);
    QVERIFY(period.has_value());
    QCOMPARE(period->percentage, 42.0);
    QVERIFY(!period->resetAt.has_value());

    // And a malformed one is treated the same way.
    object[QStringLiteral("resets_at")] = QStringLiteral("not a timestamp");
    const auto malformed = UsageClient::parseWindow(object);
    QVERIFY(malformed.has_value());
    QVERIFY(!malformed->resetAt.has_value());
}

void CoreTest::clampsOutOfRangeUtilization()
{
    QJsonObject object;
    object[QStringLiteral("utilization")] = 140.0;
    QCOMPARE(UsageClient::parseWindow(object)->percentage, 100.0);

    object[QStringLiteral("utilization")] = -3.0;
    QCOMPARE(UsageClient::parseWindow(object)->percentage, 0.0);
}

void CoreTest::rejectsGarbage()
{
    QVERIFY(!UsageClient::parseResponse(QByteArrayLiteral("<html>nope</html>")).has_value());
    QVERIFY(!UsageClient::parseResponse(QByteArrayLiteral("")).has_value());
    QVERIFY(!UsageClient::parseResponse(QByteArrayLiteral("[1,2,3]")).has_value());
}

void CoreTest::rejectsResponseWithNoUsableWindow()
{
    // A 200 that carries neither window is a failure, not an empty state - the
    // caller must keep the previous good data instead of showing 0%.
    const auto state = UsageClient::parseResponse(
        QByteArrayLiteral(R"({"five_hour": null, "seven_day": null})"));
    QVERIFY(!state.has_value());
}

void CoreTest::formatsRelativeReset()
{
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    QCOMPARE(format::resetRelative(now.addSecs(112 * 60), now), QStringLiteral("resets in 1h 52m"));
    QCOMPARE(format::resetRelative(now.addSecs(4 * 60), now), QStringLiteral("resets in 4m"));
    QCOMPARE(format::resetRelative(now.addSecs(5), now), QStringLiteral("resets now"));
    QCOMPARE(format::resetRelative(now.addSecs(-60), now), QStringLiteral("resets now"));
}

void CoreTest::formatsResetPerWindowKind()
{
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    UsagePeriod rolling;
    rolling.percentage = 63;
    rolling.resetAt = now.addSecs(112 * 60);

    // The 5-hour window is rolling, so it gets a countdown.
    QCOMPARE(format::resetFor(PeriodKind::FiveHour, rolling, now),
             QStringLiteral("resets in 1h 52m"));

    // The 7-day window lands on a schedule, so it gets a wall-clock time.
    QVERIFY(format::resetFor(PeriodKind::SevenDay, rolling, now).startsWith(QStringLiteral("resets ")));

    // No timestamp means no reset line at all, rather than an invented one.
    UsagePeriod noReset;
    noReset.percentage = 5;
    QVERIFY(format::resetFor(PeriodKind::FiveHour, noReset, now).isEmpty());
}

void CoreTest::roundsAbsoluteResetToNearestMinute()
{
    // The rolling 7-day reset drifts sub-second between calls. Without rounding
    // the displayed time flickers between two adjacent minutes.
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);
    const QDateTime justUnder(QDate(2026, 9, 8), QTime(3, 59, 59, 600), QTimeZone::UTC);
    const QDateTime justOver(QDate(2026, 9, 8), QTime(4, 0, 0, 400), QTimeZone::UTC);

    QCOMPARE(format::resetAbsolute(justUnder, now), format::resetAbsolute(justOver, now));
}

void CoreTest::buildsTooltip()
{
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    UsageState state;
    state.updatedAt = now;
    state.fiveHour = UsagePeriod { 63.0, now.addSecs(112 * 60) };
    state.sevenDay = UsagePeriod { 41.0, now.addDays(4) };

    const QStringList lines = format::tooltip(state, now).split(QLatin1Char('\n'));
    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(0), QStringLiteral("Claudometer"));
    QCOMPARE(lines.at(1), QStringLiteral("5h  63% · resets in 1h 52m"));
    QVERIFY(lines.at(2).startsWith(QStringLiteral("7d  41% · resets ")));
}

void CoreTest::formatsUpdatedAgoWithoutPluralPlaceholders()
{
    // Qt's %n plural form prints the source string verbatim when no translator
    // is installed, which is how "Updated 1 minute(s) ago" reached the popup.
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    QCOMPARE(format::updatedAgo(now.addSecs(-5), now), QStringLiteral("Updated just now"));
    QCOMPARE(format::updatedAgo(now.addSecs(-60), now), QStringLiteral("Updated 1 min ago"));
    QCOMPARE(format::updatedAgo(now.addSecs(-120), now), QStringLiteral("Updated 2 min ago"));
    QCOMPARE(format::updatedAgo(now.addSecs(-7200), now), QStringLiteral("Updated 2h ago"));

    // Guard the specific defect rather than just the happy path.
    for (const int seconds : { 5, 60, 120, 3600, 7200, 90000 }) {
        const QString text = format::updatedAgo(now.addSecs(-seconds), now);
        QVERIFY2(!text.contains(QLatin1Char('(')), qPrintable(text));
        QVERIFY2(!text.contains(QStringLiteral("%n")), qPrintable(text));
    }
}

void CoreTest::capitalisesResetForItsOwnLine()
{
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    UsagePeriod period;
    period.percentage = 63;
    period.resetAt = now.addSecs(112 * 60);

    // Lower case after the tooltip's separator, sentence case on its own line.
    QCOMPARE(format::resetFor(PeriodKind::FiveHour, period, now),
             QStringLiteral("resets in 1h 52m"));
    QCOMPARE(format::resetSentence(PeriodKind::FiveHour, period, now),
             QStringLiteral("Resets in 1h 52m"));
    QVERIFY(format::resetSentence(PeriodKind::SevenDay, period, now)
                .startsWith(QStringLiteral("Resets ")));

    // No timestamp still means no line at all.
    UsagePeriod noReset;
    QVERIFY(format::resetSentence(PeriodKind::FiveHour, noReset, now).isEmpty());
}

void CoreTest::rampsLevelsMonotonically()
{
    QVERIFY(levelFor(0, 75, 90) == UsageLevel::Normal);
    QVERIFY(levelFor(74.9, 75, 90) == UsageLevel::Normal);
    QVERIFY(levelFor(75, 75, 90) == UsageLevel::Warning);
    QVERIFY(levelFor(89.9, 75, 90) == UsageLevel::Warning);
    QVERIFY(levelFor(90, 75, 90) == UsageLevel::Critical);
    QVERIFY(levelFor(95, 75, 90) == UsageLevel::Severe);
    QVERIFY(levelFor(100, 75, 90) == UsageLevel::LimitReached);

    QCOMPARE(levelName(UsageLevel::Severe), QStringLiteral("severe"));

    // A critical threshold above the fixed severe mark, and a warning threshold
    // above the critical one, are both allowed by the settings UI. Neither may
    // produce a ramp that goes backwards as the percentage rises.
    for (const auto thresholds : { std::pair { 75, 98 }, std::pair { 90, 80 } }) {
        UsageLevel previous = UsageLevel::Normal;
        for (double percentage = 0; percentage <= 100; percentage += 0.5) {
            const UsageLevel level = levelFor(percentage, thresholds.first, thresholds.second);
            QVERIFY2(static_cast<int>(level) >= static_cast<int>(previous),
                     qPrintable(QStringLiteral("regressed at %1%").arg(percentage)));
            previous = level;
        }
    }
}

void CoreTest::emitsDocumentedJsonShape()
{
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    UsageState state;
    state.updatedAt = now;
    state.fiveHour = UsagePeriod { 63.4, now.addSecs(112 * 60) };
    state.sevenDay = UsagePeriod { 41.0, std::nullopt };

    const QJsonObject root = QJsonDocument::fromJson(json::status(state, 75, 90, now)).object();

    QCOMPARE(root.value(QStringLiteral("five_hour")).toObject().value(QStringLiteral("usage")).toInt(), 63);
    QVERIFY(root.value(QStringLiteral("five_hour")).toObject().value(QStringLiteral("reset_at")).isString());
    QCOMPARE(root.value(QStringLiteral("seven_day")).toObject().value(QStringLiteral("usage")).toInt(), 41);
    QVERIFY(root.value(QStringLiteral("seven_day")).toObject().value(QStringLiteral("reset_at")).isNull());
    QCOMPARE(root.value(QStringLiteral("updated_at")).toString(), QStringLiteral("2026-09-03T12:00:00Z"));
    QCOMPARE(root.value(QStringLiteral("stale")).toBool(), false);

    // Waybar keys.
    QCOMPARE(root.value(QStringLiteral("text")).toString(), QStringLiteral("63%"));
    QCOMPARE(root.value(QStringLiteral("class")).toString(), QStringLiteral("normal"));
    QVERIFY(!root.value(QStringLiteral("tooltip")).toString().isEmpty());

    // Nothing identifying must ever reach this output.
    const QByteArray raw = json::status(state, 75, 90, now);
    QVERIFY(!raw.contains("org"));
    QVERIFY(!raw.contains("Bearer"));
    QVERIFY(!raw.contains("token"));
}

void CoreTest::emitsValidJsonWhenUnavailable()
{
    // A status bar must still get parseable JSON when there is nothing to show.
    const QJsonObject root =
        QJsonDocument::fromJson(json::unavailable(QStringLiteral("not signed in"))).object();

    QCOMPARE(root.value(QStringLiteral("text")).toString(), QStringLiteral("--"));
    QCOMPARE(root.value(QStringLiteral("class")).toString(), QStringLiteral("unavailable"));
    QVERIFY(root.value(QStringLiteral("five_hour")).isNull());
}

void CoreTest::roundTripsStatusJson()
{
    // The CLI reads a running instance's answer back through this, so that a
    // status bar polling --json costs no API request. If the two halves drift,
    // the CLI silently falls back to spending quota.
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    UsageState original;
    original.updatedAt = now;
    original.stale = true;
    original.fiveHour = UsagePeriod { 63.0, now.addSecs(112 * 60) };
    original.sevenDay = UsagePeriod { 41.0, std::nullopt };

    const auto restored = json::parseStatus(json::status(original, 75, 90, now));
    QVERIFY(restored.has_value());

    QVERIFY(restored->fiveHour.has_value());
    QCOMPARE(restored->fiveHour->percentage, 63.0);
    QCOMPARE(restored->fiveHour->resetAt->toUTC(), original.fiveHour->resetAt->toUTC());

    QVERIFY(restored->sevenDay.has_value());
    QCOMPARE(restored->sevenDay->percentage, 41.0);
    QVERIFY(!restored->sevenDay->resetAt.has_value());

    QCOMPARE(restored->updatedAt.toUTC(), now);
    QCOMPARE(restored->stale, true);

    // The "nothing to report" payload must not read back as a usable state, or
    // the CLI would print zeroes instead of saying why it has no data.
    QVERIFY(!json::parseStatus(json::unavailable(QStringLiteral("not signed in"))).has_value());
    QVERIFY(!json::parseStatus(QByteArrayLiteral("garbage")).has_value());
}

void CoreTest::clampsRefreshIntervalToFloor()
{
    // The rate-limit bucket is per access token and shared with other monitors,
    // so aggressive polling must not be configurable.
    Config config;
    config.setRefreshIntervalSeconds(5);
    QCOMPARE(config.refreshIntervalSeconds(), Config::kMinimumRefreshSeconds);

    config.setRefreshIntervalSeconds(600);
    QCOMPARE(config.refreshIntervalSeconds(), 600);
}

void CoreTest::storesSettingsAtTheDocumentedPath()
{
    // The README and the packaging notes both promise
    // ~/.config/claudometer/claudometer.conf. QSettings derives the extension
    // from the format, so IniFormat would quietly produce a .ini instead and
    // every hand-written config file would be ignored.
    const Config config;
    const QString path = config.filePath();

    QVERIFY2(path.endsWith(QStringLiteral(".conf")), qPrintable(path));
    QVERIFY2(path.contains(QStringLiteral("/claudometer/claudometer.")), qPrintable(path));
}

void CoreTest::roundTripsSettings()
{
    Config config;

    config.setWarningThreshold(60);
    config.setCriticalThreshold(85);
    config.setNotificationsEnabled(false);
    config.setTrayStyle(Config::TrayStyle::Gauge);
    config.setTheme(Config::Theme::Dark);

    const Config reread;
    QCOMPARE(reread.warningThreshold(), 60);
    QCOMPARE(reread.criticalThreshold(), 85);
    QCOMPARE(reread.notificationsEnabled(), false);
    QVERIFY(reread.trayStyle() == Config::TrayStyle::Gauge);
    QVERIFY(reread.theme() == Config::Theme::Dark);
}

void CoreTest::defaultsSuitALinuxTray()
{
    // Guard the defaults that were deliberately chosen rather than inherited:
    // the tray shows the number, because that needs no hover and nothing to
    // interpret, and the theme follows the desktop.
    QStandardPaths::setTestModeEnabled(true);
    QSettings(QSettings::NativeFormat, QSettings::UserScope,
              QStringLiteral("claudometer"), QStringLiteral("claudometer"))
        .clear();

    const Config config;
    QVERIFY(config.trayStyle() == Config::TrayStyle::Percentage);
    QVERIFY(config.theme() == Config::Theme::System);
    QCOMPARE(config.refreshIntervalSeconds(), Config::kDefaultRefreshSeconds);
    QCOMPARE(config.warningThreshold(), 75);
    QCOMPARE(config.criticalThreshold(), 90);
    QCOMPARE(config.notificationsEnabled(), true);
}

void CoreTest::remembersAnnouncedThresholdsAcrossRestarts()
{
    // Without this the app re-announces every threshold already crossed in the
    // current window each time it starts, so a login repeats the warning for a
    // window the user is still in.
    const QDateTime reset(QDate(2026, 9, 3), QTime(21, 30, 0), QTimeZone::UTC);

    {
        Config config;
        config.setFiredThresholds(QStringLiteral("five_hour"), { 75, 90 }, reset);
    }

    const Config reread;
    auto restored = reread.firedThresholds(QStringLiteral("five_hour"));
    std::sort(restored.begin(), restored.end());
    QCOMPARE(restored, QList<int>({ 75, 90 }));
    QCOMPARE(reread.firedWindowReset(QStringLiteral("five_hour")).toUTC(), reset);

    // A window with nothing recorded must come back empty, not stale.
    QVERIFY(reread.firedThresholds(QStringLiteral("seven_day")).isEmpty());
    QVERIFY(!reread.firedWindowReset(QStringLiteral("seven_day")).isValid());
}

QTEST_GUILESS_MAIN(CoreTest)
#include "tst_core.moc"
