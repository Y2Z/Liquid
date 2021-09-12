#include "config.h"
#include "liquidappcookiejar.hpp"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"

LiquidAppWindow::LiquidAppWindow(QWidget *parent) : QWebView(parent)
{
    setMinimumSize(CONFIG_LIQUID_APP_WIN_MINSIZE_W, CONFIG_LIQUID_APP_WIN_MINSIZE_H);

    // Disable context menu
    setContextMenuPolicy(Qt::PreventContextMenu);

    // Enable anti-aliasing and other eye candies
    setRenderHints(QPainter::Antialiasing
                  |QPainter::HighQualityAntialiasing
                  |QPainter::NonCosmeticDefaultPen
                  |QPainter::SmoothPixmapTransform
                  |QPainter::TextAntialiasing
                  );

    // Connect keyboard shortcuts
    bindShortcuts();

    // Trigger window title update if <title> changes
    connect(this, SIGNAL(titleChanged(QString)), SLOT(updateWindowTitle(QString)));

    // Catch loading's end
    connect(this, SIGNAL(loadFinished(bool)), SLOT(loadFinished(bool)));

    isLoading = true;
}

LiquidAppWindow::~LiquidAppWindow()
{
}

void LiquidAppWindow::bindShortcuts()
{
    // Connect the lock shortcut
    toggleGeometryLockAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_GEOMETRY_LOCK_TOGGLE)));
    addAction(&toggleGeometryLockAction);
    connect(&toggleGeometryLockAction, SIGNAL(triggered()), this, SLOT(toggleWindowGeometryLock()));

    // Connect the go-back shortcut
    backAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_BACK)));
    addAction(&backAction);
    connect(&backAction, SIGNAL(triggered()), this, SLOT(back()));

    // Connect the go-back shortcut (backspace)
    backAction2.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_BACK_2)));
    addAction(&backAction2);
    connect(&backAction2, SIGNAL(triggered()), this, SLOT(back()));

    // Connect the go-forward shortcut
    forwardAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_FORWARD)));
    addAction(&forwardAction);
    connect(&forwardAction, SIGNAL(triggered()), this, SLOT(forward()));

    // Connect the reload shortcut
    reloadAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RELOAD)));
    addAction(&reloadAction);
    connect(&reloadAction, SIGNAL(triggered()), this, SLOT(refresh()));
    // Connect the alternative reload shortcut
    reloadAction2.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RELOAD_2)));
    addAction(&reloadAction2);
    connect(&reloadAction2, SIGNAL(triggered()), this, SLOT(refresh()));

    // Connect the reset shortcut
    resetAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RESET)));
    addAction(&resetAction);
    connect(&resetAction, SIGNAL(triggered()), this, SLOT(reset()));

    // Connect the fullscreen shortcut
    fullScreenAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE)));
    addAction(&fullScreenAction);
    connect(&fullScreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));
    // Connect the alternative fullscreen shortcut
    fullScreenAction2.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE_2)));
    addAction(&fullScreenAction2);
    connect(&fullScreenAction2, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));

    // Connect the fullscreen exit shortcut
    fullScreenExitAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_EXIT)));
    addAction(&fullScreenExitAction);
    connect(&fullScreenExitAction, SIGNAL(triggered()), this, SLOT(exitFullScreen()));

    // Connect the zoom-in shortcut
    zoomInAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_IN)));
    addAction(&zoomInAction);
    connect(&zoomInAction, SIGNAL(triggered()), this, SLOT(zoomIn()));

    // Connect the zoom-out shortcut
    zoomOutAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_OUT)));
    addAction(&zoomOutAction);
    connect(&zoomOutAction, SIGNAL(triggered()), this, SLOT(zoomOut()));

    // Connect the reset-zoom shortcut
    zoomResetAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_RESET)));
    addAction(&zoomResetAction);
    connect(&zoomResetAction, SIGNAL(triggered()), this, SLOT(zoomReset()));

    // Connect the exit shortcut
    quitAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_QUIT)));
    addAction(&quitAction);
    connect(&quitAction, SIGNAL(triggered()), this, SLOT(close()));

    // Connect the alternative exit shortcut
    quitAction2.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_LIQUID_APP_QUIT_2)));
    addAction(&quitAction2);
    connect(&quitAction2, SIGNAL(triggered()), this, SLOT(close()));
}

void LiquidAppWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void LiquidAppWindow::exitFullScreen()
{
    setWindowState(windowState() & ~Qt::WindowFullScreen);
    if (isWindowGeometryLocked) {
        // Pause here
        {
            QTime proceedAfter = QTime::currentTime().addMSecs(200);
            while (QTime::currentTime() < proceedAfter) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
        }

        // isWindowGeometryLocked = false;
        // toggleWindowGeometryLock();
        setMinimumSize(width(), height());
        setMaximumSize(width(), height());
    }
}

void LiquidAppWindow::loadFinished(bool ok)
{
    if (ok) {
        isLoading = false;

        // Set window title
        if (!liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
            updateWindowTitle(title());
        }

        // Inject additional JS
        if (liquidAppSettings->contains(SETTINGS_KEY_CUSTOM_JS)) {
            QString js = liquidAppSettings->value(SETTINGS_KEY_CUSTOM_JS).toString();

            page()->mainFrame()->evaluateJavaScript(js);
        }
    }
}

void LiquidAppWindow::moveEvent(QMoveEvent *event)
{
    // Remember window position
    liquidAppSettings->setValue(SETTINGS_KEY_WINDOW_GEOMETRY, QString(saveGeometry().toHex()));

    QWebView::moveEvent(event);
}

void LiquidAppWindow::refresh()
{
    isLoading = true;

    // Wipe all HTML but title
    setHtml("<title>" + appWindowTitle + "</title>", url());
    if (!liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        updateWindowTitle(appWindowTitle);
    }

    reload();
}

void LiquidAppWindow::reset()
{
    QUrl url(liquidAppSettings->value(SETTINGS_KEY_URL).toString(), QUrl::StrictMode);

    isLoading = true;

    if (!liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        appWindowTitle = *liquidAppName;
    }
    updateWindowTitle(appWindowTitle);

    setUrl(url);
}

void LiquidAppWindow::resizeEvent(QResizeEvent *event)
{
    // Remember window size (unless in full-screen mode)
    if (!isFullScreen()) {
        liquidAppSettings->setValue(SETTINGS_KEY_WINDOW_GEOMETRY, QString(saveGeometry().toHex()));
    }

    QWebView::resizeEvent(event);
}

