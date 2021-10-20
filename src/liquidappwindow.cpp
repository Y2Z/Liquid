#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QWebEngineHistory>

#include "config.h"
#include "liquidappcookiejar.hpp"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"

LiquidAppWindow::LiquidAppWindow(QString* name) : QWebEngineView()
{
    liquidAppName = name;

    setMinimumSize(CONFIG_LIQUID_APP_WIN_MINSIZE_W, CONFIG_LIQUID_APP_WIN_MINSIZE_H);

    // Disable context menu
    setContextMenuPolicy(Qt::PreventContextMenu);

    liquidAppSettings = new QSettings(QSettings::IniFormat,
                                      QSettings::UserScope,
                                      CONFIG_APPS_PATH,
                                      *name,
                                      nullptr);

    // Default starting web settings for all Liquid apps
    QWebEngineSettings *globalWebSettings = QWebEngineSettings::globalSettings();
    globalWebSettings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    globalWebSettings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
#endif
    globalWebSettings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    globalWebSettings->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false);
    globalWebSettings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    globalWebSettings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    globalWebSettings->setAttribute(QWebEngineSettings::JavascriptCanPaste, false);
#endif
    globalWebSettings->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    globalWebSettings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    globalWebSettings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
#endif
    globalWebSettings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    globalWebSettings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    globalWebSettings->setAttribute(QWebEngineSettings::ShowScrollBars, true);
