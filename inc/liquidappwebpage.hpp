#pragma once

#include <QAuthenticator>
#include <QDialog>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include "liquidappwindow.hpp"

class LiquidAppWindow;

class LiquidAppWebPage : public QWebEnginePage
{
public:
    LiquidAppWebPage(QWebEngineProfile* profile, LiquidAppWindow* parent = Q_NULLPTR);

    void addAllowedDomain(const QString domain);
    void addAllowedDomains(const QStringList domainList);
    void closeJsDialog();

    static void setWebSettingsToDefault(QWebEngineSettings* webSettings);

signals:
    void authenticationRequired(const QUrl& requestUrl, QAuthenticator* authenticator);

protected:
    bool acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navReqType, const bool isMainFrame) override;

private:
    bool certificateError(const QWebEngineCertificateError& error);
    void javaScriptAlert(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptConfirm(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue, QString* result) override;
    // TODO: windowCloseRequested

    LiquidAppWindow* liquidAppWindow = Q_NULLPTR;
    QStringList* allowedDomainsList = new QStringList();

    QDialog* dialogWidget = Q_NULLPTR;
};
