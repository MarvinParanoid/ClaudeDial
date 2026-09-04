#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

class QSettings;

namespace claudedial::core {

/// Persisted settings, via QSettings - which on Linux means
/// ~/.config/claudedial/claudedial.conf, i.e. plain XDG. No database, no
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

    /// What the tray icon puts inside the ClaudeDial arc.
    ///
    /// Two variants of one mark, not two icons: the arc is identical in both,
    /// and only its middle differs. Gauge photographs better; Percentage is the
    /// better daily driver, because it answers the question without a hover.
    enum class TrayStyle {
        Gauge,      ///< the needle - the logotype, and a reading you interpret
        Percentage, ///< the exact number
    };

    /// How light or dark the tray icon's neutral is drawn.
    ///
    /// This exists because the panel is unknowable and guessing was wrong in
    /// both directions. QPalette describes the *application* colours, which on
    /// Plasma is a setting separate from the panel's, and on a desktop with no
    /// Qt integration it is a built-in default rather than information. A fixed
    /// mid-tone was tried instead and read as washed out on light panels and on
    /// dark ones alike. So the one bit we cannot derive is asked for.
    enum class TrayTone {
        Auto,  ///< follow the application palette; assume a dark panel when unknown
        Light, ///< a light mark, for a dark panel
        Dark,  ///< a dark mark, for a light panel
    };

    explicit Config(QObject* parent = nullptr);

    /// Settings kept somewhere other than the user's own. A test seam, and the
    /// only way to have one: QStandardPaths test mode redirects files, and
    /// QSettings::NativeFormat is the registry on Windows and a plist on macOS.
    /// Without this the tests that write either skipped themselves on those
    /// platforms - leaving the registry and LaunchAgent code untested - or
    /// overwrote the settings of whoever ran them.
    Config(const QString& organisation, const QString& application,
           QObject* parent = nullptr);

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

    [[nodiscard]] TrayStyle trayStyle() const;
    void setTrayStyle(TrayStyle style);

    [[nodiscard]] TrayTone trayTone() const;
    void setTrayTone(TrayTone tone);

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
    // restart, or ClaudeDial re-announces thresholds it already announced -
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

} // namespace claudedial::core