#endif

    liquidAppWebProfile = new QWebEngineProfile(QString(), this);
    liquidAppWebProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    liquidAppWebProfile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    // Make this profile's web settings equal to default web settings
    setWebSettingsToDefault(liquidAppWebProfile->settings());

    if (!liquidAppWebProfile->isOffTheRecord()) {
        qDebug().noquote() << "Web profile is not off-the-record!";
    }

    liquidAppWebPage = new LiquidAppWebPage(liquidAppWebProfile, this);
    setPage(liquidAppWebPage);

    // Set window title
    liquidAppWindowTitle = *liquidAppName;
    if (liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        liquidAppWindowTitle = liquidAppSettings->value(SETTINGS_KEY_TITLE).toString();
        // Make sure the window title never gets changed
        liquidAppWindowTitleIsReadOnly = true;
    }
    updateWindowTitle();

    QUrl url(liquidAppSettings->value(SETTINGS_KEY_URL).toString());
    if (url.isValid()) {
        // Set the page's background color behind the document's body
        QColor backgroundColor;
        if (liquidAppSettings->contains(SETTINGS_KEY_BACKGROUND_COLOR)) {
            backgroundColor = QColor(liquidAppSettings->value(SETTINGS_KEY_BACKGROUND_COLOR).toString());
        } else {
            backgroundColor = Qt::black;
        }
        page()->setBackgroundColor(backgroundColor);

        // Deal with Cookies
        {
            LiquidAppCookieJar *liquidAppCookieJar = new LiquidAppCookieJar(this);
            QWebEngineCookieStore *cookieStore = page()->profile()->cookieStore();

            connect(cookieStore, &QWebEngineCookieStore::cookieAdded, liquidAppCookieJar, &LiquidAppCookieJar::upsertCookie);
            connect(cookieStore, &QWebEngineCookieStore::cookieRemoved, liquidAppCookieJar, &LiquidAppCookieJar::removeCookie);

            liquidAppCookieJar->restoreCookies(cookieStore);
        }

        // Restore window geometry
        if (liquidAppSettings->contains(SETTINGS_KEY_WINDOW_GEOMETRY)) {
            restoreGeometry(QByteArray::fromHex(
                liquidAppSettings->value(SETTINGS_KEY_WINDOW_GEOMETRY).toByteArray()
            ));
        }

        // Toggle JavaScript on if enabled in application config
        if (liquidAppSettings->contains(SETTINGS_KEY_ENABLE_JS)) {
            if (liquidAppSettings->value(SETTINGS_KEY_ENABLE_JS).toBool()) {
                settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
            }
        }

        // Toggle JavaScript on if enabled in application config
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        if (liquidAppSettings->contains(SETTINGS_KEY_SHOW_SCROLL_BARS)) {
                settings()->setAttribute(
                    QWebEngineSettings::ShowScrollBars,
                    liquidAppSettings->value(SETTINGS_KEY_SHOW_SCROLL_BARS).toBool()
                );
        }
#endif

        // Mute audio if muted in application config
        if (liquidAppSettings->contains(SETTINGS_KEY_MUTED_AUDIO)) {
            page()->setAudioMuted(liquidAppSettings->value(SETTINGS_KEY_MUTED_AUDIO).toBool());
        }

        // Web view zoom level
        if (liquidAppSettings->contains(SETTINGS_KEY_ZOOM)) {
            setZoomFactor(liquidAppSettings->value(SETTINGS_KEY_ZOOM).toDouble());
        }

        // Lock the app's window's geometry if it was locked when it was last closed
        if (liquidAppSettings->contains(SETTINGS_KEY_WINDOW_GEOMETRY_LOCKED)) {
            if (liquidAppSettings->value(SETTINGS_KEY_WINDOW_GEOMETRY_LOCKED).toBool()) {
                toggleWindowGeometryLock();
                windowGeometryIsLocked = true;
            }
        }

        // Custom user-agent string
        if (liquidAppSettings->contains(SETTINGS_KEY_USER_AGENT)) {
            liquidAppWebProfile->setHttpUserAgent(liquidAppSettings->value(SETTINGS_KEY_USER_AGENT).toString());
        }

        // Custom user-defined CSS (does not require JavascriptEnabled in order to work)
        if (liquidAppSettings->contains(SETTINGS_KEY_CUSTOM_CSS)) {
            const QString b64data = liquidAppSettings->value(SETTINGS_KEY_CUSTOM_CSS).toString().toUtf8().toBase64();
            const QString cssDataURI = "data:text/css;charset=utf-8;base64," + b64data;
            const QString js = QString("(()=>{"\
                                            "const styleEl = document.createElement('style');"\
                                            "const cssTextNode = document.createTextNode('@import url(%1)');"\
                                            "styleEl.appendChild(cssTextNode);"\
                                            "document.head.appendChild(styleEl)"\
                                        "})()").arg(cssDataURI);
            QWebEngineScript script;
            script.setInjectionPoint(QWebEngineScript::DocumentReady);
            script.setRunsOnSubFrames(false);
            script.setSourceCode(js);
            script.setWorldId(QWebEngineScript::ApplicationWorld);
            liquidAppWebPage->scripts().insert(script);
        }

        // Custom user-defined JS (does not require JavascriptEnabled in order to work)
        if (liquidAppSettings->contains(SETTINGS_KEY_CUSTOM_JS)) {
            QString js = liquidAppSettings->value(SETTINGS_KEY_CUSTOM_JS).toString();
            QWebEngineScript script;
            script.setInjectionPoint(QWebEngineScript::DocumentReady);
            script.setRunsOnSubFrames(false);
            script.setSourceCode(js);
            script.setWorldId(QWebEngineScript::ApplicationWorld);
            liquidAppWebPage->scripts().insert(script);
        }

        // Connect keyboard shortcuts
        bindKeyboardShortcuts();

        // Initialize context menu
        setupContextMenu();

        if (liquidAppSettings->contains(SETTINGS_KEY_ICON)) {
            QIcon liquidAppIcon;
            QByteArray byteArray = QByteArray::fromHex(
                liquidAppSettings->value(SETTINGS_KEY_ICON).toByteArray()
            );
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::ReadOnly);
            QDataStream in(&buffer);
            in >> liquidAppIcon;
            buffer.close();
            window()->setWindowIcon(liquidAppIcon);
        }

        // Allow page-level fullscreen happen
        connect(page(), &QWebEnginePage::fullScreenRequested, this, [](QWebEngineFullScreenRequest request) {
            request.accept();
        });

        // Trigger window title update if <title> changes
        connect(this, SIGNAL(titleChanged(QString)), SLOT(updateWindowTitle(QString)));

        // Update Liquid application's icon using the one provided by the website
        connect(page(), SIGNAL(iconChanged(QIcon)), this, SLOT(onIconChanged(QIcon)));

        // Catch loading's start
        connect(this, &QWebEngineView::loadStarted, this, &LiquidAppWindow::loadStarted);

        // Catch loading's end
        connect(this, SIGNAL(loadFinished(bool)), SLOT(loadFinished()));

        // Catch mute/unmute and save to app config
        connect(page(), &QWebEnginePage::audioMutedChanged, this, [this](const bool muted){
            liquidAppSettings->setValue(SETTINGS_KEY_MUTED_AUDIO, muted);
        });

        // Load Liquid app's starting URL
        load(url);

        // Reveal Liquid app's window and bring it to front
        show();
        raise();
        activateWindow();
    } else {
        qDebug() << "Invalid Liquid application URL:" << url;
    }
}

