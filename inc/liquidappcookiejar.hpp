#pragma once

#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QSettings>
#include <QWebEngineCookieStore>

#include "liquidappwindow.hpp"

class LiquidAppCookieJar : public QNetworkCookieJar
{
    Q_OBJECT

public:
    LiquidAppCookieJar(QObject* parent = Q_NULLPTR);
    ~LiquidAppCookieJar(void);

    bool removeCookie(const QNetworkCookie& cookie);
    bool upsertCookie(const QNetworkCookie& cookie);

    void restoreCookies(QWebEngineCookieStore* cookieStore);

private:
    QSettings* liquidAppConfig;
    LiquidAppWindow* liquidAppWindow;

    void save(void);
};
