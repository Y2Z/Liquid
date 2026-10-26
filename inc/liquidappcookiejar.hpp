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

    // Removes cookies that are no longer in the Liquid app's file (e.g. after getting removed in settings)
    void removeCookiesMissingFromConfig(QWebEngineCookieStore* cookieStore);

    // How cookies are identified in Liquid apps' files
    static QString getCookieId(const QNetworkCookie& cookie);

private:
    QSettings* liquidAppConfig;
    LiquidAppWindow* liquidAppWindow;

    void save(void);
};