LiquidAppWindow::~LiquidAppWindow()
{
    delete liquidAppWebPage;
    delete liquidAppWebProfile;
}

void LiquidAppWindow::contextMenuEvent(QContextMenuEvent* event)
{
    (void)event;

    contextMenuBackAction->setEnabled(history()->canGoBack());
    contextMenuForwardAction->setEnabled(history()->canGoForward());

    contextMenu->exec(QCursor::pos());
}

void LiquidAppWindow::bindKeyboardShortcuts(void)
{
    // Connect window geometry lock shortcut
    toggleGeometryLockAction = new QAction;
    toggleGeometryLockAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_GEOMETRY_LOCK_TOGGLE)));
    addAction(toggleGeometryLockAction);
    connect(toggleGeometryLockAction, SIGNAL(triggered()), this, SLOT(toggleWindowGeometryLock()));

    // Connect "mute audio" shortcut
    muteAudioAction = new QAction;
    muteAudioAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_MUTE_AUDIO)));
    addAction(muteAudioAction);
    connect(muteAudioAction, &QAction::triggered, this, [this](){
        page()->setAudioMuted(!page()->isAudioMuted());
        updateWindowTitle(title());
    });

    // Connect "go back" shortcut
    backAction = new QAction;
    backAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_BACK)));
    addAction(backAction);
    connect(backAction, SIGNAL(triggered()), this, SLOT(back()));

    // Connect "go back" shortcut (backspace)
    backAction2 = new QAction;
    backAction2->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_BACK_2)));
    addAction(backAction2);
    connect(backAction2, SIGNAL(triggered()), this, SLOT(back()));

    // Connect "go forward" shortcut
    forwardAction = new QAction;
    forwardAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_FORWARD)));
    addAction(forwardAction);
    connect(forwardAction, SIGNAL(triggered()), this, SLOT(forward()));

    // Connect "reload" shortcut
    reloadAction = new QAction;
    reloadAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RELOAD)));
    addAction(reloadAction);
    connect(reloadAction, SIGNAL(triggered()), this, SLOT(reload()));
    // Connect "alternative reload" shortcut (there can be only one QKeySequence per QAction)
    reloadAction2 = new QAction;
    reloadAction2->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RELOAD_2)));
    addAction(reloadAction2);
    connect(reloadAction2, SIGNAL(triggered()), this, SLOT(reload()));

    // Connect "hard reload" shortcut
    hardReloadAction = new QAction;
    hardReloadAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RESET)));
    addAction(hardReloadAction);
    connect(hardReloadAction, SIGNAL(triggered()), this, SLOT(hardReload()));

    // Connect "toggle fulls-creen" shortcut
    toggleFullScreenModeAction = new QAction;
    toggleFullScreenModeAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE)));
    addAction(toggleFullScreenModeAction);
    connect(toggleFullScreenModeAction, SIGNAL(triggered()), this, SLOT(toggleFullScreenMode()));
    // Connect "alternative toggle full-screen" shortcut (there can be only one QKeySequence per QAction)
    toggleFullScreenModeAction2 = new QAction;
    toggleFullScreenModeAction2->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE_2)));
    addAction(toggleFullScreenModeAction2);
    connect(toggleFullScreenModeAction2, SIGNAL(triggered()), this, SLOT(toggleFullScreenMode()));

    // Connect "stop loading"/"exit full-screen mode" shortcut
    stopLoadingOrExitFullScreenModeAction = new QAction;
    stopLoadingOrExitFullScreenModeAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_EXIT)));
    addAction(stopLoadingOrExitFullScreenModeAction);
    connect(stopLoadingOrExitFullScreenModeAction, SIGNAL(triggered()), this, SLOT(stopLoadingOrExitFullScreenMode()));

    // Connect "zoom in" shortcut
    zoomInAction = new QAction;
    zoomInAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_IN)));
    addAction(zoomInAction);
    connect(zoomInAction, SIGNAL(triggered()), this, SLOT(zoomIn()));

    // Connect "zoom out" shortcut
    zoomOutAction = new QAction;
    zoomOutAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_OUT)));
    addAction(zoomOutAction);
    connect(zoomOutAction, SIGNAL(triggered()), this, SLOT(zoomOut()));

    // Connect "reset zoom" shortcut
    zoomResetAction = new QAction;
    zoomResetAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_RESET)));
    addAction(zoomResetAction);
    connect(zoomResetAction, SIGNAL(triggered()), this, SLOT(zoomReset()));

    // Connect "exit" shortcut
    quitAction = new QAction;
    quitAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_QUIT)));
    addAction(quitAction);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));

    // Connect "alternative exit" shortcut
    quitAction2 = new QAction;
    quitAction2->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_QUIT_2)));
    addAction(quitAction2);
    connect(quitAction2, SIGNAL(triggered()), this, SLOT(close()));

    // Make it possible to intercept zoom events
    QApplication::instance()->installEventFilter(this);
}

