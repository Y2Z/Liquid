#include <QGuiApplication>

#include "config.h"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"

LiquidAppWebPage::LiquidAppWebPage(QWebEngineProfile* profile, QObject* parent) : QWebEnginePage(profile, parent)
{
}

bool LiquidAppWebPage::acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navReqType, const bool isMainFrame)
{
    const bool differentHost = url().host() != reqUrl.host();
    const bool keyModifierActive = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);

    // Top-level window
    switch (navReqType) {
        // Open external websites using system's default browser
        case QWebEnginePage::NavigationTypeLinkClicked:
            // QWebEnginePage::NavigationTypeLinkClicked is the same type for both JS-induced clicks and actual physical clicks made by the user (go figure...)
            // isMainFrame is the only thing that indicates that it was the user who clicked the link, not some JS code (e.g. to redirect / pop some window up)
            if (isMainFrame && (differentHost || keyModifierActive)) {
                QDesktopServices::openUrl(reqUrl);
                return false;
            }
            break;

        // Prevent form submissions to other hosts
        case QWebEnginePage::NavigationTypeFormSubmitted:
            if (differentHost) {
                return false;
            }
            break;

        default:;
    }

    return QWebEnginePage::acceptNavigationRequest(reqUrl, navReqType, isMainFrame);
}
