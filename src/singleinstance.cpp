#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

#include "singleinstance.hpp"

static const int CONNECT_TIMEOUT_MS = 500;

SingleInstance::SingleInstance(const QString& instanceId, QObject* parent) : QObject(parent)
{
    this->instanceId = instanceId;
}

QString SingleInstance::serverName(const QString& instanceId)
{
    // Per-user, so that different users on the same machine don't collide.
    // Hashed, since Liquid app names may contain characters (or be of length) unsuitable for socket names.
    const QByteArray key = (QDir::home().dirName() + "\n" + instanceId).toUtf8();

    return QString(PROG_NAME "-") + QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(24);
}

bool SingleInstance::claim(void)
{
    const QString name = serverName(instanceId);

    // Is another instance listening?
    {
        QLocalSocket socket;
        socket.connectToServer(name);

        if (socket.waitForConnected(CONNECT_TIMEOUT_MS)) {
            socket.disconnectFromServer();
            return false;
        }
    }

    // This is the first instance: clear a stale socket possibly left behind by a crash, then listen
    QLocalServer::removeServer(name);

    server = new QLocalServer(this);
    server->setSocketOptions(QLocalServer::UserAccessOption);

    connect(server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* client = server->nextPendingConnection()) {
            // Messages are newline-terminated
            connect(client, &QLocalSocket::readyRead, this, [this, client]() {
                while (client->canReadLine()) {
                    const QByteArray message = client->readLine().trimmed();

                    if (!message.isEmpty()) {
                        emit messageReceived(message);
                    }
                }
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });

    if (!server->listen(name)) {
        qDebug().noquote() << "Unable to listen for other instances:" << server->errorString();
    }

    return true;
}

bool SingleInstance::sendMessage(const QString& instanceId, const QByteArray& message)
{
    QLocalSocket socket;
    socket.connectToServer(serverName(instanceId));

    if (!socket.waitForConnected(CONNECT_TIMEOUT_MS)) {
        return false;
    }

    socket.write(message + "\n");
    socket.waitForBytesWritten(CONNECT_TIMEOUT_MS);
    socket.disconnectFromServer();

    return true;
}
