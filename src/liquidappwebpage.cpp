#include <QDesktopServices>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWebEngineSettings>

#include "liquidappwebpage.hpp"
#include "lqd.h"

LiquidAppWebPage::LiquidAppWebPage(QWebEngineProfile* profile, LiquidAppWindow* parent) : QWebEnginePage(profile, parent)
{
    liquidAppWindow = parent;

    // Set this profile's web settings to default
    setWebSettingsToDefault(profile->settings());

    // Allow page-level full-screen requests to happen
    connect(this, &QWebEnginePage::fullScreenRequested, this, [](QWebEngineFullScreenRequest request) {
        // This doesn't let JS maximize the OS window, just ensures that this action is always allowed to happen.
        // We basically trick web apps into thinking that they went full-screen, while never really allowing them to (they instead go "full-window").
        request.accept();
    });

    connect(this, &QWebEnginePage::authenticationRequired, this, &LiquidAppWebPage::authenticationRequired);
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

void LiquidAppWebPage::authenticationRequired(const QUrl& requestUrl, QAuthenticator* authenticator)
{
    dialogWidget = new QDialog(liquidAppWindow, Qt::FramelessWindowHint);
    dialogWidget->setAttribute(Qt::WA_DeleteOnClose);
    dialogWidget->setObjectName("liquidAppHttpBasicAuthDialogPrompt");
    dialogWidget->setWindowModality(Qt::ApplicationModal);

    QVBoxLayout* httpBasicAuthPromptDialogLayout = new QVBoxLayout(dialogWidget);
    dialogWidget->setLayout(httpBasicAuthPromptDialogLayout);

    httpBasicAuthPromptDialogLayout->addWidget(new QLabel(requestUrl.host(), dialogWidget));

    QLineEdit* httpBasicAuthPromptDialogUsernameTextInput = new QLineEdit("", dialogWidget);
    httpBasicAuthPromptDialogUsernameTextInput->setPlaceholderText(tr("Username"));
    httpBasicAuthPromptDialogLayout->addWidget(httpBasicAuthPromptDialogUsernameTextInput);

    QLineEdit* httpBasicAuthPromptDialogPasswordTextInput = new QLineEdit("", dialogWidget);
    httpBasicAuthPromptDialogPasswordTextInput->setPlaceholderText(tr("Password"));
    httpBasicAuthPromptDialogLayout->addWidget(httpBasicAuthPromptDialogPasswordTextInput);

    QPushButton* dialogPromptButton = new QPushButton(tr("Sign in"), dialogWidget);
    httpBasicAuthPromptDialogLayout->addWidget(dialogPromptButton);

    connect(dialogPromptButton, &QPushButton::clicked, dialogWidget, &QDialog::accept);

    if (dialogWidget->exec() == QDialog::Accepted) {
        authenticator->setUser(httpBasicAuthPromptDialogUsernameTextInput->text());
        authenticator->setPassword(httpBasicAuthPromptDialogPasswordTextInput->text());
    } else {
        *authenticator = QAuthenticator();
    }

    dialogWidget = Q_NULLPTR;
}

bool LiquidAppWebPage::certificateError(const QWebEngineCertificateError& error)
{
    Q_UNUSED(error);

    emit liquidAppWindow->certificateError();

    return true;
}

void LiquidAppWebPage::closeJsDialog()
{
    if (dialogWidget != Q_NULLPTR) {
        dialogWidget->reject();
    }
}

void LiquidAppWebPage::javaScriptAlert(const QUrl& securityOrigin, const QString& msg)
{
    Q_UNUSED(securityOrigin);

    dialogWidget = new QDialog(liquidAppWindow, Qt::FramelessWindowHint);
    dialogWidget->setAttribute(Qt::WA_DeleteOnClose);
    dialogWidget->setObjectName("liquidAppJsDialogAlert");
    dialogWidget->setWindowModality(Qt::ApplicationModal);

    QVBoxLayout* jsAlertDialogLayout = new QVBoxLayout(dialogWidget);

    dialogWidget->setLayout(jsAlertDialogLayout);

    jsAlertDialogLayout->addWidget(new QLabel(msg));

    dialogWidget->exec();
    dialogWidget = Q_NULLPTR;
}

bool LiquidAppWebPage::javaScriptConfirm(const QUrl& securityOrigin, const QString& msg)
{
    Q_UNUSED(securityOrigin);

    dialogWidget = new QDialog(liquidAppWindow, Qt::FramelessWindowHint);
    dialogWidget->setAttribute(Qt::WA_DeleteOnClose);
    dialogWidget->setObjectName("liquidAppJsDialogConfirm");
    dialogWidget->setWindowModality(Qt::ApplicationModal);

    QVBoxLayout* jsConfirmDialogLayout = new QVBoxLayout(dialogWidget);

    dialogWidget->setLayout(jsConfirmDialogLayout);

    jsConfirmDialogLayout->addWidget(new QLabel(msg, dialogWidget));

    QPushButton* jsConfirmDialogButton = new QPushButton(tr("Confirm"), dialogWidget);
    jsConfirmDialogLayout->addWidget(jsConfirmDialogButton);

    connect(jsConfirmDialogButton, &QPushButton::clicked, dialogWidget, &QDialog::accept);

    if (dialogWidget->exec() == QDialog::Accepted) {
        dialogWidget = Q_NULLPTR;
        return true;
    }

    dialogWidget = Q_NULLPTR;
    return false;
}

bool LiquidAppWebPage::javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue, QString* result)
{
    Q_UNUSED(securityOrigin);

    dialogWidget = new QDialog(liquidAppWindow, Qt::FramelessWindowHint);
    dialogWidget->setAttribute(Qt::WA_DeleteOnClose);
    dialogWidget->setObjectName("liquidAppJsDialogPrompt");
    dialogWidget->setWindowModality(Qt::ApplicationModal);

    QHBoxLayout* jsPromptDialogLayout = new QHBoxLayout(dialogWidget);

    dialogWidget->setLayout(jsPromptDialogLayout);

    jsPromptDialogLayout->addWidget(new QLabel(msg, dialogWidget));

    QLineEdit* jsPromptDialogTextInput = new QLineEdit(defaultValue, dialogWidget);
    jsPromptDialogTextInput->setPlaceholderText(tr("Value"));
    jsPromptDialogLayout->addWidget(jsPromptDialogTextInput);

    QPushButton* dialogPromptButton = new QPushButton(tr("Submit"), dialogWidget);
    jsPromptDialogLayout->addWidget(dialogPromptButton);

    connect(dialogPromptButton, &QPushButton::clicked, dialogWidget, &QDialog::accept);

    if (dialogWidget->exec() == QDialog::Accepted) {
        *result = jsPromptDialogTextInput->text();
        dialogWidget = Q_NULLPTR;
        return true;
    }

    dialogWidget = Q_NULLPTR;
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
    webSettings->setAttribute(QWebEngineSettings::LocalStorageEnabled, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    webSettings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
#endif
    webSettings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    webSettings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    webSettings->setAttribute(QWebEngineSettings::ShowScrollBars, true);
#endif
}