void LiquidAppWindow::closeEvent(QCloseEvent* event)
{
    if (!isFullScreen()) {
        liquidAppSettings->setValue(SETTINGS_KEY_WINDOW_GEOMETRY, QString(liquidAppWindowGeometry.toHex()));
        liquidAppSettings->sync();
    }

    event->accept();
    deleteLater();
}

bool LiquidAppWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched->parent() == this) {
        switch (event->type()) {
            case QEvent::Wheel:
                if (handleWheelEvent(static_cast<QWheelEvent*>(event))) {
                    return true;
                }
                break;

            default:
                break;
        }
    }

    return QWebEngineView::eventFilter(watched, event);
}

void LiquidAppWindow::exitFullScreenMode()
{
    // Exit from full-screen mode
    setWindowState(windowState() & ~Qt::WindowFullScreen);

    if (windowGeometryIsLocked) {
        // Pause here to wait for any kind of window resize animations to finish
        {
            QTime proceedAfter = QTime::currentTime().addMSecs(200);
            while (QTime::currentTime() < proceedAfter) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
        }

        setMinimumSize(width(), height());
        setMaximumSize(width(), height());
    }
}

void LiquidAppWindow::stopLoadingOrExitFullScreenMode()
{
    if (pageIsLoading) {
        triggerPageAction(QWebEnginePage::Stop);
    } else {
        exitFullScreenMode();
    }
}

bool LiquidAppWindow::handleWheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        (event->delta() > 0) ? zoomIn() : zoomOut();
        event->accept();
        return true;
    }

    return false;
}

void LiquidAppWindow::loadFinished()
{
    pageIsLoading = false;

    updateWindowTitle(title());
}

void LiquidAppWindow::loadStarted()
{
    pageIsLoading = true;

    updateWindowTitle(title());
}

void LiquidAppWindow::moveEvent(QMoveEvent *event)
{
    // Remember window position
    liquidAppWindowGeometry = saveGeometry();

    QWebEngineView::moveEvent(event);
}

