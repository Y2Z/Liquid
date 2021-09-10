#include "config.h"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"

LiquidAppWebPage::LiquidAppWebPage(QObject *parent) : QWebPage(parent)
{
    QSettings *liquidAppSettings = ((LiquidAppWindow*)parent)->liquidAppSettings;

    if (liquidAppSettings->contains(SETTINGS_KEY_USER_AGENT)) {
        hasCustomUserAgent = true;
        customUserAgent = liquidAppSettings->value(SETTINGS_KEY_USER_AGENT).toString();
    }
}

QString LiquidAppWebPage::userAgentForUrl(const QUrl &url) const
{
    if (hasCustomUserAgent) {
        return customUserAgent;
    }

    return QWebPage::userAgentForUrl(url);
}

bool LiquidAppWebPage::acceptNavigationRequest(QWebFrame *frame,
    const QNetworkRequest &request, QWebPage::NavigationType type)
{
    if (mainFrame()->url() != QUrl()) { // allow initial redirect(s)
        switch (type)
        {
            // Open external websites using system's default browser
            case QWebPage::NavigationTypeLinkClicked:
                if (mainFrame()->url().host() != request.url().host()) {
                    QDesktopServices::openUrl(request.url());
                    return false;
                }
            break;

            // Prevent form submissions to other hosts
            case QWebPage::NavigationTypeFormSubmitted:
                if (mainFrame()->url().host() != request.url().host()) {
                    return false;
                }
            break;

            default:;
        }
    }

    return QWebPage::acceptNavigationRequest(frame, request, type);
}

bool LiquidAppWebPage::extension(Extension extension,
   const ExtensionOption *option, ExtensionReturn *output)
{
    if (!option || !output) {
        return false;
    }

    if (extension == QWebPage::ErrorPageExtension) {
        const ErrorPageExtensionOption *errorOption = static_cast<const ErrorPageExtensionOption*>(option);

        QFile errorPageFile(":/templates/errorpage.html");
        errorPageFile.open(QFile::ReadOnly|QFile::Text);
        QTextStream in(&errorPageFile);
        QString errorPageContent = in.readAll();
        errorPageFile.close();

        QString errorMessageString;
        switch (errorOption->domain)
        {
            case QWebPage::QtNetwork:
                errorMessageString = "Qt network layer, code ";
            break;

            case QWebPage::Http:
                errorMessageString = "HTTP layer, HTTP error code ";
            break;

            case QWebPage::WebKit:
                errorMessageString = "WebKit internals, error code ";
            break;

            default:
                errorMessageString += "Unknown domain, error code ";
            break;
        }

        errorPageContent = errorPageContent.arg(errorOption->url.toString());
        errorPageContent = errorPageContent.arg(errorMessageString);
        errorPageContent = errorPageContent.arg(errorOption->error);
        errorPageContent = errorPageContent.arg(errorOption->errorString);

        ErrorPageExtensionReturn *errorReturn = static_cast<ErrorPageExtensionReturn*>(output);
        errorReturn->baseUrl = errorOption->url;
        errorReturn->content = errorPageContent.toUtf8();

        return true;
    }

    return false;
}

bool LiquidAppWebPage::supportsExtension(Extension extension) const
{
    if (extension == QWebPage::ErrorPageExtension) {
      return true;
    }

    return QWebPage::supportsExtension(extension);
}
