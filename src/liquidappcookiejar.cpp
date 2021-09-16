#include "config.h"
#include "liquidappcookiejar.hpp"
#include "liquidappwindow.hpp"

LiquidAppCookieJar::LiquidAppCookieJar(QObject *parent) : QNetworkCookieJar(parent)
{
    parentWindow = (LiquidAppWindow *)parent;
    liquidAppSettings = parentWindow->liquidAppSettings;
}

LiquidAppCookieJar::~LiquidAppCookieJar()
{
}

bool LiquidAppCookieJar::upsertCookie(const QNetworkCookie &cookie)
{
    if (!liquidAppSettings->value(SETTINGS_KEY_ALLOW_COOKIES).toBool()) {
        return false;
    }

    bool isThirdParty = !validateCookie(cookie, parentWindow->url());
    if (isThirdParty && !liquidAppSettings->value(SETTINGS_KEY_ALLOW_THIRD_PARTY_COOKIES).toBool()) {
        return false;
    }

    foreach(QNetworkCookie existingCookie, allCookies()) {
        if (existingCookie.hasSameIdentifier(cookie)) {
            deleteCookie(existingCookie);
        }
    }

    bool inserted = insertCookie(cookie);

    if (inserted) {
        save();
    }

    return inserted;
}

bool LiquidAppCookieJar::removeCookie(const QNetworkCookie &cookie)
{
    if (!liquidAppSettings->value(SETTINGS_KEY_ALLOW_COOKIES).toBool()) {
        return false;
    }

    bool isThirdParty = !validateCookie(cookie, parentWindow->url());
    if (isThirdParty && !liquidAppSettings->value(SETTINGS_KEY_ALLOW_THIRD_PARTY_COOKIES).toBool()) {
        return false;
    }

    bool deleted = deleteCookie(cookie);

    if (deleted) {
        save();
    }

    return deleted;
}

void LiquidAppCookieJar::restoreCookies(QWebEngineCookieStore *cookieStore) {
    if (!liquidAppSettings->value(SETTINGS_KEY_ALLOW_COOKIES).toBool()) {
        return;
    }

    liquidAppSettings->beginGroup("Cookies");
    foreach(QString cookieId, liquidAppSettings->allKeys()) {
        QByteArray rawCookie = liquidAppSettings->value(cookieId).toByteArray();
        QNetworkCookie cookie = QNetworkCookie::parseCookies(rawCookie)[0];

        // Construct origin URL based on the cookie itself
        QString scheme("http");
        if (cookie.isSecure()) {
            scheme += "s";
        }
        QString domain(cookie.domain());
        while (domain.startsWith(".")) {
            domain = domain.right(domain.size() - 1);
        }
        QUrl url(scheme + "://" + domain + cookie.path());

        // Avoid prepending leading dot (https://bugreports.qt.io/browse/QTBUG-64732)
        if (!cookie.domain().startsWith(".")) {
            cookie.setDomain("");
        }
        cookieStore->setCookie(cookie, url);
    }
    liquidAppSettings->endGroup();
}

void LiquidAppCookieJar::save()
{
    if (!liquidAppSettings->value(SETTINGS_KEY_ALLOW_COOKIES).toBool()) {
        return;
    }

    liquidAppSettings->beginGroup("Cookies");
    // Remove all cookies
    foreach(QString cookieName, liquidAppSettings->allKeys()) {
        liquidAppSettings->remove(cookieName);
    }
    // Save all cookies
    foreach(QNetworkCookie cookie, allCookies()) {
        QByteArray cookieId = QByteArray(cookie.domain().toLatin1() +
                                         "_" + cookie.path().toLatin1() +
                                         "_" + cookie.name());
        QString rawCookie = QString(cookie.toRawForm(QNetworkCookie::Full));
        liquidAppSettings->setValue(cookieId, rawCookie);
    }
    liquidAppSettings->endGroup();
    liquidAppSettings->sync();
}