void LiquidAppWindow::runLiquidApp(QString *name)
{
    liquidAppSettings = new QSettings(QSettings::IniFormat,
                                QSettings::UserScope,
                                CONFIG_APPS_PATH,
                                *name,
                                nullptr);

    liquidAppName = name;
    appWindowTitle = *liquidAppName;

    // Set custom user-agent HTTP header and handle page links
    setPage(new LiquidAppWebPage(this));

    QWebSettings *globalWebSettings = QWebSettings::globalSettings();

    // Default web settings
    globalWebSettings->setAttribute(QWebSettings::DnsPrefetchEnabled, true);
    globalWebSettings->setAttribute(QWebSettings::PrivateBrowsingEnabled, true);
    globalWebSettings->setAttribute(QWebSettings::AutoLoadImages, true);
    globalWebSettings->setAttribute(QWebSettings::DeveloperExtrasEnabled, false);
    globalWebSettings->setAttribute(QWebSettings::PluginsEnabled, false);
    globalWebSettings->setAttribute(QWebSettings::JavaEnabled, false);
    globalWebSettings->setAttribute(QWebSettings::LocalStorageEnabled, true);
    globalWebSettings->setAttribute(QWebSettings::LocalStorageDatabaseEnabled, false);
    globalWebSettings->setAttribute(QWebSettings::OfflineStorageDatabaseEnabled, false);
    globalWebSettings->setAttribute(QWebSettings::OfflineWebApplicationCacheEnabled, false);
    //globalWebSettings->setAttribute(QWebSettings::FrameFlatteningEnabled, true);
    globalWebSettings->setAttribute(QWebSettings::SiteSpecificQuirksEnabled, false);
    globalWebSettings->setAttribute(QWebSettings::ScrollAnimatorEnabled, true);
    globalWebSettings->setAttribute(QWebSettings::HyperlinkAuditingEnabled, false);
    globalWebSettings->setAttribute(QWebSettings::WebAudioEnabled, true);
    globalWebSettings->setThirdPartyCookiePolicy(QWebSettings::AlwaysBlockThirdPartyCookies);
    globalWebSettings->setAttribute(QWebSettings::JavascriptEnabled, false);

    // Permanently disabled
    globalWebSettings->setAttribute(QWebSettings::JavascriptCanOpenWindows, false);
    globalWebSettings->setAttribute(QWebSettings::JavascriptCanCloseWindows, false);
    globalWebSettings->setAttribute(QWebSettings::JavascriptCanAccessClipboard, false);

    // Set custom window title
    if (liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        appWindowTitle = liquidAppSettings->value(SETTINGS_KEY_TITLE).toString();
    }
    updateWindowTitle(appWindowTitle);

    QUrl url(liquidAppSettings->value(SETTINGS_KEY_URL).toString());
    if (url.isValid()) {
        // Set the base background color
        QPalette palette;
        QColor background;
        if (liquidAppSettings->contains(SETTINGS_KEY_BACKGROUND_COLOR)) {
            background = QColor(liquidAppSettings->value(SETTINGS_KEY_BACKGROUND_COLOR).toString());
        } else {
            background = Qt::black;
        }
        palette.setColor(QPalette::Base, background);
        setPalette(palette);

        // Deal with Cookies
        if (liquidAppSettings->value(SETTINGS_KEY_ALLOW_COOKIES).toBool()) {
            // Determine third-party Cookie policy
            if (liquidAppSettings->value(SETTINGS_KEY_ALLOW_THIRD_PARTY_COOKIES).toBool()) {
                globalWebSettings->setThirdPartyCookiePolicy(
                    QWebSettings::AlwaysAllowThirdPartyCookies);
            }

            // Set up our custom cookie jar
            page()->networkAccessManager()->setCookieJar(new LiquidAppCookieJar(this));
        }

        // Toggle JavaScript on if allowed
        if (liquidAppSettings->value(SETTINGS_KEY_ENABLE_JS).toBool()) {
            globalWebSettings->setAttribute(QWebSettings::JavascriptEnabled, true);
        }

        // Restore the window geometry
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

        // Custom user-defined CSS
        if (liquidAppSettings->contains(SETTINGS_KEY_CUSTOM_CSS)) {
            QString b64data = liquidAppSettings->value(SETTINGS_KEY_CUSTOM_CSS)
                                                 .toString().toUtf8().toBase64();
            QString cssDataURI ="data:text/css;charset=utf-8;base64," + b64data;
            globalWebSettings->setUserStyleSheetUrl(cssDataURI);
        }

        // Load the starting URL
        load(url);

        // Reveal the app window
        show();
        raise();
        activateWindow();
    } else {
        qDebug() << "Invalid Liquid application URL:" << url;
    }
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
    }

    updateWindowTitle(title());
}

void LiquidAppWindow::updateWindowTitle(QString title)
{
    QString flags;

    if (liquidAppSettings->contains(SETTINGS_KEY_TITLE)) {
        title = liquidAppSettings->value(SETTINGS_KEY_TITLE).toString();
    }

    if (isLoading) {
        flags.append(ICON_LOADING);
    }

    if (isWindowGeometryLocked) {
        flags.append(ICON_LOCKED);
    }

    if (flags != "") {
        flags = " " + flags;
    }

    setWindowTitle(title + flags);
}

void LiquidAppWindow::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        (event->delta() > 0) ? zoomIn() : zoomOut();
        event->accept();
        return;
    }

    QWebView::wheelEvent(event);
}

void LiquidAppWindow::zoomBy(qreal factor)
{
    qreal newZoomFactor = zoomFactor() + factor;

    if (factor) {
        if (newZoomFactor < CONFIG_ZOOM_MIN) {
            newZoomFactor = CONFIG_ZOOM_MIN;
        }
        else if (newZoomFactor > CONFIG_ZOOM_MAX) {
            newZoomFactor = CONFIG_ZOOM_MAX;
        }
    } else {
        newZoomFactor = 1;
    }

    setZoomFactor(newZoomFactor);

    liquidAppSettings->setValue(SETTINGS_KEY_ZOOM, newZoomFactor);
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
