#include "core/Autostart.h"
#include "core/Config.h"
#include "core/Credentials.h"
#include "core/Format.h"
#include "core/PanelTheme.h"
#include "core/RefreshSchedule.h"
#include "core/GaugeGeometry.h"
#include "core/UsageClient.h"
#include "core/UsageJson.h"
#include "core/UsageLevel.h"
#include "core/UsageState.h"

#include <QJsonDocument>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QtMath>

#include <algorithm>
#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

using namespace claudedial::core;

class CoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void readsThePlasmaPanelColour();
    void parsesRealResponse();
    void ignoresUnknownKeys();
    void toleratesNullWindows();
    void toleratesMissingResetTimestamp();
    void clampsOutOfRangeUtilization();
    void rejectsGarbage();
    void readsRetryAfterInBothForms();
    void rejectsResponseWithNoUsableWindow();

    void formatsRelativeReset();
    void formatsResetPerWindowKind();
    void roundsAbsoluteResetToNearestMinute();
    void buildsTooltip();
    void buildsMenuEntries();
    void measuresPositionWithinTheFiveHourWindow();
    void refusesToGuessTheWindowWhenItCannotKnow();
    void formatsUpdatedAgoWithoutPluralPlaceholders();
    void capitalisesResetForItsOwnLine();
    void rampsLevelsMonotonically();

    void emitsDocumentedJsonShape();
    void emitsValidJsonWhenUnavailable();
    void roundTripsStatusJson();

    void clampsRefreshIntervalToFloor();
    void climbsTheRateLimitLadder();
    void neverPollsSoonerThanTheServerAsked();
    void alignsToAResetOnlyWhenNotRateLimited();
    void shippedIconMatchesCanonicalGeometry();
    void storesSettingsAtTheDocumentedPath();
    void roundTripsSettings();
    void defaultsSuitALinuxTray();
    void remembersAnnouncedThresholdsAcrossRestarts();

    void writesTheAutostartEntryWhereTheDesktopReadsIt();

    void classifiesTheCredentialFile();
    void prefersAnExplicitTokenFromTheEnvironment();
    void neverNamesTheTokenItHolds();
};

void CoreTest::initTestCase()
{
    // Never touch the real ~/.config while testing Config.
    QStandardPaths::setTestModeEnabled(true);
}

namespace {

/// Somewhere disposable for the tests that write.
///
/// QStandardPaths test mode redirects files, and QSettings::NativeFormat is a
/// file only on Linux - the registry on Windows, a plist on macOS - so on those
/// platforms it redirects nothing. These names do instead, on all three, which
/// is what lets the writing tests run everywhere rather than skipping and
/// leaving the registry and LaunchAgent code uncovered.
constexpr auto kTestOrganisation = "claudedial-test";
constexpr auto kTestApplication = "claudedial-test";

} // namespace

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

void CoreTest::readsRetryAfterInBothForms()
{
    const QDateTime now(QDate(2026, 9, 4), QTime(12, 0, 0), QTimeZone::UTC);

    // The specification allows a delay in seconds or an absolute date, and a
    // server that tells us when to come back should be obeyed rather than
    // guessed at.
    QCOMPARE(UsageClient::parseRetryAfter(QByteArrayLiteral("120"), now), 120);
    QCOMPARE(UsageClient::parseRetryAfter(QByteArrayLiteral("  120  "), now), 120);
    QCOMPARE(UsageClient::parseRetryAfter(
                 QByteArrayLiteral("Fri, 04 Sep 2026 12:05:00 GMT"), now), 300);

    // Absent, unparseable, or already past: fall back to our own ladder.
    QCOMPARE(UsageClient::parseRetryAfter(QByteArray(), now), 0);
    QCOMPARE(UsageClient::parseRetryAfter(QByteArrayLiteral("soon"), now), 0);
    QCOMPARE(UsageClient::parseRetryAfter(QByteArrayLiteral("0"), now), 0);
    QCOMPARE(UsageClient::parseRetryAfter(QByteArrayLiteral("-30"), now), 0);
    QCOMPARE(UsageClient::parseRetryAfter(
                 QByteArrayLiteral("Fri, 04 Sep 2026 11:00:00 GMT"), now), 0);

    // A header asking for three days must not freeze the indicator until the
    // user restarts it.
    QCOMPARE(UsageClient::parseRetryAfter(QByteArrayLiteral("259200"), now), 3600);
}