void LiquidAppWindow::onIconChanged(QIcon icon)
{
    // Set window icon
    setWindowIcon(icon);

    // Save icon in settings
    if (!liquidAppSettings->contains(SETTINGS_KEY_ICON)) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        QDataStream out(&buffer);
        out << icon;
        buffer.close();
        liquidAppSettings->setValue(SETTINGS_KEY_ICON, QString(byteArray.toHex()));
        liquidAppSettings->sync();
    }
}

void LiquidAppWindow::setupContextMenu(void)
{
    contextMenu = new QMenu;

    contextMenuCopyUrlAction = new QAction(QIcon::fromTheme(QStringLiteral("internet-web-browser")), tr("Copy Current URL"));
    contextMenuReloadAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh"));
    contextMenuBackAction = new QAction(QIcon::fromTheme(QStringLiteral("go-previous")), tr("Go Back"));
    contextMenuForwardAction = new QAction(QIcon::fromTheme(QStringLiteral("go-next")), tr("Go Forward"));
    contextMenuCloseAction = new QAction(QIcon::fromTheme(QStringLiteral("process-stop")), tr("Quit"));

    contextMenu->addAction(contextMenuCopyUrlAction);
    contextMenu->addAction(contextMenuReloadAction);
    contextMenu->addAction(contextMenuBackAction);
    contextMenu->addAction(contextMenuForwardAction);
    contextMenu->addAction(contextMenuCloseAction);

    connect(contextMenuCopyUrlAction, &QAction::triggered, this, [this](){
        QApplication::clipboard()->setText(page()->url().toString());
    });
    connect(contextMenuReloadAction, &QAction::triggered, this, &QWebEngineView::reload);
    connect(contextMenuBackAction, &QAction::triggered, this, &QWebEngineView::back);
    connect(contextMenuForwardAction, &QAction::triggered, this, &QWebEngineView::forward);
    connect(contextMenuCloseAction, SIGNAL(triggered()), this, SLOT(close()));

    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void LiquidAppWindow::setWebSettingsToDefault(QWebEngineSettings *webSettings)
{
    QWebEngineSettings *defaultWebSettings = QWebEngineSettings::defaultSettings();

    static QList<QWebEngineSettings::WebAttribute> webAttributeKeys = {
        QWebEngineSettings::AutoLoadImages,
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        QWebEngineSettings::DnsPrefetchEnabled,
#endif
        QWebEngineSettings::FullScreenSupportEnabled,
        QWebEngineSettings::HyperlinkAuditingEnabled,
        QWebEngineSettings::JavascriptCanAccessClipboard,
        QWebEngineSettings::JavascriptCanOpenWindows,
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        QWebEngineSettings::JavascriptCanPaste,
#endif
        QWebEngineSettings::JavascriptEnabled,
        QWebEngineSettings::LocalStorageEnabled,
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
        QWebEngineSettings::PdfViewerEnabled,
#endif
        QWebEngineSettings::PluginsEnabled,
        QWebEngineSettings::ScrollAnimatorEnabled,
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        QWebEngineSettings::ShowScrollBars,
#endif
    };

    for (int i = 0; i < webAttributeKeys.size(); i++) {
        webSettings->setAttribute(webAttributeKeys.at(i), defaultWebSettings->testAttribute(webAttributeKeys.at(i)));
    }
}

void LiquidAppWindow::hardReload()
{
    // TODO: if JS enabled, stop all currently running JS (destroy web workers, promises, etc)

    // Synchronously wipe all document contents (page's setContent() and setHtml() are aynchrnonous, can't use them here)
    const QString js = QString("(()=>{"\
                                   "let e=document.firstElementChild;"\
                                   "if(e){"\
                                       "e.remove()"\
                                   "}"\
                               "})()");
    page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);

    // Ensure that while this Liquid app is being reset, the window title remains to be set to this Liquid application's name
    // to mimic the same experience that happens when the user first launches this Liquid app
    if (!liquidAppWindowTitleIsReadOnly) {
        liquidAppWindowTitle = *liquidAppName;

        const QString js = QString("(()=>{"\
                                       "let e=document.createElement('title');"\
                                       "e.innerText='%1';"\
                                       "document.appendChild(e)"\
                                   "})()").arg((liquidAppWindowTitle).replace("'", "\\'"));
        page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);
    }

    updateWindowTitle(title());

    // TODO: reset localStorage / Cookies in case they're disabled?

    QUrl url(liquidAppSettings->value(SETTINGS_KEY_URL).toString(), QUrl::StrictMode);
    setUrl(url);
}

