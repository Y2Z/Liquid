#pragma once

#include <QtGui/QDesktopServices>
#include <QWebEnginePage>
#include <QWebEngineProfile>

class LiquidAppWebPage : public QWebEnginePage
{
public:
    LiquidAppWebPage(QWebEngineProfile* profile, QObject* parent = 0);

protected:
    bool acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navType, const bool isMainFrame);

private:
    QWebEngineProfile* liquidAppWebProfile;
};
