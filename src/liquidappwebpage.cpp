#include <QDesktopServices>
#include <QGuiApplication>
#include <QWebEngineSettings>

#include "globals.h"

#include "liquidappwebpage.hpp"

LiquidAppWebPage::LiquidAppWebPage(QWebEngineProfile* profile, QObject* parent) : QWebEnginePage(profile, parent)
{
    liquidAppWindow = (LiquidAppWindow*)parent;

    // Set this profile's web settings to default
    setWebSettingsToDefault(profile->settings());
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

void LiquidAppWebPage::setWebSettingsToDefault(QWebEngineSettings* webSettings)
{
    // Default starting web settings for all Liquid apps
    webSettings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    webSettings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
#endif
    webSettings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    webSettings->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false);
    webSettings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    webSettings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    webSettings->setAttribute(QWebEngineSettings::JavascriptCanPaste, false);
#endif
    webSettings->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    webSettings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    webSettings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
#endif
    webSettings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    webSettings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    webSettings->setAttribute(QWebEngineSettings::ShowScrollBars, true);
#endif
}
