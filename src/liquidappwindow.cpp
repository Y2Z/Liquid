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
    setMinimumSize(CONFIG_LIQUID_APP_WIN_MINSIZE_W, CONFIG_LIQUID_APP_WIN_MINSIZE_H);

    // Disable context menu
    setContextMenuPolicy(Qt::PreventContextMenu);

    isLoading = true;

    liquidAppSettings = new QSettings(QSettings::IniFormat,
                                      QSettings::UserScope,
                                      CONFIG_APPS_PATH,
                                      *name,
                                      nullptr);

    liquidAppName = name;

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

    liquidAppWebProfile = new QWebEngineProfile(QString(), this);
    liquidAppWebProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    liquidAppWebProfile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    // Make this profile's web settings equal to default web settings
    setWebSettingsToDefault(liquidAppWebProfile->settings());

    if (!liquidAppWebProfile->isOffTheRecord()) {
        qDebug() << "Web profile is not off-the-record!";
    }

    liquidAppWebPage = new LiquidAppWebPage(liquidAppWebProfile, this);

    setPage(liquidAppWebPage);

    // Set custom window title
    liquidAppWindowTitle = *liquidAppName;
    if (liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        liquidAppWindowTitle = liquidAppSettings->value(SETTINGS_KEY_TITLE).toString();
    }
    updateWindowTitle(liquidAppWindowTitle);

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

        // Toggle JavaScript on if enabled in application settings
        if (liquidAppSettings->contains(SETTINGS_KEY_ENABLE_JS)) {
            if (liquidAppSettings->value(SETTINGS_KEY_ENABLE_JS).toBool()) {
                settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
            }
        }

        // Restore window geometry
        if (liquidAppSettings->contains(SETTINGS_KEY_WINDOW_GEOMETRY)) {
            restoreGeometry(QByteArray::fromHex(
                liquidAppSettings->value(SETTINGS_KEY_WINDOW_GEOMETRY).toByteArray()
            ));
        }

        // Web view zoom level
        if (liquidAppSettings->contains(SETTINGS_KEY_ZOOM)) {
            setZoomFactor(liquidAppSettings->value(SETTINGS_KEY_ZOOM).toDouble());
        }

        // Lock the app's window's geometry if it was locked when it was last closed
        if (liquidAppSettings->contains(SETTINGS_KEY_WINDOW_GEOMETRY_LOCKED)) {
            if (liquidAppSettings->value(SETTINGS_KEY_WINDOW_GEOMETRY_LOCKED).toBool()) {
                toggleWindowGeometryLock();
                isWindowGeometryLocked = true;
            }
        }

        // Custom user-agent string
        if (liquidAppSettings->contains(SETTINGS_KEY_USER_AGENT)) {
            liquidAppWebProfile->setHttpUserAgent(liquidAppSettings->value(SETTINGS_KEY_USER_AGENT).toString());
        }

        // Custom user-defined CSS (does not require JavascriptEnabled in order to work)
        if (liquidAppSettings->contains(SETTINGS_KEY_CUSTOM_CSS)) {
            QString b64data = liquidAppSettings->value(SETTINGS_KEY_CUSTOM_CSS).toString().toUtf8().toBase64();
            QString cssDataURI = "data:text/css;charset=utf-8;base64," + b64data;
            // This will likely not work if JS is disabled for this Liquid app
            QString js = QString("(function(){"\
                                 "    const styleEl = document.createElement('style');"\
                                 "    const cssTextNode = document.createTextNode('@import url(%1)');"\
                                 "    styleEl.appendChild(cssTextNode);"\
                                 "    document.head.appendChild(styleEl);"\
                                 "})();").arg(cssDataURI);
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

        // Catch loading's end
        connect(this, SIGNAL(loadFinished(bool)), SLOT(loadFinished(bool)));

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
    connect(reloadAction, SIGNAL(triggered()), this, SLOT(refresh()));
    // Connect "alternative reload" shortcut
    reloadAction2 = new QAction;
    reloadAction2->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RELOAD_2)));
    addAction(reloadAction2);
    connect(reloadAction2, SIGNAL(triggered()), this, SLOT(refresh()));

    // Connect "reset" shortcut
    resetAction = new QAction;
    resetAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RESET)));
    addAction(resetAction);
    connect(resetAction, SIGNAL(triggered()), this, SLOT(reset()));

    // Connect "fullscreen" shortcut
    fullScreenAction = new QAction;
    fullScreenAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE)));
    addAction(fullScreenAction);
    connect(fullScreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));
    // Connect "alternative fullscreen" shortcut
    fullScreenAction2 = new QAction;
    fullScreenAction2->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE_2)));
    addAction(fullScreenAction2);
    connect(fullScreenAction2, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));

    // Connect "fullscreen" exit shortcut
    fullScreenExitAction = new QAction;
    fullScreenExitAction->setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_EXIT)));
    addAction(fullScreenExitAction);
    connect(fullScreenExitAction, SIGNAL(triggered()), this, SLOT(exitFullScreen()));

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

