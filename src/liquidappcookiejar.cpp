#include "liquidappcookiejar.hpp"
#include "liquidappwindow.hpp"

LiquidAppCookieJar::LiquidAppCookieJar(QObject *parent) : QNetworkCookieJar(parent)
{
    liquidAppSettings = ((LiquidAppWindow*)parent)->liquidAppSettings;

    QList<QNetworkCookie> jar;

    liquidAppSettings->beginGroup("Cookies");
    foreach(QString cookieName, liquidAppSettings->allKeys()) {
        QByteArray rawCookie = liquidAppSettings->value(cookieName).toByteArray();
        jar.append(QNetworkCookie::parseCookies(rawCookie));
    }
    liquidAppSettings->endGroup();

    setAllCookies(jar);
}

LiquidAppCookieJar::~LiquidAppCookieJar()
{
}

bool LiquidAppCookieJar::insertCookie(const QNetworkCookie &cookie)
{
    bool inserted = QNetworkCookieJar::insertCookie(cookie);

    if (inserted) {
        save();
    }

    return inserted;
}

bool LiquidAppCookieJar::updateCookie(const QNetworkCookie &cookie)
{
    bool updated = QNetworkCookieJar::updateCookie(cookie);

    if (updated) {
        save();
    }

    return updated;
}

bool LiquidAppCookieJar::deleteCookie(const QNetworkCookie &cookie)
{
    bool deleted = QNetworkCookieJar::deleteCookie(cookie);

    if (deleted) {
        save();
    }

    return deleted;
}

void LiquidAppCookieJar::save()
{
    liquidAppSettings->beginGroup("Cookies");
    foreach(QString cookieName, liquidAppSettings->allKeys()) {
        liquidAppSettings->remove(cookieName);
    }
    foreach(QNetworkCookie cookie, allCookies()) {
        QByteArray cookieId = QByteArray(cookie.domain().toLatin1() +
                                         "_" + cookie.path().toLatin1() +
                                         "_" + cookie.name());
        QString rawCookie = QString(cookie.toRawForm(QNetworkCookie::Full));
        liquidAppSettings->setValue(cookieId, rawCookie);
    }
    liquidAppSettings->endGroup();
}
