#pragma once

#include <QtGui/QDesktopServices>
#include <QtNetwork/QNetworkRequest>
#include <QSettings>
#include <QtWebKitWidgets/QWebPage>
#include <QtWebKitWidgets/QWebFrame>

class LiquidAppWebPage : public QWebPage
{
public:
    LiquidAppWebPage(QObject *parent = 0);
    QString userAgentForUrl(const QUrl &url) const;
    bool extension(Extension extension,
                   const ExtensionOption *option,
                   ExtensionReturn *output);
    bool supportsExtension(Extension extension) const;

protected:
    bool acceptNavigationRequest(QWebFrame *frame,
                                 const QNetworkRequest &request,
                                 QWebPage::NavigationType type);

private:
    QString customUserAgent;
    bool hasCustomUserAgent = false;
};
