#pragma once

#include <QWebEnginePage>
#include <QWebEngineProfile>

#include "liquidappwindow.hpp"

class LiquidAppWindow;

class LiquidAppWebPage : public QWebEnginePage
{
public:
    LiquidAppWebPage(QWebEngineProfile* profile, QObject* parent = Q_NULLPTR);

    static void setWebSettingsToDefault(QWebEngineSettings* webSettings);

protected:
    bool acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navReqType, const bool isMainFrame);

private:
    QWebEngineProfile* liquidAppWebProfile;
    LiquidAppWindow* liquidAppWindow;
};
