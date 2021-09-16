#pragma once

#include <QDateTime>
#include <QList>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QSettings>
#include <QStringList>
#include <QVariantList>
#include <QWebEngineCookieStore>

#include "liquidappwindow.hpp"

class LiquidAppCookieJar : public QNetworkCookieJar
{
    Q_OBJECT

public:
    LiquidAppCookieJar(QObject *parent);
    ~LiquidAppCookieJar();

    bool removeCookie(const QNetworkCookie &cookie);
    bool upsertCookie(const QNetworkCookie &cookie);

    void restoreCookies(QWebEngineCookieStore *cookieStore);

private:
    QSettings *liquidAppSettings;
    LiquidAppWindow *parentWindow;

    void save();
};