void CoreTest::formatsRelativeReset()
{
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    QCOMPARE(format::resetRelative(now.addSecs(112 * 60), now), QStringLiteral("resets in 1h 52m"));
    QCOMPARE(format::resetRelative(now.addSecs(4 * 60), now), QStringLiteral("resets in 4m"));

    // A zero component is noise: "2h", not "2h 0m"; "37m", not "0h 37m".
    QCOMPARE(format::resetRelative(now.addSecs(120 * 60), now), QStringLiteral("resets in 2h"));
    QCOMPARE(format::resetRelative(now.addSecs(60 * 60), now), QStringLiteral("resets in 1h"));
    QCOMPARE(format::resetRelative(now.addSecs(37 * 60), now), QStringLiteral("resets in 37m"));
    QCOMPARE(format::resetRelative(now.addSecs(5), now), QStringLiteral("resets now"));

    // Days, because the seven-day window is a countdown too and "76h 30m" is
    // not a duration anyone reads. Two units at most, no zero component.
    QCOMPARE(format::resetRelative(now.addSecs(3 * 24 * 3600 + 5 * 3600), now),
             QStringLiteral("resets in 3d 5h"));
    QCOMPARE(format::resetRelative(now.addSecs(3 * 24 * 3600), now),
             QStringLiteral("resets in 3d"));
    QCOMPARE(format::resetRelative(now.addSecs(24 * 3600 - 60), now),
             QStringLiteral("resets in 23h 59m"));
    QCOMPARE(format::resetRelative(now.addSecs(-60), now), QStringLiteral("resets now"));
}

void CoreTest::formatsResetPerWindowKind()
{
    const QDateTime now(QDate(2026, 9, 3), QTime(12, 0, 0), QTimeZone::UTC);

    UsagePeriod session;
    session.percentage = 63;
    session.resetAt = now.addSecs(112 * 60);

    QCOMPARE(format::resetFor(PeriodKind::FiveHour, session, now),
             QStringLiteral("resets in 1h 52m"));

    // Both windows read as a countdown, which is what Claude Code shows. Pinned
    // exactly rather than by prefix: a loose check here is what let the seven-day
    // text change without a single test noticing.
    UsagePeriod weekly;
    weekly.percentage = 41;
    weekly.resetAt = now.addSecs(3 * 24 * 3600 + 5 * 3600);
    QCOMPARE(format::resetFor(PeriodKind::SevenDay, weekly, now),
             QStringLiteral("resets in 3d 5h"));

    // No timestamp means no reset line at all, rather than an invented one.
    UsagePeriod noReset;
    noReset.percentage = 5;
    QVERIFY(format::resetFor(PeriodKind::FiveHour, noReset, now).isEmpty());
}

void CoreTest::roundsAbsoluteResetToNearestMinute()
{
    // The 7-day reset drifts sub-second between calls. Without rounding
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
    QCOMPARE(lines.at(0), QStringLiteral("ClaudeDial"));
    // 1h52m left of a five-hour window puts us 63% through it - which the
    // five-hour row reports and the seven-day row deliberately does not.
    QCOMPARE(lines.at(1), QStringLiteral("5h  63% · 63% through · resets in 1h 52m"));
    QVERIFY(lines.at(2).startsWith(QStringLiteral("7d  41% · resets in ")));
    QVERIFY(!lines.at(2).contains(QStringLiteral("through")));
}

