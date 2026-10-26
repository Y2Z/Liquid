#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

#include "liquidapplocalstorage.hpp"
#include "lqd.h"

// Returns { origin, items, sessionItems } (items of local and session storage),
// or null if the page has no storage (e.g. it's not from a website)
static const char* const CAPTURE_SCRIPT =
    "(function() {"
        "try {"
            "const read = function(storage) {"
                "const items = {};"
                "for (let i = 0; i < storage.length; i++) {"
                    "const key = storage.key(i);"
                    "items[key] = storage.getItem(key);"
                "}"
                "return items;"
            "};"
            "return JSON.stringify({ origin: location.origin, items: read(window.localStorage), sessionItems: read(window.sessionStorage) });"
        "} catch (e) {"
            "return null;"
        "}"
    "})()";

LiquidAppLocalStorage::LiquidAppLocalStorage(QWebEnginePage* page, QSettings* liquidAppConfig, QObject* parent)
    : QObject(parent), page(page), liquidAppConfig(liquidAppConfig)
{
    captureTimer = new QTimer(this);
    captureTimer->setInterval(LQD_LOCAL_STORAGE_CAPTURE_INTERVAL);
    connect(captureTimer, &QTimer::timeout, this, &LiquidAppLocalStorage::capture);

    // Whatever is in the page's local storage once it's loaded is what the website intends to keep there
    connect(page, &QWebEnginePage::loadFinished, this, [this](const bool ok) {
        if (ok && isEnabledBool) {
            onOriginVisited(getOrigin(this->page->url()));
        }
        if (ok) {
            capture();
        }
    });

    reloadConfig();
}

void LiquidAppLocalStorage::reloadConfig(void)
{
    isEnabledBool = liquidAppConfig->value(LQD_CFG_KEY_NAME_REMEMBER_LOCAL_STORAGE, false).toBool();

    if (!isEnabledBool) {
        // Local storage keeps working, it just won't be remembered after quitting
        updateCaptureTimer();
        items = QJsonObject();
        liveOrigins.clear();
        editedOrigins.clear();
        updateRestoreScript();
        return;
    }

    // Items may have been edited in settings: those of the origins that have changed get put in place right away
    // (if it's the page that's currently shown), or whenever their pages are visited next
    const QJsonObject savedItems = load(liquidAppConfig);
    const QString currentOrigin = getOrigin(page->url());
    QStringList origins = savedItems.keys() + items.keys();
    origins.removeDuplicates();

    for (const QString& origin : origins) {
        if (savedItems.value(origin) != items.value(origin)) {
            QJsonObject editedItems;
            editedItems.insert(origin, savedItems.value(origin).toObject());

            if (origin == currentOrigin) {
                page->runJavaScript(getRestoreScript(QJsonObject(), editedItems), QWebEngineScript::ApplicationWorld);
            } else {
                // Visited during this run or not, its local storage has to become what's been saved, once it's visited next
                liveOrigins.remove(origin);
                editedOrigins.insert(origin);
            }
        }
    }

    items = savedItems;
    updateRestoreScript();
    updateCaptureTimer();
}

void LiquidAppLocalStorage::updateCaptureTimer(void)
{
    // Session storage is never remembered, it only gets read while settings are open to show it
    const bool isNeeded = isEnabledBool || isWatchingSessionStorageBool;

    if (isNeeded && !captureTimer->isActive()) {
        captureTimer->start();
    } else if (!isNeeded) {
        captureTimer->stop();
    }
}

void LiquidAppLocalStorage::setWatchingSessionStorage(const bool watching)
{
    isWatchingSessionStorageBool = watching;
    updateCaptureTimer();

    if (watching) {
        capture();
    }
}

QString LiquidAppLocalStorage::getSessionStorageOrigin(void) const
{
    return sessionStorageOrigin;
}

QJsonObject LiquidAppLocalStorage::getSessionStorageItems(void) const
{
    return sessionStorageItems;
}

void LiquidAppLocalStorage::removeSessionStorageItems(const QString& origin, const QStringList& keys)
{
    // Session storage can only be reached from a page of its origin (and this is the only tab there is)
    const QString script = QString(
        "(function(origin, keys) {"
            "if (location.origin !== origin) return;"
            "try {"
                "for (const key of keys) {"
                    "window.sessionStorage.removeItem(key);"
                "}"
            "} catch (e) {}"
        "})(%1, %2)"
    ).arg(QString::fromUtf8(QJsonDocument(QJsonArray() << origin).toJson(QJsonDocument::Compact)).mid(1).chopped(1),
          QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(keys)).toJson(QJsonDocument::Compact)));

    page->runJavaScript(script, QWebEngineScript::ApplicationWorld);
    capture();
}

bool LiquidAppLocalStorage::isEnabled(void) const
{
    return isEnabledBool;
}

QString LiquidAppLocalStorage::getCaptureScript(void)
{
    return QString::fromLatin1(CAPTURE_SCRIPT);
}

void LiquidAppLocalStorage::capture(void)
{
    if (!isEnabledBool && !isWatchingSessionStorageBool) {
        return;
    }

    QPointer<LiquidAppLocalStorage> self(this);

    page->runJavaScript(getCaptureScript(), QWebEngineScript::ApplicationWorld, [self](const QVariant& result) {
        if (self) {
            self->onCaptured(result.toString());
        }
    });
}

