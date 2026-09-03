#include "SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace claudometer {
namespace {

/// Per-user, and per-session via the runtime directory QLocalServer uses.
QString socketName()
{
    return QStringLiteral("claudometer-single-instance");
}

constexpr int kConnectTimeoutMs = 300;

} // namespace

SingleInstance::SingleInstance(QObject* parent)
    : QObject(parent)
{
}

bool SingleInstance::acquire()
{
    // Is someone already listening?
    QLocalSocket probe;
    probe.connectToServer(socketName());
    if (probe.waitForConnected(kConnectTimeoutMs)) {
        probe.write(QByteArrayLiteral("activate"));
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
            Q_EMIT activationRequested();
        }
    });

    return true;
}

} // namespace claudometer
