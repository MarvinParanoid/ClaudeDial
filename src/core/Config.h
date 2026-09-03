#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

class QSettings;

namespace claudometer::core {

/// Persisted settings, via QSettings - which on Linux means
/// ~/.config/claudometer/claudometer.conf, i.e. plain XDG. No database, no
/// bespoke format, nothing to migrate.
///
/// Credentials are never stored here. This file is user-readable by design.
class Config : public QObject
{
    Q_OBJECT

public:
    enum class Theme {
        System,
        Light,
        Dark,
    };

    explicit Config(QObject* parent = nullptr);

    /// Where settings live. Useful in bug reports, and asserted in tests so the
    /// documented path cannot silently drift from the real one - QSettings picks
    /// the extension from the format, which is easy to get wrong.
    [[nodiscard]] QString filePath() const;

    [[nodiscard]] int refreshIntervalSeconds() const;
    void setRefreshIntervalSeconds(int seconds);

    [[nodiscard]] bool notificationsEnabled() const;
    void setNotificationsEnabled(bool enabled);

    [[nodiscard]] int warningThreshold() const;
    void setWarningThreshold(int percent);

    [[nodiscard]] int criticalThreshold() const;
    void setCriticalThreshold(int percent);

    [[nodiscard]] bool showPercentageInTray() const;
    void setShowPercentageInTray(bool show);

    [[nodiscard]] Theme theme() const;
    void setTheme(Theme theme);

    /// Backed by the presence of an XDG autostart entry rather than a setting,
    /// so it stays true to what the desktop will actually do.
    [[nodiscard]] bool startOnLogin() const;
    void setStartOnLogin(bool enabled);

    // --- notification bookkeeping ------------------------------------------
    //
    // Not user preferences: this is state, kept under a [state] group so it is
    // obvious that editing it by hand achieves nothing. It has to survive a
    // restart, or Claudometer re-announces thresholds it already announced -
    // every login would repeat the warning for the window you are still in.

    /// Thresholds already announced for `windowKey` ("five_hour"/"seven_day").
    [[nodiscard]] QList<int> firedThresholds(const QString& windowKey) const;

    /// The window those thresholds belong to, identified by its reset time.
    [[nodiscard]] QDateTime firedWindowReset(const QString& windowKey) const;

    void setFiredThresholds(const QString& windowKey, const QList<int>& thresholds,
                            const QDateTime& windowReset);

    /// The polling floor. Never let a user configure aggressive polling: the
    /// rate-limit bucket is per access token and shared with other monitors.
    static constexpr int kMinimumRefreshSeconds = 60;
    static constexpr int kDefaultRefreshSeconds = 300;

Q_SIGNALS:
    void changed();

private:
    QSettings* m_settings;
};

} // namespace claudometer::core
