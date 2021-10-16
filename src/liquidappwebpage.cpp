#include <QGuiApplication>

#include "config.h"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"

LiquidAppWebPage::LiquidAppWebPage(QWebEngineProfile* profile, QObject* parent) : QWebEnginePage(profile, parent)
{
}

bool LiquidAppWebPage::acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navType, const bool isMainFrame)
{
    const bool differentHost = url().host() != reqUrl.host();
    const bool keyModifierActive = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);

    // Top-level window
    switch (navType) {
        // Open external websites using system's default browser
        case QWebEnginePage::NavigationTypeLinkClicked:
            if ((isMainFrame && differentHost) || keyModifierActive) {
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

    return QWebEnginePage::acceptNavigationRequest(reqUrl, navType, isMainFrame);
}
