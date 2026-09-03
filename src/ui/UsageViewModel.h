#pragma once

#include <QObject>
#include <QString>

class QTimer;

namespace claudometer::core {
class Config;
class UsageService;
}

namespace claudometer::ui {

/// The only thing QML ever sees.
///
/// Everything here is a display value: numbers, formatted strings and a level
/// name. Nothing from core::Credentials reaches this class, so there is no path
/// by which a token could end up in a Q_PROPERTY and therefore in the QML
/// debugger.
class UsageViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY changed)
    Q_PROPERTY(bool stale READ stale NOTIFY changed)
    Q_PROPERTY(bool fetching READ fetching NOTIFY changed)
    Q_PROPERTY(QString updatedText READ updatedText NOTIFY changed)

    Q_PROPERTY(bool fiveHourAvailable READ fiveHourAvailable NOTIFY changed)
    Q_PROPERTY(int fiveHourPercent READ fiveHourPercent NOTIFY changed)
    Q_PROPERTY(QString fiveHourReset READ fiveHourReset NOTIFY changed)
    Q_PROPERTY(QString fiveHourLevel READ fiveHourLevel NOTIFY changed)

    Q_PROPERTY(bool sevenDayAvailable READ sevenDayAvailable NOTIFY changed)
    Q_PROPERTY(int sevenDayPercent READ sevenDayPercent NOTIFY changed)
    Q_PROPERTY(QString sevenDayReset READ sevenDayReset NOTIFY changed)
    Q_PROPERTY(QString sevenDayLevel READ sevenDayLevel NOTIFY changed)

public:
    UsageViewModel(core::UsageService* service, core::Config* config, QObject* parent = nullptr);

    [[nodiscard]] bool available() const;
    [[nodiscard]] QString unavailableReason() const;
    [[nodiscard]] bool stale() const;
    [[nodiscard]] bool fetching() const;
    [[nodiscard]] QString updatedText() const;

    [[nodiscard]] bool fiveHourAvailable() const;
    [[nodiscard]] int fiveHourPercent() const;
    [[nodiscard]] QString fiveHourReset() const;
    [[nodiscard]] QString fiveHourLevel() const;

    [[nodiscard]] bool sevenDayAvailable() const;
    [[nodiscard]] int sevenDayPercent() const;
    [[nodiscard]] QString sevenDayReset() const;
    [[nodiscard]] QString sevenDayLevel() const;

public Q_SLOTS:
    void refresh();
    void openSettings();
    void close();

Q_SIGNALS:
    void changed();
    void settingsRequested();
    void closeRequested();

private:
    core::UsageService* m_service;
    core::Config* m_config;

    /// Relative times ("resets in 1h 52m", "Updated 2 minutes ago") go stale on
    /// their own, so the text is re-emitted on a timer without refetching.
    QTimer* m_tick;
};

} // namespace claudometer::ui