void CoreTest::measuresPositionWithinTheFiveHourWindow()
{
    // A percentage alone does not say whether it is a lot: 63% with four hours
    // to go is heavy, 63% with forty minutes left is fine. This is the second
    // number that makes the first mean something.
    const QDateTime now(QDate(2026, 9, 4), QTime(12, 0, 0), QTimeZone::UTC);

    const auto at = [&](int minutesUntilReset) {
        UsagePeriod p;
        p.percentage = 63;
        p.resetAt = now.addSecs(minutesUntilReset * 60);
        return p;
    };

    QCOMPARE(qRound(*windowProgress(PeriodKind::FiveHour, at(300), now)), 0);   // just started
    QCOMPARE(qRound(*windowProgress(PeriodKind::FiveHour, at(120), now)), 60);
    QCOMPARE(qRound(*windowProgress(PeriodKind::FiveHour, at(0), now)), 100);   // about to reset

    QCOMPARE(format::pace(PeriodKind::FiveHour, at(120), now),
             QStringLiteral("Usage 63% · window 60%"));
}

void CoreTest::refusesToGuessTheWindowWhenItCannotKnow()
{
    const QDateTime now(QDate(2026, 9, 4), QTime(12, 0, 0), QTimeZone::UTC);

    UsagePeriod period;
    period.percentage = 63;

    // No reset time: nothing to measure against.
    QVERIFY(!windowProgress(PeriodKind::FiveHour, period, now).has_value());

    // The seven-day window never gets this. Consumption across days is
    // naturally uneven, so "40% through the week" says nothing about whether
    // 40% spent is a lot.
    period.resetAt = now.addSecs(2 * 24 * 3600);
    QVERIFY(!windowProgress(PeriodKind::SevenDay, period, now).has_value());
    QVERIFY(format::pace(PeriodKind::SevenDay, period, now).isEmpty());

    // The window length is an assumption about an undocumented endpoint. If a
    // reset time ever lands outside it, this must go quiet rather than lie.
    period.resetAt = now.addSecs(6 * 3600);
    QVERIFY(!windowProgress(PeriodKind::FiveHour, period, now).has_value());
    period.resetAt = now.addSecs(-3600);
    QVERIFY(!windowProgress(PeriodKind::FiveHour, period, now).has_value());
    QVERIFY(format::pace(PeriodKind::FiveHour, period, now).isEmpty());
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
    UsagePeriod weekly;
    weekly.percentage = 41;
    weekly.resetAt = now.addSecs(3 * 24 * 3600);
    QCOMPARE(format::resetSentence(PeriodKind::SevenDay, weekly, now),
             QStringLiteral("Resets in 3d"));

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
    for (const auto& thresholds : { std::pair { 75, 98 }, std::pair { 90, 80 } }) {
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

void CoreTest::shippedIconMatchesCanonicalGeometry()
{
    // The application icon is an SVG, so it cannot share code with the painter -
    // only numbers. It had been hand-copied once and drifted: centre 7.8% lower,
    // outer edge 5% smaller, which is what made the identity mark and the tray
    // mark stop looking like the same thing. This is the guard.
    using namespace claudedial::core::gaugeGeometry;

    QFile file(QStringLiteral(CLAUDEDIAL_SOURCE_DIR
                              "/data/icons/hicolor/scalable/apps/claudedial.svg"));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
    const QString svg = QString::fromUtf8(file.readAll());

    // A custom raw-string delimiter, because the pattern itself contains quotes.
    static const QRegularExpression arc(QStringLiteral(
        R"RE(M ([\d.]+) ([\d.]+) A ([\d.]+) [\d.]+ 0 1 1 ([\d.]+) ([\d.]+)" stroke-width="([\d.]+)")RE"));
    const auto match = arc.match(svg);
    QVERIFY2(match.hasMatch(), "the arc path is not in the shape this test can read");

    constexpr double kSide = 64.0;
    const double startX = match.captured(1).toDouble();
    const double startY = match.captured(2).toDouble();
    const double radius = match.captured(3).toDouble();
    const double endX = match.captured(4).toDouble();
    const double stroke = match.captured(6).toDouble();

    // Stroke and radius must be the canonical fractions of the icon's edge.
    QVERIFY2(qAbs(stroke / kSide - kDialThickness) < 0.005,
             qPrintable(QStringLiteral("stroke %1 vs %2").arg(stroke / kSide).arg(kDialThickness)));
    QVERIFY2(qAbs(radius / kSide - dialRadius(kDialThickness)) < 0.005,
             qPrintable(QStringLiteral("radius %1 vs %2")
                            .arg(radius / kSide).arg(dialRadius(kDialThickness))));

    // And the arc must start and end where the canonical angles put it, which is
    // what pins the centre: a shifted centre moves both endpoints together.
    const double expectedX = kSide * kCentre + radius * std::cos(qDegreesToRadians(kStartAngle));
    const double expectedY = kSide * kCentre - radius * std::sin(qDegreesToRadians(kStartAngle));
    QVERIFY2(qAbs(startX - expectedX) < 0.5,
             qPrintable(QStringLiteral("start x %1 vs %2").arg(startX).arg(expectedX)));
    QVERIFY2(qAbs(startY - expectedY) < 0.5,
             qPrintable(QStringLiteral("start y %1 vs %2").arg(startY).arg(expectedY)));
    QCOMPARE(qRound((startX + endX) / 2.0), qRound(kSide * kCentre));
}

void CoreTest::storesSettingsAtTheDocumentedPath()
{
    const Config config;
    const QString path = config.filePath();

#if defined(Q_OS_WIN)
    // NativeFormat is the registry here, so there is no extension to get wrong -
    // but the location is still worth pinning, because it is where someone goes
    // to undo a setting by hand, and because it is not written down anywhere
    // else.
    QVERIFY2(path.contains(QLatin1String("claudedial"), Qt::CaseInsensitive),
             qPrintable(path));
#elif defined(Q_OS_MACOS)
    // And a plist here: ~/Library/Preferences/com.claudedial.claudedial.plist.
    // Three formats for one setting, which is why this test exists at all.
    QVERIFY2(path.endsWith(QStringLiteral(".plist")), qPrintable(path));
    QVERIFY2(path.contains(QLatin1String("claudedial"), Qt::CaseInsensitive),
             qPrintable(path));
#else
    // The README and the packaging notes both promise
    // ~/.config/claudedial/claudedial.conf. QSettings derives the extension
    // from the format, so IniFormat would quietly produce a .ini instead and
    // every hand-written config file would be ignored.
    QVERIFY2(path.endsWith(QStringLiteral(".conf")), qPrintable(path));
    QVERIFY2(path.contains(QStringLiteral("/claudedial/claudedial.")), qPrintable(path));
#endif
}

void CoreTest::roundTripsSettings()
{
    Config config(kTestOrganisation, kTestApplication);

    config.setWarningThreshold(60);
    config.setCriticalThreshold(85);
    config.setNotificationsEnabled(false);
    config.setTrayStyle(Config::TrayStyle::Gauge);
    config.setTheme(Config::Theme::Dark);

    const Config reread(kTestOrganisation, kTestApplication);
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
    // Cleared, so an earlier test's writes cannot be mistaken for defaults -
    // and cleared in the disposable scope, never the user's own.
    QSettings(QSettings::NativeFormat, QSettings::UserScope,
              QLatin1String(kTestOrganisation), QLatin1String(kTestApplication))
        .clear();

    const Config config(kTestOrganisation, kTestApplication);
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
        Config config(kTestOrganisation, kTestApplication);
        config.setFiredThresholds(QStringLiteral("five_hour"), { 75, 90 }, reset);
    }

    const Config reread(kTestOrganisation, kTestApplication);
    auto restored = reread.firedThresholds(QStringLiteral("five_hour"));
    std::sort(restored.begin(), restored.end());
    QCOMPARE(restored, QList<int>({ 75, 90 }));
    QCOMPARE(reread.firedWindowReset(QStringLiteral("five_hour")).toUTC(), reset);

    // A window with nothing recorded must come back empty, not stale.
    QVERIFY(reread.firedThresholds(QStringLiteral("seven_day")).isEmpty());
    QVERIFY(!reread.firedWindowReset(QStringLiteral("seven_day")).isValid());
}

/// The panel colour is the one thing about a tray icon that cannot be asked for
/// at runtime, and getting it wrong made the icon invisible twice. These are the
/// real strings from the machine where that happened.
void CoreTest::readsThePlasmaPanelColour()
{
    const QString twilightDefaults = QStringLiteral(
        "[kdeglobals][General]\n"
        "ColorScheme=BreezeLight\n"
        "\n"
        "[plasmarc][Theme]\n"
        "name=breeze-dark\n");

    // No plasmarc on the machine at all, while the desktop was plainly using
    // breeze-dark - so the look-and-feel package has to be consulted, and this
    // is the case that a plain INI parser would miss: the group is
    // [plasmarc][Theme], not [Theme].
    QCOMPARE(plasmaThemeName(QString(), twilightDefaults),
             QStringLiteral("breeze-dark"));

    // An explicit choice wins over the package default.
    QCOMPARE(plasmaThemeName(QStringLiteral("[Theme]\nname=oxygen\n"), twilightDefaults),
             QStringLiteral("oxygen"));

    QVERIFY(plasmaThemeName(QString(), QString()).isEmpty());

    QCOMPARE(lookAndFeelPackage(QStringLiteral(
                 "[General]\nBrowserApplication=x.desktop\n"
                 "[KDE]\nLookAndFeelPackage=org.kde.breezetwilight.desktop\n")),
             QStringLiteral("org.kde.breezetwilight.desktop"));

    // breeze-dark's own colours. #202326, which is what a screenshot of the
    // panel measured, so this is the panel and not an approximation of it.
    const auto dark = plasmaPanelBackground(
        QStringLiteral("[Colors:Window]\nBackgroundNormal=32,35,38\n"));
    QVERIFY(dark.has_value());
    QCOMPARE(dark->r, 32);
    QVERIFY(dark->isDark());

    const auto light = plasmaPanelBackground(
        QStringLiteral("[Colors:Window]\nBackgroundNormal=239,240,241\n"));
    QVERIFY(light.has_value());
    QVERIFY(!light->isDark());

    // The stock "default" theme ships no colours at all, which is how it says
    // it follows the application colour scheme. That must read as "no answer",
    // not as black.
    QVERIFY(!plasmaPanelBackground(QString()).has_value());
    QVERIFY(!plasmaPanelBackground(
                 QStringLiteral("[Colors:Button]\nBackgroundNormal=1,2,3\n"))
                 .has_value());
    QVERIFY(!plasmaPanelBackground(
                 QStringLiteral("[Colors:Window]\nBackgroundNormal=nonsense\n"))
                 .has_value());
}

namespace {

/// A state whose five-hour window resets at a chosen distance from now.
UsageState stateResettingIn(const QDateTime& now, int seconds)
{
    UsagePeriod period;
    period.percentage = 40;
    period.resetAt = now.addSecs(seconds);
    UsageState state;
    state.fiveHour = period;
    state.updatedAt = now;
    return state;
}

} // namespace

/// The ladder only ever runs after something has gone wrong, so in production
/// it is exercised for the first time on the day it matters most. These are the
/// real numbers, read from the same function the service calls.
void CoreTest::climbsTheRateLimitLadder()
{
    QCOMPARE(rateLimitBackoffMinutes(0), 0);
    QCOMPARE(rateLimitBackoffMinutes(1), 3);
    QCOMPARE(rateLimitBackoffMinutes(2), 6);
    QCOMPARE(rateLimitBackoffMinutes(3), 12);
    QCOMPARE(rateLimitBackoffMinutes(4), 15);

    // Capped, not continued. A ladder that kept doubling would eventually stop
    // polling for a day.
    QCOMPARE(rateLimitBackoffMinutes(5), 15);
    QCOMPARE(rateLimitBackoffMinutes(50), 15);

    const QDateTime now(QDate(2026, 9, 5), QTime(12, 0, 0), QTimeZone::UTC);
    const UsageState none;

    // At the shortest interval the app allows, every rung is longer and shows.
    QCOMPARE(nextRefreshMs(60, 1, 0, none, now), 3 * 60 * 1000);
    QCOMPARE(nextRefreshMs(60, 3, 0, none, now), 12 * 60 * 1000);

    // The ladder never shortens the wait: a user who asked for an hour gets an
    // hour after a refusal, not three minutes.
    QCOMPARE(nextRefreshMs(3600, 1, 0, none, now), 3600 * 1000);

    // Worth knowing rather than fixing: at the default five minutes the first
    // rung is shorter than the interval, so the ladder does nothing until the
    // second strike. The first 429 costs no extra delay at all for most users.
    QCOMPARE(nextRefreshMs(300, 1, 0, none, now), 300 * 1000);
    QCOMPARE(nextRefreshMs(300, 2, 0, none, now), 6 * 60 * 1000);
}

void CoreTest::neverPollsSoonerThanTheServerAsked()
{
    const QDateTime now(QDate(2026, 9, 5), QTime(12, 0, 0), QTimeZone::UTC);
    const UsageState none;

    // Retry-After beats our guess when it is longer - it is the one number in
    // this calculation that is not a guess.
    QCOMPARE(nextRefreshMs(60, 1, 900, none, now), 900 * 1000);

    // And loses when it is shorter, because the ladder is there for a server
    // that keeps saying no.
    QCOMPARE(nextRefreshMs(60, 3, 60, none, now), 12 * 60 * 1000);
}

void CoreTest::alignsToAResetOnlyWhenNotRateLimited()
{
    const QDateTime now(QDate(2026, 9, 5), QTime(12, 0, 0), QTimeZone::UTC);

    // A reset sooner than the interval pulls the next poll forward to just
    // after it, so a new window is not reported a full interval late.
    QCOMPARE(nextRefreshMs(300, 0, 0, stateResettingIn(now, 60), now), 65 * 1000);

    // A reset further out than the interval changes nothing.
    QCOMPARE(nextRefreshMs(300, 0, 0, stateResettingIn(now, 3600), now), 300 * 1000);

    // A reset already past is not a reason to poll immediately.
    QCOMPARE(nextRefreshMs(300, 0, 0, stateResettingIn(now, -60), now), 300 * 1000);

    // And while rate-limited the alignment is skipped entirely: polling on the
    // stroke of a new window is the burst a rate limiter is objecting to.
    QCOMPARE(nextRefreshMs(60, 1, 0, stateResettingIn(now, 60), now), 3 * 60 * 1000);
}

/// The menu carries the readings because AppIndicator has no tooltips at all,
/// so on GNOME this is the only place they appear without opening the popup.
void CoreTest::buildsMenuEntries()
{
    const QDateTime now(QDate(2026, 9, 5), QTime(12, 0, 0), QTimeZone::UTC);

    UsageState state;
    UsagePeriod session;
    session.percentage = 63;
    session.resetAt = now.addSecs(2 * 3600);
    state.fiveHour = session;
    state.updatedAt = now;

    QCOMPARE(format::menuEntry(PeriodKind::FiveHour, state, now),
             QStringLiteral("Session 63% · resets in 2h"));

    // The same words as the popup and the notifications, so nobody has to match
    // up two namings of one window.
    QVERIFY(format::menuEntry(PeriodKind::FiveHour, state, now).startsWith(QLatin1String("Session")));

    // A window with no data yields nothing, so the caller leaves the entry out
    // rather than showing a placeholder.
    QVERIFY(format::menuEntry(PeriodKind::SevenDay, state, now).isEmpty());

    // And a window with no reset timestamp still reports its percentage.
    UsagePeriod weekly;
    weekly.percentage = 41;
    state.sevenDay = weekly;
    QCOMPARE(format::menuEntry(PeriodKind::SevenDay, state, now), QStringLiteral("Weekly 41%"));
}

namespace {

/// Writes a credential file shaped exactly like Claude Code's own.
void writeCredentials(const QString& path, qint64 expiresAt, qint64 refreshExpiresAt,
                      const QString& token = QStringLiteral("sk-ant-oat01-test"))
{
    QJsonObject oauth;
    oauth.insert(QStringLiteral("accessToken"), token);
    oauth.insert(QStringLiteral("expiresAt"), static_cast<double>(expiresAt));
    oauth.insert(QStringLiteral("refreshTokenExpiresAt"), static_cast<double>(refreshExpiresAt));
    QJsonObject root;
    root.insert(QStringLiteral("claudeAiOauth"), oauth);

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(root).toJson());
}

} // namespace

void CoreTest::writesTheAutostartEntryWhereTheDesktopReadsIt()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // Both write outside anything QStandardPaths test mode redirects - the real
    // registry, the real ~/Library/LaunchAgents - so exercising them here would
    // change the machine running the tests. Left to the platform.
    QSKIP("autostart writes outside the test sandbox on this platform");
#else
    // The path is asserted rather than round-tripped through isEnabled(): GNOME
    // and KDE read exactly this location, so a change of mind about it is a
    // change to the contract, not an implementation detail.
    const QString expected = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/claudedial.desktop");
    QCOMPARE(autostart::location(), expected);

    QFile::remove(expected);
    QVERIFY(!autostart::isEnabled());

    QVERIFY(autostart::setEnabled(true));
    QVERIFY(autostart::isEnabled());
    QVERIFY(QFile::exists(expected));

    QFile entry(expected);
    QVERIFY(entry.open(QIODevice::ReadOnly));
    const QString written = QString::fromUtf8(entry.readAll());
    entry.close();

    // A .desktop file without Type= is ignored in silence, which is the worst
    // possible failure for a toggle that claims to have worked.
    QVERIFY(written.startsWith(QLatin1String("[Desktop Entry]")));
    QVERIFY(written.contains(QLatin1String("Type=Application")));
    QVERIFY(written.contains(QLatin1String("Exec=claudedial")));

    // Turning it off removes the entry, and doing so twice is still a success.
    QVERIFY(autostart::setEnabled(false));
    QVERIFY(!autostart::isEnabled());
    QVERIFY(autostart::setEnabled(false));

    // Config reports what is on disk rather than a remembered flag.
    Config config(QStringLiteral("claudedial-test"), QStringLiteral("autostart"));
    QVERIFY(!config.startOnLogin());
    config.setStartOnLogin(true);
    QVERIFY(config.startOnLogin());
    config.setStartOnLogin(false);
    QVERIFY(!config.startOnLogin());
#endif
}

