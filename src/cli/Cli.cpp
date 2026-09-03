#include "Cli.h"

#include "SingleInstance.h"

#include <optional>

#include "core/Config.h"
#include "core/Credentials.h"
#include "core/Format.h"
#include "core/UsageClient.h"
#include "core/UsageJson.h"
#include "core/UsageState.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

namespace claudometer::cli {
namespace {

using namespace claudometer::core;

QString describe(FetchError error, Credentials::Status status)
{
    switch (error) {
    case FetchError::NoCredentials:
        // Both statuses land here, but they call for different wording: one
        // means "never signed in", the other "signed in, but too long ago".
        if (status == Credentials::Status::RefreshExpired)
            return QStringLiteral("Claude Code's sign-in has expired - run Claude Code to sign in again");
        return QStringLiteral("no Claude subscription credentials found - sign in with Claude Code");
    case FetchError::TokenExpired:
        return QStringLiteral("Claude Code's token has expired - run Claude Code to refresh it");
    case FetchError::Unauthorized:
        return QStringLiteral("the server rejected the token");
    case FetchError::RateLimited:
        return QStringLiteral("rate limited - try again in a few minutes");
    case FetchError::Network:
        return QStringLiteral("could not reach api.anthropic.com");
    case FetchError::BadResponse:
        return QStringLiteral("the usage endpoint returned something unexpected");
    }
    return QStringLiteral("unknown error");
}

void printHuman(QTextStream& out, const UsageState& state)
{
    const auto line = [&out](const QString& label, PeriodKind kind,
                             const std::optional<UsagePeriod>& period) {
        if (!period) {
            out << label << "  n/a\n";
            return;
        }
        const QString reset = format::resetFor(kind, *period);
        out << QStringLiteral("%1  %2%").arg(label).arg(qRound(period->percentage));
        if (!reset.isEmpty())
            out << "  " << reset;
        out << '\n';
    };

    line(QStringLiteral("5 hour"), PeriodKind::FiveHour, state.fiveHour);
    line(QStringLiteral("7 day "), PeriodKind::SevenDay, state.sevenDay);
    out << format::updatedAgo(state.updatedAt) << '\n';
}

} // namespace

int run(int argc, char** argv, bool jsonOutput)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("claudometer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CLAUDOMETER_VERSION));

    QTextStream out(stdout);

    // A tray instance is already polling on its own timer, and the rate-limit
    // bucket is per access token - so a status bar calling this every few
    // seconds would consume it twice over. Take the running instance's answer
    // when there is one, and only reach for the network when there is not.
    // A running instance is not consulted while simulating: an explicit debug
    // override has to win, or it silently reports the real numbers instead.
    if (const auto shared = UsageClient::isSimulating() ? std::nullopt
                                                        : SingleInstance::queryStatus()) {
        if (jsonOutput) {
            out << *shared;
            out.flush();
            return 0;
        }
        if (const auto state = json::parseStatus(*shared)) {
            printHuman(out, *state);
            out.flush();
            return 0;
        }
    }

    Credentials credentials;
    Config config;
    UsageClient client(&credentials);

    credentials.reload();

    QTextStream err(stderr);
    int exitCode = 0;

    QObject::connect(&client, &UsageClient::succeeded, &app, [&](const UsageState& state) {
        if (jsonOutput)
            out << json::status(state, config.warningThreshold(), config.criticalThreshold());
        else
            printHuman(out, state);
        QCoreApplication::quit();
    });

    QObject::connect(&client, &UsageClient::failed, &app, [&](FetchError error) {
        const QString reason = describe(error, credentials.status());
        if (jsonOutput) {
            // Still valid JSON, so a status bar shows "--" rather than breaking.
            out << json::unavailable(reason);
        } else {
            err << "claudometer: " << reason << '\n';
        }
        exitCode = 1;
        QCoreApplication::quit();
    });

    QTimer::singleShot(0, &client, &UsageClient::fetch);

    const int result = QCoreApplication::exec();
    out.flush();
    err.flush();
    return result != 0 ? result : exitCode;
}

} // namespace claudometer::cli
