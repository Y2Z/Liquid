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
    LiquidAppCookieJar(QObject* parent);
    ~LiquidAppCookieJar(void);

    bool removeCookie(const QNetworkCookie& cookie);
    bool upsertCookie(const QNetworkCookie& cookie);

    void restoreCookies(QWebEngineCookieStore* cookieStore);

private:
    QSettings* liquidAppConfig;
    LiquidAppWindow* parentWindow;

    void save(void);
};
