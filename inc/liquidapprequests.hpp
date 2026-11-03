#pragma once

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class QLocalSocket;
class QTimer;
class QUrl;
class QWebEngineProfile;

// Keeps track of where the Liquid app's pages make requests to, for settings to show it (it never gets stored anywhere):
// it's what tells which domains a Liquid app needs to be allowed to use for websites to work in it.
// Also what blocks requests to domains it isn't allowed to use (scripts, styles, images, frames, etc).
class LiquidAppRequests : public QObject
{
public:
    LiquidAppRequests(QWebEngineProfile* profile, QObject* parent = Q_NULLPTR);
    ~LiquidAppRequests(void);

    // Method is e.g. "GET", or what Liquid did instead of letting it happen (e.g. "BLOCKED")
    void record(const QUrl& url, const QString& method);

    // Requests to any other hosts get blocked (none do until it's set)
    void setAllowedDomains(const QStringList& domains);
    // Safe to call from any thread
    bool isAllowed(const QUrl& url);

    // Keeps sending what's known over the connection (as it changes), until it closes
    void watch(QLocalSocket* connection);

private:
    struct Host {
        int count = 0;
        // Number of times each request has been made, by "METHOD URL"
        QHash<QString, int> requests;
    };

    void send(QLocalSocket* connection) const;
    void sendToWatchers(void);

    QPointer<QWebEngineProfile> profile;
    // The interceptor may run on a thread of its own (depending on the version of Qt)
    QMutex allowedDomainsMutex;
    QSet<QString> allowedDomains;
    QHash<QString, Host> hosts;
    QList<QPointer<QLocalSocket>> watchers;
    QTimer* sendTimer;
};
