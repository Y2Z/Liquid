#pragma once

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QStringList>

class QTimer;
class QUrl;
class QWebEnginePage;

// Remembers what websites keep in their local storage (window.localStorage) across runs of a Liquid app.
//
// Qt provides no access to local storage, and Liquid apps' profiles are off-the-record (nothing gets written to disk),
// so it's done via Liquid's own JS world (which works even if JavaScript is disabled for the page itself):
// whatever the page keeps there gets read every now and then (and before quitting), and stored in the .liquid file,
// and saved items get put back in place before the website's own code runs, the first time it's visited in a run.
// Only the top-level page's storage is remembered (pages embedded in frames can't be reached this way in Qt 5).
class LiquidAppLocalStorage : public QObject
{
    Q_OBJECT

public:
    LiquidAppLocalStorage(QWebEnginePage* page, QSettings* liquidAppConfig, QObject* parent = Q_NULLPTR);

    // Picks up changes made to settings (whether to remember local storage, items edited in settings)
    void reloadConfig(void);

    bool isEnabled(void) const;

    // The script that reads the page's local storage, and what to do with its result
    // (for reading it synchronously right before quitting)
    static QString getCaptureScript(void);
    void onCaptured(const QString& json);

    // Session storage of the page that's currently shown (read only while it's being watched, e.g. by settings)
    void setWatchingSessionStorage(const bool watching);
    QString getSessionStorageOrigin(void) const;
    QJsonObject getSessionStorageItems(void) const;
    void removeSessionStorageItems(const QString& origin, const QStringList& keys);

    // Stored items: { "origin": { "key": "value", ... }, ... }
    static QJsonObject load(QSettings* liquidAppConfig);
    static void save(QSettings* liquidAppConfig, const QJsonObject& items);

signals:
    void sessionStorageChanged(void);

private:
    void capture(void);
    void updateCaptureTimer(void);
    void updateRestoreScript(void);
    void markOriginAsLive(const QString& origin);
    void onOriginVisited(const QString& origin);
    static QString getOrigin(const QUrl& url);
    static QString getRestoreScript(const QJsonObject& savedItems, const QJsonObject& editedItems);

    QWebEnginePage* page;
    QSettings* liquidAppConfig;
    QTimer* captureTimer;
    bool isEnabledBool = false;
    bool isWatchingSessionStorageBool = false;
    QString sessionStorageOrigin;
    QJsonObject sessionStorageItems;

    // What's known to be in the local storage of each origin
    QJsonObject items;
    // Origins visited during this run: their local storage in the page is the up-to-date one,
    // so it never gets restored from the saved items again (e.g. after the website has removed some of them)
    QSet<QString> liveOrigins;
    // Origins whose items have been edited in settings, to be put in place once they get visited
    QSet<QString> editedOrigins;
};
