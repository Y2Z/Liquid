#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QUrl>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>

#include "liquidapprequests.hpp"
#include "lqd.h"

namespace {

// Sees every request made by pages of the profile (including those of frames, scripts, styles, images, etc)
class RequestInterceptor : public QWebEngineUrlRequestInterceptor
{
public:
    RequestInterceptor(LiquidAppRequests* liquidAppRequests) : QWebEngineUrlRequestInterceptor(liquidAppRequests)
    {
        this->liquidAppRequests = liquidAppRequests;
    }

    void interceptRequest(QWebEngineUrlRequestInfo& info) override
    {
        const QUrl url = info.requestUrl();
        const QString method = QString::fromLatin1(info.requestMethod());

        // Navigating the page itself is up to LiquidAppWebPage::acceptNavigationRequest() (which sends links to the web browser)
        if (info.resourceType() != QWebEngineUrlRequestInfo::ResourceTypeMainFrame && !liquidAppRequests->isAllowed(url)) {
            info.block(true);
        }

        // May be called from a thread other than the main one (depending on the version of Qt)
        LiquidAppRequests* liquidAppRequests = this->liquidAppRequests;
        QMetaObject::invokeMethod(liquidAppRequests, [liquidAppRequests, url, method]() {
            liquidAppRequests->record(url, method);
        }, Qt::QueuedConnection);
    }

private:
    LiquidAppRequests* liquidAppRequests;
};

}

LiquidAppRequests::LiquidAppRequests(QWebEngineProfile* profile, QObject* parent) : QObject(parent)
{
    this->profile = profile;

    sendTimer = new QTimer(this);
    sendTimer->setSingleShot(true);
    sendTimer->setInterval(LQD_REQUESTS_SEND_INTERVAL);
    connect(sendTimer, &QTimer::timeout, this, &LiquidAppRequests::sendToWatchers);

#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    profile->setUrlRequestInterceptor(new RequestInterceptor(this));
#else
    profile->setRequestInterceptor(new RequestInterceptor(this));
#endif
}

LiquidAppRequests::~LiquidAppRequests(void)
{
    // The profile doesn't own the interceptor (which is about to be gone)
    if (!profile.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
        profile->setUrlRequestInterceptor(Q_NULLPTR);
#else
        profile->setRequestInterceptor(Q_NULLPTR);
#endif
    }
}

void LiquidAppRequests::setAllowedDomains(const QStringList& domains)
{
    QSet<QString> newAllowedDomains;
    for (const QString& domain : domains) {
        if (!domain.trimmed().isEmpty()) {
            newAllowedDomains.insert(domain.trimmed().toLower());
        }
    }

    QMutexLocker locker(&allowedDomainsMutex);
    allowedDomains = newAllowedDomains;
}

bool LiquidAppRequests::isAllowed(const QUrl& url)
{
    // Only requests made to websites (not e.g. to data: URLs, or by Liquid's own pages)
    static const QStringList schemes = { "http", "https", "ws", "wss" };
    if (!schemes.contains(url.scheme().toLower())) {
        return true;
    }

    QMutexLocker locker(&allowedDomainsMutex);

    return allowedDomains.isEmpty() || allowedDomains.contains(url.host().toLower());
}

void LiquidAppRequests::record(const QUrl& url, const QString& method)
{
    // Only requests made to websites (not e.g. to data: URLs, or by Liquid's own pages)
    static const QStringList schemes = { "http", "https", "ws", "wss" };
    const QString hostName = url.host().toLower();
    if (hostName.isEmpty() || !schemes.contains(url.scheme().toLower())) {
        return;
    }

    // Within reason (it's all kept in memory for as long as the Liquid app runs)
    if (!hosts.contains(hostName) && hosts.size() >= LQD_REQUESTS_MAX_HOSTS) {
        return;
    }

    Host& host = hosts[hostName];
    host.count++;

    const QString request = method + ' ' + url.toString(QUrl::RemoveUserInfo);
    if (host.requests.contains(request) || host.requests.size() < LQD_REQUESTS_MAX_PER_HOST) {
        host.requests[request]++;
    }

    if (!watchers.isEmpty() && !sendTimer->isActive()) {
        sendTimer->start();
    }
}

void LiquidAppRequests::watch(QLocalSocket* connection)
{
    watchers << QPointer<QLocalSocket>(connection);

    connect(connection, &QLocalSocket::disconnected, this, [this]() {
        for (int i = watchers.size() - 1; i >= 0; i--) {
            if (watchers[i].isNull() || watchers[i]->state() != QLocalSocket::ConnectedState) {
                watchers.removeAt(i);
            }
        }
    });

    // What's known so far, more comes as it happens
    send(connection);
}

void LiquidAppRequests::send(QLocalSocket* connection) const
{
    // [ { "host": "...", "count": N, "requests": [ { "count": N, "method": "...", "url": "..." }, ... ] }, ... ],
    // with only the most frequent requests of each host
    QJsonArray hostsJson;

    for (auto host = hosts.constBegin(); host != hosts.constEnd(); ++host) {
        QList<QPair<int, QString>> requests;
        for (auto request = host->requests.constBegin(); request != host->requests.constEnd(); ++request) {
            requests << qMakePair(request.value(), request.key());
        }
        std::sort(requests.begin(), requests.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
            return (a.first != b.first) ? a.first > b.first : a.second < b.second;
        });

        QJsonArray requestsJson;
        for (int i = 0; i < requests.size() && i < LQD_REQUESTS_TOP_COUNT; i++) {
            QJsonObject requestJson;
            requestJson.insert("count", requests[i].first);
            requestJson.insert("method", requests[i].second.section(' ', 0, 0));
            requestJson.insert("url", requests[i].second.section(' ', 1));
            requestsJson << requestJson;
        }

        QJsonObject hostJson;
        hostJson.insert("host", host.key());
        hostJson.insert("count", host->count);
        hostJson.insert("requests", requestsJson);
        hostsJson << hostJson;
    }

    connection->write(QByteArray(LQD_INSTANCE_MSG_REQUESTS " ") + QJsonDocument(hostsJson).toJson(QJsonDocument::Compact).toBase64() + "\n");
    connection->flush();
}

void LiquidAppRequests::sendToWatchers(void)
{
    for (const QPointer<QLocalSocket>& watcher : watchers) {
        if (!watcher.isNull() && watcher->state() == QLocalSocket::ConnectedState) {
            send(watcher.data());
        }
    }
}
