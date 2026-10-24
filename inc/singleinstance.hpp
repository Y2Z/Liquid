#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QLocalServer;

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

signals:
    void messageReceived(const QByteArray& message);

private:
    static QString serverName(const QString& instanceId);

    QString instanceId;
    QLocalServer* server = Q_NULLPTR;
};