void CoreTest::classifiesTheCredentialFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qunsetenv("CLAUDE_CODE_OAUTH_TOKEN");
    qputenv("CLAUDE_CONFIG_DIR", dir.path().toUtf8());
    const QString path = dir.filePath(QStringLiteral(".credentials.json"));

    Credentials credentials;

    // Nothing there at all: not an error, just no subscription credentials.
    QCOMPARE(credentials.reload(), Credentials::Status::Missing);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 hour = 60 * 60 * 1000;

    writeCredentials(path, now + hour, now + 30 * 24 * hour);
    QCOMPARE(credentials.reload(), Credentials::Status::Ok);
    QCOMPARE(credentials.sourceDescription(), path);

    // A file on disk is watched, so nobody needs to poll it.
    QVERIFY(credentials.watchesForChanges());

    // Expired access token, live refresh token: Claude Code will fix this itself.
    writeCredentials(path, now - hour, now + 30 * 24 * hour);
    QCOMPARE(credentials.reload(), Credentials::Status::Expired);

    // Claude Code treats a token as expired a minute early, and so do we.
    writeCredentials(path, now + 30 * 1000, now + 30 * 24 * hour);
    QCOMPARE(credentials.reload(), Credentials::Status::Expired);

    // Refresh token gone too: only the user can fix this.
    writeCredentials(path, now - hour, now - hour);
    QCOMPARE(credentials.reload(), Credentials::Status::RefreshExpired);

    // An API-key or Bedrock setup writes a file with no OAuth block in it.
    QFile other(path);
    QVERIFY(other.open(QIODevice::WriteOnly | QIODevice::Truncate));
    other.write("{\"apiKey\":\"nope\"}");
    other.close();
    QCOMPARE(credentials.reload(), Credentials::Status::Missing);

    // And a half-written file must not be mistaken for anything.
    QVERIFY(other.open(QIODevice::WriteOnly | QIODevice::Truncate));
    other.write("{\"claudeAiOauth\": {\"accessTok");
    other.close();
    QCOMPARE(credentials.reload(), Credentials::Status::Missing);

    qunsetenv("CLAUDE_CONFIG_DIR");
}