void LiquidAppLocalStorage::onCaptured(const QString& json)
{
    const QJsonObject result = QJsonDocument::fromJson(json.toUtf8()).object();
    const QString origin = result.value("origin").toString().toLower();
    const QJsonObject originItems = result.value("items").toObject();

    // Session storage (only to be shown in settings, never stored)
    const QString newSessionStorageOrigin = (origin == "null") ? QString() : origin;
    const QJsonObject newSessionStorageItems = result.value("sessionItems").toObject();
    if (newSessionStorageOrigin != sessionStorageOrigin || newSessionStorageItems != sessionStorageItems) {
        sessionStorageOrigin = newSessionStorageOrigin;
        sessionStorageItems = newSessionStorageItems;
        emit sessionStorageChanged();
    }

    if (!isEnabledBool || json.isEmpty()) {
        return;
    }

    // Pages that aren't from websites (e.g. "about:blank") have no origin of their own
    if (origin.isEmpty() || origin == "null") {
        return;
    }

    markOriginAsLive(origin);

    // Items of an origin edited in settings may not have been put in place yet (e.g. the page was still loading)
    if (editedOrigins.contains(origin)) {
        return;
    }

    if (items.value(origin).toObject() == originItems) {
        return;
    }

    if (originItems.isEmpty()) {
        items.remove(origin);
    } else {
        items.insert(origin, originItems);
    }

    save(liquidAppConfig, items);
}

void LiquidAppLocalStorage::markOriginAsLive(const QString& origin)
{
    if (origin.isEmpty() || liveOrigins.contains(origin)) {
        return;
    }

    liveOrigins.insert(origin);

    if (items.contains(origin) || editedOrigins.contains(origin)) {
        updateRestoreScript();
    }
}

void LiquidAppLocalStorage::onOriginVisited(const QString& origin)
{
    // The page has just loaded, so whatever's been edited in settings for its origin is in place by now
    if (editedOrigins.remove(origin)) {
        markOriginAsLive(origin);
        updateRestoreScript();
    }
}

QString LiquidAppLocalStorage::getOrigin(const QUrl& url)
{
    // Same as what location.origin is in the page
    const QString scheme = url.scheme().toLower();
    QString host = url.host(QUrl::FullyEncoded).toLower();

    if (host.isEmpty() || (scheme != "http" && scheme != "https")) {
        return QString();
    }

    if (host.contains(':')) {
        host = "[" + host + "]"; // IPv6
    }

    const int port = url.port();
    const bool isDefaultPort = port < 0 || (scheme == "http" && port == 80) || (scheme == "https" && port == 443);

    return scheme + "://" + host + ((isDefaultPort) ? QString() : ":" + QString::number(port));
}

QString LiquidAppLocalStorage::getRestoreScript(const QJsonObject& savedItems, const QJsonObject& editedItems)
{
    // Saved items get put in place only if the local storage is empty (as it is when the origin gets visited for
    // the first time in this run), so that the second page of a website loaded right after the first one
    // (e.g. upon a redirect) doesn't lose what the first one has written there;
    // items edited in settings replace whatever is there
    return QString(
        "(function(saved, edited) {"
            "try {"
                "const storage = window.localStorage;"
                "let items = edited[location.origin];"
                "if (!items) {"
                    "items = saved[location.origin];"
                    "if (!items || storage.length > 0) return;"
                "}"
                "storage.clear();"
                "for (const key of Object.keys(items)) {"
                    "storage.setItem(key, items[key]);"
                "}"
            "} catch (e) {}"
        "})(%1, %2)"
    ).arg(QString::fromUtf8(QJsonDocument(savedItems).toJson(QJsonDocument::Compact)),
          QString::fromUtf8(QJsonDocument(editedItems).toJson(QJsonDocument::Compact)));
}

void LiquidAppLocalStorage::updateRestoreScript(void)
{
    QWebEngineScriptCollection& scripts = page->scripts();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QList<QWebEngineScript> existingScripts = scripts.find(LQD_SCRIPT_NAME_LOCAL_STORAGE);
#else
    const QList<QWebEngineScript> existingScripts = scripts.findScripts(LQD_SCRIPT_NAME_LOCAL_STORAGE);
#endif
    for (const QWebEngineScript& existingScript : existingScripts) {
        scripts.remove(existingScript);
    }

    // Only origins not yet visited during this run get their items restored
    QJsonObject savedItems;
    QJsonObject editedItems;
    for (QJsonObject::const_iterator it = items.constBegin(); it != items.constEnd(); ++it) {
        if (!liveOrigins.contains(it.key()) && !editedOrigins.contains(it.key())) {
            savedItems.insert(it.key(), it.value());
        }
    }
    for (const QString& origin : editedOrigins) {
        editedItems.insert(origin, items.value(origin).toObject()); // An empty object empties the local storage
    }

    if (!isEnabledBool || (savedItems.isEmpty() && editedItems.isEmpty())) {
        return;
    }

    // Runs before any of the website's own code does
    QWebEngineScript script;
    script.setName(LQD_SCRIPT_NAME_LOCAL_STORAGE);
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setRunsOnSubFrames(false);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    script.setSourceCode(getRestoreScript(savedItems, editedItems));
    scripts.insert(script);
}

QJsonObject LiquidAppLocalStorage::load(QSettings* liquidAppConfig)
{
    return QJsonDocument::fromJson(liquidAppConfig->value(LQD_CFG_KEY_NAME_LOCAL_STORAGE).toString().toUtf8()).object();
}

void LiquidAppLocalStorage::save(QSettings* liquidAppConfig, const QJsonObject& items)
{
    if (items.isEmpty()) {
        liquidAppConfig->remove(LQD_CFG_KEY_NAME_LOCAL_STORAGE);
    } else {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_LOCAL_STORAGE,
                                  QString::fromUtf8(QJsonDocument(items).toJson(QJsonDocument::Compact)));
    }

    liquidAppConfig->sync();
}
