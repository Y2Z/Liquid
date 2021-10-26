#include <QDesktopServices>
#include <QGuiApplication>

#include "globals.h"

#include "liquidappwebpage.hpp"

LiquidAppWebPage::LiquidAppWebPage(QWebEngineProfile* profile, QObject* parent) : QWebEnginePage(profile, parent)
{
    liquidAppWindow = (LiquidAppWindow*)parent;
}

bool LiquidAppWebPage::acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navReqType, const bool isMainFrame)
{
    const bool isDifferentHost = url().host() != reqUrl.host();
    const bool keyModifierActive = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);

    // Top-level window
    switch (navReqType) {
        // Open external websites using system's default browser
        case QWebEnginePage::NavigationTypeLinkClicked:
            // QWebEnginePage::NavigationTypeLinkClicked is the same type for both JS-induced clicks and actual physical clicks made by the user (go figure...)
            // isMainFrame is the only thing that indicates that it was the user who clicked the link, not some JS code (e.g. to redirect / pop some window up)
            if (isMainFrame && (isDifferentHost || keyModifierActive)) {
                QDesktopServices::openUrl(reqUrl);
                liquidAppWindow->setForgiveNextPageLoadError(true);
                return false;
            }
            break;

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        // Prevent redirects
        case QWebEnginePage::NavigationTypeRedirect:
#endif
        // Prevent form submissions to other hosts
        case QWebEnginePage::NavigationTypeFormSubmitted:
            if (isDifferentHost) {
                liquidAppWindow->setForgiveNextPageLoadError(true);
                return false;
            }
            break;

        default:;
    }

    return true;
}
