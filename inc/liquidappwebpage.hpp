#pragma once

#include <QWebEnginePage>
#include <QWebEngineProfile>

#include "liquidappwindow.hpp"

class LiquidAppWindow;

class LiquidAppWebPage : public QWebEnginePage
{
public:
    LiquidAppWebPage(QWebEngineProfile* profile, LiquidAppWindow* parent = Q_NULLPTR);

    static void setWebSettingsToDefault(QWebEngineSettings* webSettings);

    void addAllowedDomain(const QString domain);
    void addAllowedDomains(const QStringList domainList);

protected:
    bool acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navReqType, const bool isMainFrame);

private:
    QWebEngineProfile* liquidAppWebProfile;
    LiquidAppWindow* liquidAppWindow;
    QStringList* allowedDomainsList = new QStringList();
};
