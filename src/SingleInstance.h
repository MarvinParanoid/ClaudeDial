#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace claudometer {

/// Keeps exactly one tray icon on screen.
///
/// Without this, launching Claudometer again just adds another icon to the
/// panel - easy to do from a launcher, and confusing once it has happened. A
/// second launch instead asks the running instance to show its popup, which
/// doubles as the `on-click` handler for status-bar modules.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject* parent = nullptr);

    /// Becomes the primary instance, or returns false if one is already running.
    /// When false, the running instance has been asked to show itself.
    [[nodiscard]] bool acquire();

Q_SIGNALS:
    /// Another launch happened; show the popup.
    void activationRequested();

private:
    QLocalServer* m_server = nullptr;
};

} // namespace claudometer
