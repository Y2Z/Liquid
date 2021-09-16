#pragma once

#include <QtGui/QDesktopServices>
#include <QWebEnginePage>
#include <QWebEngineProfile>

class LiquidAppWebPage : public QWebEnginePage
{
public:
    LiquidAppWebPage(QWebEngineProfile *profile, QObject *parent = 0);

protected:
    bool acceptNavigationRequest(const QUrl &reqUrl, QWebEnginePage::NavigationType navReqType, bool isMainFrame);

private:
    QWebEngineProfile *liquidAppWebProfile;
};
