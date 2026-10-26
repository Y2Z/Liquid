#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

// Ensures that only one instance (of the main window, or of each Liquid app) runs at a time,
// and lets other processes talk to the one that's running, using local sockets (named pipes on Windows).
// Nothing polls anything: the running instance only wakes up when another process connects to it.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString& instanceId, QObject* parent = Q_NULLPTR);

    // Attempts to become the running instance; returns false if another one is already running
    bool claim(void);

    // Delivers a message to the running instance; returns false if there's none running
    static bool sendMessage(const QString& instanceId, const QByteArray& message);

    // Tells whether an instance with the given ID is currently running
    static bool isRunning(const QString& instanceId);

    // Name of the local socket (named pipe on Windows) the instance with the given ID listens on
    static QString serverName(const QString& instanceId);

signals:
    void messageReceived(const QByteArray& message);
    // The same, along with the connection it came through (for replying, or for keeping on sending things over it)
    void messageReceivedFrom(QLocalSocket* connection, const QByteArray& message);

private:
    QString instanceId;
    QLocalServer* server = Q_NULLPTR;
};
