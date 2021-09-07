#pragma once

#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QSettings>
#include <QStringList>
#include <QVariantList>
#include <QDateTime>
#include <QList>

class LiquidAppCookieJar : public QNetworkCookieJar
{
    Q_OBJECT

public:
    LiquidAppCookieJar(QObject *parent);
    ~LiquidAppCookieJar();

    bool insertCookie(const QNetworkCookie &cookie);
    bool updateCookie(const QNetworkCookie &cookie);
    bool deleteCookie(const QNetworkCookie &cookie);

private:
    QSettings *liquidAppSettings;
    void save();
};
