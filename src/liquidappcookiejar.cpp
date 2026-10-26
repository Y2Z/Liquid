#include <QRegularExpression>

#include "lqd.h"
#include "liquidappcookiejar.hpp"
#include "liquidappwindow.hpp"

LiquidAppCookieJar::LiquidAppCookieJar(QObject *parent) : QNetworkCookieJar(parent)
{
    liquidAppWindow = (LiquidAppWindow*)parent;
    liquidAppConfig = liquidAppWindow->liquidAppConfig;
}

LiquidAppCookieJar::~LiquidAppCookieJar(void)
{
}

bool LiquidAppCookieJar::upsertCookie(const QNetworkCookie &cookie)
{
    if (!liquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_COOKIES).toBool()) {
        return false;
    }

    const bool isThirdParty = !validateCookie(cookie, liquidAppWindow->url());
    if (isThirdParty && !liquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_3RD_PARTY_COOKIES).toBool()) {
        return false;
    }

    foreach(QNetworkCookie existingCookie, allCookies()) {
        if (existingCookie.hasSameIdentifier(cookie)) {
            deleteCookie(existingCookie);
        }
    }

    const bool inserted = insertCookie(cookie);

    if (inserted) {
        save();
    }

    return inserted;
}

bool LiquidAppCookieJar::removeCookie(const QNetworkCookie &cookie)
{
    if (!liquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_COOKIES).toBool()) {
        return false;
    }

    const bool isThirdParty = !validateCookie(cookie, liquidAppWindow->url());
    if (isThirdParty && !liquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_3RD_PARTY_COOKIES).toBool()) {
        return false;
    }

    const bool deleted = deleteCookie(cookie);

    if (deleted) {
        save();
    }

    return deleted;
}

void LiquidAppCookieJar::restoreCookies(QWebEngineCookieStore *cookieStore) {
    if (liquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_COOKIES).toBool()) {
        liquidAppConfig->beginGroup(LQD_CFG_GROUP_NAME_COOKIES);
        foreach(QString cookieId, liquidAppConfig->allKeys()) {
            const QByteArray rawCookie = liquidAppConfig->value(cookieId).toByteArray();
            const QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(rawCookie);
            if (cookies.size() > 0) {
                QNetworkCookie cookie = cookies[0];

                // Construct origin URL based on cookie data
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
        }
        liquidAppConfig->endGroup();
    }
}

void LiquidAppCookieJar::save(void)
{
    if (!liquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_COOKIES).toBool()) {
        return;
    }

    liquidAppConfig->beginGroup(LQD_CFG_GROUP_NAME_COOKIES);
    // Remove all cookies
    foreach(QString cookieName, liquidAppConfig->allKeys()) {
        liquidAppConfig->remove(cookieName);
    }
    // Save all cookies
    foreach(QNetworkCookie cookie, allCookies()) {
        QString rawCookie = QString(cookie.toRawForm(QNetworkCookie::Full));
        // TODO: save Session cookies?
        liquidAppConfig->setValue(getCookieId(cookie), rawCookie);
    }
    liquidAppConfig->endGroup();
    liquidAppConfig->sync();
}

QString LiquidAppCookieJar::getCookieId(const QNetworkCookie& cookie)
{
    const QString cookieId = cookie.domain() + "_" + cookie.path() + "_" + QString::fromLatin1(cookie.name());

    // The same as it ends up being as a key in settings (where slashes separate groups, and get normalized)
    static const QRegularExpression multipleSlashes(QStringLiteral("/{2,}"));
    QString key = cookieId;
    key.replace(multipleSlashes, QStringLiteral("/"));
    while (key.startsWith('/')) {
        key.remove(0, 1);
    }
    while (key.endsWith('/')) {
        key.chop(1);
    }

    return key;
}

void LiquidAppCookieJar::removeCookiesMissingFromConfig(QWebEngineCookieStore* cookieStore)
{
    liquidAppConfig->beginGroup(LQD_CFG_GROUP_NAME_COOKIES);
    const QStringList cookieIds = liquidAppConfig->allKeys();
    liquidAppConfig->endGroup();

    // The file always lists all of the cookies this jar has (it gets rewritten upon every change),
    // so whatever's missing from it has been removed in settings; removing it from the store removes it from here too
    foreach(QNetworkCookie cookie, allCookies()) {
        if (!cookieIds.contains(getCookieId(cookie))) {
            cookieStore->deleteCookie(cookie);
        }
    }
}
