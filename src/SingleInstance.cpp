#include "SingleInstance.h"

#include <QLocalServer>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QLocalSocket>

namespace claudometer {
namespace {

/// Per user *and* per graphical session.
///
/// QLocalServer puts its socket in XDG_RUNTIME_DIR, which is per user but not
/// per session. Without the display in the name, a user logged into two
/// graphical sessions at once would share one lock, and the second session's
/// launch would poke the first session's popup instead of starting a tray icon
/// of its own.
QString socketName()
{
    const auto env = QProcessEnvironment::systemEnvironment();
    QString session = env.value(QStringLiteral("WAYLAND_DISPLAY"));
    if (session.isEmpty())
        session = env.value(QStringLiteral("DISPLAY"));
    if (session.isEmpty())
        return QStringLiteral("claudometer");

    // A display name may hold characters a socket name cannot.
    static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9_-]"));
    session.replace(unsafe, QStringLiteral("_"));
    return QStringLiteral("claudometer-%1").arg(session);
}

constexpr int kConnectTimeoutMs = 300;
constexpr int kReadTimeoutMs = 1000;

/// The two requests one instance may make of another.
constexpr auto kActivate = "activate\n";
constexpr auto kStatus = "status\n";

} // namespace

SingleInstance::SingleInstance(QObject* parent)
    : QObject(parent)
{
}

void SingleInstance::setStatusProvider(std::function<QByteArray()> provider)
{
    m_statusProvider = std::move(provider);
}

std::optional<QByteArray> SingleInstance::queryStatus()
{
    QLocalSocket socket;
    socket.connectToServer(socketName());
    if (!socket.waitForConnected(kConnectTimeoutMs))
        return std::nullopt; // nothing running; the caller falls back to the API

    socket.write(QByteArray(kStatus));
    if (!socket.waitForBytesWritten(kConnectTimeoutMs))
        return std::nullopt;

    // The server writes its answer and closes, so read until it does.
    QByteArray payload;
    while (socket.state() == QLocalSocket::ConnectedState) {
        if (!socket.waitForReadyRead(kReadTimeoutMs))
            break;
        payload.append(socket.readAll());
    }
    payload.append(socket.readAll());

    if (payload.isEmpty())
        return std::nullopt;
    return payload;
}

bool SingleInstance::acquire()
{
    // Is someone already listening?
    QLocalSocket probe;
    probe.connectToServer(socketName());
    if (probe.waitForConnected(kConnectTimeoutMs)) {
        probe.write(QByteArray(kActivate));
        probe.waitForBytesWritten(kConnectTimeoutMs);
        probe.disconnectFromServer();
        return false;
    }

    // Nobody answered. A crash can leave a stale socket file behind, so clear
    // it before claiming the name.
    QLocalServer::removeServer(socketName());

    m_server = new QLocalServer(this);
    if (!m_server->listen(socketName())) {
        // Could not listen at all. Running unguarded beats not running.
        delete m_server;
        m_server = nullptr;
        return true;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* connection = m_server->nextPendingConnection()) {
            connect(connection, &QLocalSocket::disconnected, connection, &QObject::deleteLater);
            connect(connection, &QLocalSocket::readyRead, this, [this, connection] {
                const QByteArray request = connection->readLine().trimmed();
                if (request == QByteArray(kStatus).trimmed()) {
                    if (m_statusProvider)
                        connection->write(m_statusProvider());
                    connection->flush();
                } else {
                    Q_EMIT activationRequested();
                }
                connection->disconnectFromServer();
            });
        }
    });

    return true;
}

} // namespace claudometer
