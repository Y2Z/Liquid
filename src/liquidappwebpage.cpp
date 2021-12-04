#include <QDesktopServices>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWebEngineSettings>

#include "liquid.hpp"
#include "liquidappwebpage.hpp"
#include "lqd.h"

LiquidAppWebPage::LiquidAppWebPage(QWebEngineProfile* profile, LiquidAppWindow* parent) : QWebEnginePage(profile, parent)
{
    liquidAppWindow = parent;

    // Set this profile's web settings to default
    setWebSettingsToDefault(profile->settings());
}

void LiquidAppWebPage::addAllowedDomain(const QString domain) {
    // TODO: check if already there
    allowedDomainsList->append(domain);
}

void LiquidAppWebPage::addAllowedDomains(const QStringList domainsList) {
    allowedDomainsList->append(domainsList);
    // TODO: remove duplicates from allowedDomainsList
}

bool LiquidAppWebPage::acceptNavigationRequest(const QUrl& reqUrl, const QWebEnginePage::NavigationType navReqType, const bool isMainFrame)
{
    const bool isDomainAllowed = allowedDomainsList->contains(reqUrl.host());
    const bool isKeyModifierActive = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);

    // Top-level window
    switch (navReqType) {
        // Open external websites using system's default browser
        case QWebEnginePage::NavigationTypeLinkClicked:
            // QWebEnginePage::NavigationTypeLinkClicked is the same type for both JS-induced clicks and actual physical clicks made by the user (go figure...)
            // isMainFrame is the only thing that indicates that it was the user who clicked the link, not some JS code (e.g. to redirect / pop some window up)
            if (isMainFrame && (!isDomainAllowed || isKeyModifierActive)) {
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
            if (!isDomainAllowed) {
                liquidAppWindow->setForgiveNextPageLoadError(true);
                return false;
            }
            break;

        default:;
    }

    return true;
}

bool LiquidAppWebPage::certificateError(const QWebEngineCertificateError& error)
{
    (void)error;

    emit liquidAppWindow->certificateError();

    return true;
}

void LiquidAppWebPage::closeJsDialog()
{
    if (jsDialogWidget != Q_NULLPTR) {
        jsDialogWidget->reject();
    }
}

void LiquidAppWebPage::javaScriptAlert(const QUrl& securityOrigin, const QString& msg)
{
    (void)securityOrigin;

    jsDialogWidget = new QDialog(liquidAppWindow, Qt::FramelessWindowHint);
    jsDialogWidget->setObjectName("liquidAppJsDialogAlert");
    jsDialogWidget->setAttribute(Qt::WA_DeleteOnClose);
    jsDialogWidget->setWindowModality(Qt::ApplicationModal);
    Liquid::applyQtStyleSheets(jsDialogWidget);

    QVBoxLayout* jsAlertDialogLayout = new QVBoxLayout(jsDialogWidget);

    jsDialogWidget->setLayout(jsAlertDialogLayout);

    jsAlertDialogLayout->addWidget(new QLabel(msg));

    jsDialogWidget->exec();
    jsDialogWidget = Q_NULLPTR;
}

bool LiquidAppWebPage::javaScriptConfirm(const QUrl& securityOrigin, const QString& msg)
{
    (void)securityOrigin;

    jsDialogWidget = new QDialog(liquidAppWindow, Qt::FramelessWindowHint);
    jsDialogWidget->setObjectName("liquidAppJsDialogConfirm");
    jsDialogWidget->setAttribute(Qt::WA_DeleteOnClose);
    jsDialogWidget->setWindowModality(Qt::ApplicationModal);
    Liquid::applyQtStyleSheets(jsDialogWidget);

    QVBoxLayout* jsConfirmDialogLayout = new QVBoxLayout(jsDialogWidget);

    jsDialogWidget->setLayout(jsConfirmDialogLayout);

    jsConfirmDialogLayout->addWidget(new QLabel(msg, jsDialogWidget));

    QPushButton* jsConfirmDialogButton = new QPushButton(tr("Confirm"), jsDialogWidget);
    jsConfirmDialogLayout->addWidget(jsConfirmDialogButton);

    connect(jsConfirmDialogButton, &QPushButton::clicked, jsDialogWidget, &QDialog::accept);

    if (jsDialogWidget->exec() == QDialog::Accepted) {
        jsDialogWidget = Q_NULLPTR;
        return true;
    }

    jsDialogWidget = Q_NULLPTR;
    return false;
}

bool LiquidAppWebPage::javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue, QString* result)
{
    (void)securityOrigin;

    jsDialogWidget = new QDialog(liquidAppWindow, Qt::FramelessWindowHint);
    jsDialogWidget->setObjectName("liquidAppJsDialogPrompt");
    jsDialogWidget->setAttribute(Qt::WA_DeleteOnClose);
    jsDialogWidget->setWindowModality(Qt::ApplicationModal);
    Liquid::applyQtStyleSheets(jsDialogWidget);

    QHBoxLayout* jsPromptDialogLayout = new QHBoxLayout(jsDialogWidget);

    jsDialogWidget->setLayout(jsPromptDialogLayout);

    jsPromptDialogLayout->addWidget(new QLabel(msg, jsDialogWidget));

    QLineEdit* jsPromptDialogTextInput = new QLineEdit(defaultValue, jsDialogWidget);
    jsPromptDialogLayout->addWidget(jsPromptDialogTextInput);

    QPushButton* jsPromptDialogButton = new QPushButton(tr("Submit"), jsDialogWidget);
    jsPromptDialogLayout->addWidget(jsPromptDialogButton);

    connect(jsPromptDialogButton, &QPushButton::clicked, jsDialogWidget, &QDialog::accept);

    if (jsDialogWidget->exec() == QDialog::Accepted) {
        *result = jsPromptDialogTextInput->text();
        jsDialogWidget = Q_NULLPTR;
        return true;
    }

    jsDialogWidget = Q_NULLPTR;
    return false;
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