void CoreTest::prefersAnExplicitTokenFromTheEnvironment()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral(".credentials.json"));
    qputenv("CLAUDE_CONFIG_DIR", dir.path().toUtf8());

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    writeCredentials(path, now - 1000, now - 1000, QStringLiteral("from-the-file"));

    // The environment wins even over a file, and carries no expiry of its own.
    qputenv("CLAUDE_CODE_OAUTH_TOKEN", "sk-ant-oat01-from-the-environment");
    Credentials credentials;
    QCOMPARE(credentials.reload(), Credentials::Status::Ok);
    QVERIFY(credentials.sourceDescription().contains(QLatin1String("CLAUDE_CODE_OAUTH_TOKEN")));

    QNetworkRequest request { QUrl(QStringLiteral("https://example.invalid/")) };
    QVERIFY(credentials.authorize(request));
    QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer sk-ant-oat01-from-the-environment"));

    qunsetenv("CLAUDE_CODE_OAUTH_TOKEN");
    qunsetenv("CLAUDE_CONFIG_DIR");
}

void CoreTest::neverNamesTheTokenItHolds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qunsetenv("CLAUDE_CODE_OAUTH_TOKEN");
    qputenv("CLAUDE_CONFIG_DIR", dir.path().toUtf8());

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString secret = QStringLiteral("sk-ant-oat01-do-not-leak-me");
    writeCredentials(dir.filePath(QStringLiteral(".credentials.json")),
                     now + 60 * 60 * 1000, now + 60 * 60 * 1000, secret);

    Credentials credentials;
    QCOMPARE(credentials.reload(), Credentials::Status::Ok);

    // sourceDescription() is the one thing about the token that reaches the UI,
    // the logs and the crash reports. It must never be the token.
    QVERIFY(!credentials.sourceDescription().contains(secret));

    // An unusable token authorizes nothing, so no stale header can leak either.
    writeCredentials(dir.filePath(QStringLiteral(".credentials.json")), now - 1, now - 1, secret);
    QCOMPARE(credentials.reload(), Credentials::Status::RefreshExpired);
    QNetworkRequest request { QUrl(QStringLiteral("https://example.invalid/")) };
    QVERIFY(!credentials.authorize(request));
    QVERIFY(!request.hasRawHeader("Authorization"));

    qunsetenv("CLAUDE_CONFIG_DIR");
}

QTEST_GUILESS_MAIN(CoreTest)
#include "tst_core.moc"
