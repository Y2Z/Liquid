#include "config.h"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"

LiquidAppWebPage::LiquidAppWebPage(QWebEngineProfile *profile, QObject *parent) : QWebEnginePage(profile, parent)
{
}

bool LiquidAppWebPage::acceptNavigationRequest(const QUrl &reqUrl, QWebEnginePage::NavigationType navReqType, bool isMainFrame)
{
    if (isMainFrame) {
        switch (navReqType) {
            // Open external websites using system's default browser
            case QWebEnginePage::NavigationTypeLinkClicked:
                if (url().host() != reqUrl.host()) {
                    QDesktopServices::openUrl(reqUrl);
                    return false;
                }
                break;

            // Prevent form submissions to other hosts
            case QWebEnginePage::NavigationTypeFormSubmitted:
                if (url().host() != reqUrl.host()) {
                    return false;
                }
                break;

            default:;
        }
    }

    return QWebEnginePage::acceptNavigationRequest(reqUrl, navReqType, isMainFrame);
}