void LiquidAppWindow::resizeEvent(QResizeEvent *event)
{
    // Remember window size (unless in full-screen mode)
    if (!isFullScreen()) {
        liquidAppWindowGeometry = saveGeometry();
    }

    QWebEngineView::resizeEvent(event);
}

void LiquidAppWindow::toggleFullScreenMode()
{
    if (isFullScreen()) {
        exitFullScreenMode();
    } else {
        // Make it temporarily possible to resize the window if geometry is locked
        if (windowGeometryIsLocked) {
            setMinimumSize(CONFIG_WIN_MINSIZE_W, CONFIG_WIN_MINSIZE_H);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
        // Enter the full-screen mode
        setWindowState(windowState() | Qt::WindowFullScreen);
    }
}

void LiquidAppWindow::toggleWindowGeometryLock()
{
    // Prevent toggling window geometry lock while in full-screen mode
    if (!isFullScreen()) {
        if (windowGeometryIsLocked) {
            // Open up resizing restrictions
            setMinimumSize(CONFIG_WIN_MINSIZE_W, CONFIG_WIN_MINSIZE_H);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            windowGeometryIsLocked = false;
        } else {
            // Lock down resizing
            setMinimumSize(width(), height());
            setMaximumSize(width(), height());
            windowGeometryIsLocked = true;
        }

        liquidAppSettings->setValue(SETTINGS_KEY_WINDOW_GEOMETRY_LOCKED, windowGeometryIsLocked);
        liquidAppSettings->sync();
    }

    updateWindowTitle(title());
}

void LiquidAppWindow::updateWindowTitle(QString title)
{
    QString textIcons;

    if (!liquidAppWindowTitleIsReadOnly) {
        if (title.size() > 0) {
            liquidAppWindowTitle = title;
        } else {
            liquidAppWindowTitle = *liquidAppName;
        }
    }

    if (windowGeometryIsLocked) {
        textIcons.append(ICON_LOCKED);
    }
    if (page()->isAudioMuted()) {
        textIcons.append(ICON_MUTED);
    }
    if (pageIsLoading) {
        textIcons.append(ICON_LOADING);
    }

    if (textIcons != "") {
        textIcons = " " + textIcons;
    }

    setWindowTitle(liquidAppWindowTitle + textIcons);
}

void LiquidAppWindow::zoomBy(qreal factor)
{
    qreal newZoomFactor = zoomFactor() + factor;

    if (factor) {
        if (newZoomFactor < CONFIG_ZOOM_MIN) {
            newZoomFactor = CONFIG_ZOOM_MIN;
        } else if (newZoomFactor > CONFIG_ZOOM_MAX) {
            newZoomFactor = CONFIG_ZOOM_MAX;
        }
    } else {
        newZoomFactor = 1;
    }

    setZoomFactor(newZoomFactor);

    liquidAppSettings->setValue(SETTINGS_KEY_ZOOM, newZoomFactor);
    liquidAppSettings->sync();
}

void LiquidAppWindow::zoomIn()
{
    zoomBy(CONFIG_ZOOM_STEP);
}

void LiquidAppWindow::zoomOut()
{
    zoomBy(-CONFIG_ZOOM_STEP);
}

void LiquidAppWindow::zoomReset()
{
    zoomBy(0);
}