void LiquidAppWindow::closeEvent(QCloseEvent *event)
{
    if (!isFullScreen()) {
        liquidAppSettings->setValue(SETTINGS_KEY_WINDOW_GEOMETRY, QString(liquidAppWindowGeometry.toHex()));
        liquidAppSettings->sync();
    }

    event->accept();
    deleteLater();
}

bool LiquidAppWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched->parent() == this) {
        switch (event->type()) {
            case QEvent::Wheel:
                if (handleWheelEvent(static_cast<QWheelEvent *>(event))) {
                    return true;
                }
                break;

            default:
                break;
        }
    }

    return QWebEngineView::eventFilter(watched, event);
}

void LiquidAppWindow::exitFullScreen()
{
    setWindowState(windowState() & ~Qt::WindowFullScreen);

    if (isWindowGeometryLocked) {
        // Pause here to wait for any kind of window resize animation to finish
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

bool LiquidAppWindow::handleWheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        (event->delta() > 0) ? zoomIn() : zoomOut();
        event->accept();
        return true;
    }

    return false;
}

void LiquidAppWindow::loadFinished(bool ok)
{
    if (ok) {
        isLoading = false;

        // Set window title
        if (!liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
            // TODO: deal with absent <TITLE> returning page's URL when title() is called
            updateWindowTitle(title());
        }
    }
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
    };

    for (int i = 0; i < webAttributeKeys.size(); i++) {
        webSettings->setAttribute(webAttributeKeys.at(i), defaultWebSettings->testAttribute(webAttributeKeys.at(i)));
    }
}

void LiquidAppWindow::refresh()
{
    isLoading = true;

    QString titleBeforeRefresh = title();

    if (!liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        updateWindowTitle(titleBeforeRefresh);
    }

    reload();
}

void LiquidAppWindow::reset()
{
    isLoading = true;

    // Synchronously wipe all document contents
    page()->runJavaScript("document.getElementsByTagName('html')[0].remove();", QWebEngineScript::ApplicationWorld);

    // Ensure that while it's being reset, the window title remains to be set to this Liquid application's name
    QString js = QString("const titleEl = document.createElement('title');"\
                         "titleEl.innerText='%1';"\
                         "document.appendChild(titleEl);").arg((*liquidAppName).replace("'", "\\'"));
    page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);

    if (!liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        liquidAppWindowTitle = *liquidAppName;
    }
    updateWindowTitle(liquidAppWindowTitle);

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

void LiquidAppWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        exitFullScreen();
    } else {
        // Make it temporarily possible to resize the window if geometry is locked
        if (isWindowGeometryLocked) {
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
        if (isWindowGeometryLocked) {
            // Open up resizing restrictions
            setMinimumSize(CONFIG_WIN_MINSIZE_W, CONFIG_WIN_MINSIZE_H);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            isWindowGeometryLocked = false;
        } else {
            // Lock down resizing
            setMinimumSize(width(), height());
            setMaximumSize(width(), height());
            isWindowGeometryLocked = true;
        }

        liquidAppSettings->setValue(SETTINGS_KEY_WINDOW_GEOMETRY_LOCKED, isWindowGeometryLocked);
        liquidAppSettings->sync();
    }

    updateWindowTitle(title());
}

void LiquidAppWindow::updateWindowTitle(QString title)
{
    QString textIcons;

    if (liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        title = liquidAppSettings->value(SETTINGS_KEY_TITLE).toString();
    }

    if (isLoading) {
        textIcons.append(ICON_LOADING);
    }
    if (isWindowGeometryLocked) {
        textIcons.append(ICON_LOCKED);
    }
    if (textIcons != "") {
        textIcons = " " + textIcons;
    }

    setWindowTitle(title + textIcons);
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
