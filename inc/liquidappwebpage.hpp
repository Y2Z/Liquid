#pragma once

#include <QAuthenticator>
#include <QDialog>
#include <QWebEngineCertificateError>
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool certificateError(const QWebEngineCertificateError& error) override;
#else
    void handleCertificateError(const QWebEngineCertificateError& error);
#endif
    void javaScriptAlert(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptConfirm(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue, QString* result) override;

    LiquidAppWindow* liquidAppWindow = Q_NULLPTR;
    QStringList* allowedDomainsList = new QStringList();

    QDialog* dialogWidget = Q_NULLPTR;
};
