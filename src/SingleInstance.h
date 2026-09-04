#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QLocalServer;

namespace claudedial {

/// Keeps exactly one tray icon on screen.
///
/// Without this, launching ClaudeDial again just adds another icon to the
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

    /// Supplies what this instance answers to a `status` request. The payload is
    /// the same JSON `--json` prints: percentages and timestamps only.
    void setStatusProvider(std::function<QByteArray()> provider);

    /// Asks a running instance for its current status, so a one-shot CLI call
    /// costs no API request. nullopt when nothing is running.
    [[nodiscard]] static std::optional<QByteArray> queryStatus();

Q_SIGNALS:
    /// Another launch happened; show the popup.
    void activationRequested();

private:
    QLocalServer* m_server = nullptr;
    std::function<QByteArray()> m_statusProvider;
};

} // namespace claudedial
