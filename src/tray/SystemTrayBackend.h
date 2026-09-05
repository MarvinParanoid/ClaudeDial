#pragma once

#include "TrayBackend.h"

class QAction;
class QMenu;
class QSystemTrayIcon;

namespace claudedial::tray {

/// QSystemTrayIcon-backed tray.
///
/// On a desktop with a StatusNotifierWatcher on the bus - Plasma, and GNOME with
/// the AppIndicator extension - Qt routes this over D-Bus as a
/// StatusNotifierItem automatically, which is the modern mechanism we want. Qt
/// already does this, so there is nothing to implement by hand.
class SystemTrayBackend : public TrayBackend
{
    Q_OBJECT

public:
    explicit SystemTrayBackend(QObject* parent = nullptr);
    ~SystemTrayBackend() override;

    /// False when no tray is available at all; the caller should say so and exit
    /// rather than running invisibly.
    ///
    /// Necessary but not sufficient: it can report a tray where no icon of ours
    /// can appear. Confirm with hasVisibleIcon() once the icon has been shown.
    [[nodiscard]] static bool isAvailable();

    /// Whether anything on this system could display our icon.
    ///
    /// Weaker than it sounds, on purpose. It answers "is there a mechanism"
    /// rather than "did the icon appear", because no reliable way to prove the
    /// latter exists: registration is asynchronous and hosts disagree about what
    /// they report. Trying to prove it produced a false alarm on GNOME with the
    /// AppIndicator extension, where the icon was working in the top bar.
    /// See docs/platform-support.md.
    [[nodiscard]] bool hasVisibleIcon() const override;

    void setIcon(const QIcon& icon) override;
    void setToolTip(const QString& tooltip) override;
    void setSummary(const QString& fiveHour, const QString& sevenDay) override;
    void show() override;
    void showMessage(const QString& title, const QString& body, const QImage& icon,
                     bool critical) override;
    [[nodiscard]] QRect iconGeometry() const override;

private:
    QSystemTrayIcon* m_tray;
    QMenu* m_menu;

    /// Disabled entries, not commands: they are a reading the menu displays.
    QAction* m_fiveHourEntry;
    QAction* m_sevenDayEntry;
    QAction* m_summarySeparator;
};

} // namespace claudedial::tray
