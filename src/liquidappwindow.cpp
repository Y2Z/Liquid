#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QClipboard>
#include <QNetworkProxy>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWebEngineHistory>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include "liquid.hpp"
#include "liquidappcookiejar.hpp"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"
#include "lqd.h"

LiquidAppWindow::LiquidAppWindow(const QString* name) : QWebEngineView()
{
    liquidAppName = (QString*)name;

    {
        QString instanceName = QApplication::applicationName() + "_" + liquidAppName;
        singleInstance = new SingleInstance((QWidget*)this, &instanceName);
    }
}

LiquidAppWindow::~LiquidAppWindow(void)
{
    saveLiquidAppConfig();

    delete liquidAppWebPage;
    delete liquidAppWebProfile;
}

void LiquidAppWindow::attemptToSetZoomFactorTo(const qreal desiredZoomFactor)
{
    int i = 0;
    const int ilen = zoomFactors.size();

    for (; i < ilen; i++) {
        if (qFuzzyCompare(zoomFactors[i], desiredZoomFactor)) {
            setZoomFactor(zoomFactors[i]);
            return;
        }
    }

    // Attempt to determine closest zoom level to snap to
    for (i = 0; i < ilen; i++) {
        if ((i == 0 || zoomFactors[i - 1] < desiredZoomFactor) && (i == ilen - 1 || zoomFactors[i + 1] > desiredZoomFactor)) {
            setZoomFactor(zoomFactors[i]);
            return;
        }
    }
}

void LiquidAppWindow::bindKeyboardShortcuts(void)
{
    // Connect window geometry lock shortcut
    toggleGeometryLockAction = new QAction;
    toggleGeometryLockAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_TOGGLE_WIN_GEOM_LOCK)));
    addAction(toggleGeometryLockAction);
    connect(toggleGeometryLockAction, SIGNAL(triggered()), this, SLOT(toggleWindowGeometryLock()));

    // Connect "mute audio" shortcut
    muteAudioAction = new QAction;
    muteAudioAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_MUTE_AUDIO)));
    addAction(muteAudioAction);
    connect(muteAudioAction, &QAction::triggered, this, [this](){
        page()->setAudioMuted(!page()->isAudioMuted());
        updateWindowTitle(title());
    });

    // Connect "go back" shortcut
    backAction = new QAction;
    backAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_GO_BACK)));
    addAction(backAction);
    connect(backAction, SIGNAL(triggered()), this, SLOT(back()));

    // Connect "go back" shortcut (backspace)
    backAction2 = new QAction;
    backAction2->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_GO_BACK_2)));
    addAction(backAction2);
    connect(backAction2, SIGNAL(triggered()), this, SLOT(back()));

    // Connect "go forward" shortcut
    forwardAction = new QAction;
    forwardAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_GO_FORWARD)));
    addAction(forwardAction);
    connect(forwardAction, SIGNAL(triggered()), this, SLOT(forward()));

    // Connect "reload" shortcut
    reloadAction = new QAction;
    reloadAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_RELOAD)));
    addAction(reloadAction);
    connect(reloadAction, SIGNAL(triggered()), this, SLOT(reload()));
    // Connect "alternative reload" shortcut (there can be only one QKeySequence per QAction)
    reloadAction2 = new QAction;
    reloadAction2->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_RELOAD_2)));
    addAction(reloadAction2);
    connect(reloadAction2, SIGNAL(triggered()), this, SLOT(reload()));

    // Connect "hard reload" shortcut
    hardReloadAction = new QAction;
    hardReloadAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_HARD_RELOAD)));
    addAction(hardReloadAction);
    connect(hardReloadAction, SIGNAL(triggered()), this, SLOT(hardReload()));

    // Connect "toggle full-screen" shortcut
    toggleFullScreenModeAction = new QAction;
    toggleFullScreenModeAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_TOGGLE_FS_MODE)));
    addAction(toggleFullScreenModeAction);
    connect(toggleFullScreenModeAction, SIGNAL(triggered()), this, SLOT(toggleFullScreenMode()));
    // Connect "alternative toggle full-screen" shortcut (there can be only one QKeySequence per QAction)
    toggleFullScreenModeAction2 = new QAction;
    toggleFullScreenModeAction2->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_TOGGLE_FS_MODE_2)));
    addAction(toggleFullScreenModeAction2);
    connect(toggleFullScreenModeAction2, SIGNAL(triggered()), this, SLOT(toggleFullScreenMode()));

    // Connect "stop loading" / "exit full-screen mode" shortcut
    stopLoadingOrExitFullScreenModeAction = new QAction;
    stopLoadingOrExitFullScreenModeAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_STOP_OR_EXIT_FS_MODE)));
    addAction(stopLoadingOrExitFullScreenModeAction);
    connect(stopLoadingOrExitFullScreenModeAction, SIGNAL(triggered()), this, SLOT(stopLoadingOrExitFullScreenMode()));

    // Connect "zoom in" shortcut
    zoomInAction = new QAction;
    zoomInAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_ZOOM_LVL_INC)));
    addAction(zoomInAction);
    connect(zoomInAction, &QAction::triggered, this, [this](){
        zoomIn(false);
    });

    // Connect "zoom out" shortcut
    zoomOutAction = new QAction;
    zoomOutAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_ZOOM_LVL_DEC)));
    addAction(zoomOutAction);
    connect(zoomOutAction, &QAction::triggered, this, [this](){
        zoomOut(false);
    });

    // Connect "fine zoom in" shortcut
    zoomInFineAction = new QAction;
    zoomInFineAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_ZOOM_LVL_INC_FINE)));
    addAction(zoomInFineAction);
    connect(zoomInFineAction, &QAction::triggered, this, [this](){
        zoomIn(true);
    });

    // Connect "fine zoom out" shortcut
    zoomOutFineAction = new QAction;
    zoomOutFineAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_ZOOM_LVL_DEC_FINE)));
    addAction(zoomOutFineAction);
    connect(zoomOutFineAction, &QAction::triggered, this, [this](){
        zoomOut(true);
    });

    // Connect "reset zoom" shortcut
    zoomResetAction = new QAction;
    zoomResetAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_ZOOM_LVL_RESET)));
    addAction(zoomResetAction);
    connect(zoomResetAction, SIGNAL(triggered()), this, SLOT(zoomReset()));

    // Connect "alternative reset zoom" shortcut
    zoomResetAltAction = new QAction;
    zoomResetAltAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_ZOOM_LVL_RESET_2)));
    addAction(zoomResetAltAction);
    connect(zoomResetAltAction, SIGNAL(triggered()), this, SLOT(zoomReset()));

    // Connect "exit" shortcut
    quitAction = new QAction;
    quitAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_QUIT)));
    addAction(quitAction);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));

    // Connect "alternative exit" shortcut
    quitAction2 = new QAction;
    quitAction2->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_QUIT_2)));
    addAction(quitAction2);
    connect(quitAction2, SIGNAL(triggered()), this, SLOT(close()));

    // Connect "take snapshot" shortcut
    takeSnapshotAction = new QAction;
    takeSnapshotAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_TAKE_SNAPSHOT)));
    addAction(takeSnapshotAction);
    connect(takeSnapshotAction, SIGNAL(triggered()), this, SLOT(takeSnapshotSlot()));

    // Connect "take full-page snapshot" shortcut
    takeSnapshotFullPageAction = new QAction;
    takeSnapshotFullPageAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_TAKE_SNAPSHOT_FULL)));
    addAction(takeSnapshotFullPageAction);
    connect(takeSnapshotFullPageAction, SIGNAL(triggered()), this, SLOT(takeSnapshotFullPageSlot()));

    // Connect "save page" shortcut
    savePageAction = new QAction;
    savePageAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_SAVE_PAGE)));
    addAction(savePageAction);
    connect(savePageAction, &QAction::triggered, this, [this](){
        // TODO: make it save the page along with its custom CSS/JS, as one monolithic HTML file (instead of MHT)
        page()->save(QString("%1 (%2).mhtml").arg(*liquidAppName).arg(Liquid::getReadableDateTimeString()));
    });

    // Make it possible to intercept zoom events
    QApplication::instance()->installEventFilter(this);
}

void LiquidAppWindow::certificateError(void)
{
    const bool updateTitle = !pageHasCertificateError;

    pageHasCertificateError = true;

    if (updateTitle) {
        updateWindowTitle(title());
    }
}

void LiquidAppWindow::closeEvent(QCloseEvent* event)
{
    event->accept();
    deleteLater();
}

const QString LiquidAppWindow::colorToRgba(const QColor color)
{
    return QString("rgba(%1, %2, %3, %4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alphaF());
}

void LiquidAppWindow::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event);

    contextMenuBackAction->setEnabled(history()->canGoBack());
    contextMenuForwardAction->setEnabled(history()->canGoForward());

    contextMenu->exec(QCursor::pos());
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

            case QEvent::MouseButtonPress:
                liquidAppWebPage->closeJsDialog();
                break;

            default:
                break;
        }
    }

    return QWebEngineView::eventFilter(watched, event);
}

void LiquidAppWindow::exitFullScreenMode(void)
{
    // Exit from full-screen mode
    setWindowState(windowState() & ~Qt::WindowFullScreen);

    if (windowGeometryIsLocked) {
        // Pause here to wait for any kind of window resize animations to finish
        Liquid::sleep(200);

        setMinimumSize(width(), height());
        setMaximumSize(width(), height());
    }
}

bool LiquidAppWindow::handleWheelEvent(QWheelEvent *event)
{
    const bool isCtrlActive = event->modifiers() & Qt::ControlModifier;
    const bool isShiftActive = event->modifiers() & Qt::ShiftModifier;

    if (isCtrlActive) {
        (event->inverted()) ? zoomIn(isShiftActive) : zoomOut(isShiftActive);
        event->accept();
        return true;
    }

    return false;
}

void LiquidAppWindow::hardReload(void)
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

    // Ensure that while this Liquid App is being reset, the window title remains to be set to this Liquid App's name
    // to mimic the same experience that happens when the user initially runs this Liquid app
    if (!liquidAppWindowTitleIsReadOnly) {
        liquidAppWindowTitle = *liquidAppName;

        const QString js = QString("(()=>{"\
                                       "let e=document.createElement('title');"\
                                       "e.innerText='%1';"\
                                       "document.appendChild(e)"\
                                   "})()").arg(liquidAppWindowTitle.replace("'", "\\'"));
        page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);
    }

    updateWindowTitle(title());

    // TODO: reset localStorage / Cookies in case they're disabled?

    // TODO: clear any type of cache, if possible

    QUrl url(liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString(), QUrl::StrictMode);
    setUrl(url);
}

bool LiquidAppWindow::isAlreadyRunning(void)
{
    const bool raiseExisting = true;
    const bool isAnotherInstanceRunning = singleInstance->isAlreadyRunning(raiseExisting);

    return isAnotherInstanceRunning;
}

void LiquidAppWindow::loadFinished(bool ok)
{
    pageIsLoading = false;

    if (ok) {
        pageHasError = false;
    } else {
        if (forgiveNextPageLoadError) {
            pageHasError = false;
        } else {
            pageHasError = true;
        }
    }

    // Unset forgiveNextPageLoadError
    if (forgiveNextPageLoadError) {
        forgiveNextPageLoadError = false;
    }

    updateWindowTitle(title());
}

void LiquidAppWindow::loadLiquidAppConfig(void)
{
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_TITLE)) {
        liquidAppWindowTitle = liquidAppConfig->value(LQD_CFG_KEY_NAME_TITLE).toString();
        // Make sure the window title never gets changed
        liquidAppWindowTitleIsReadOnly = true;
    }

    // Apply network proxy configuration
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_USE_PROXY)) {
        proxy = new QNetworkProxy;

        if (liquidAppConfig->value(LQD_CFG_KEY_NAME_USE_PROXY, false).toBool()) {
            const bool isSocks = liquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USE_SOCKS).toBool();

            proxy->setType((isSocks) ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy);

            if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_HOST)) {
                proxy->setHostName(liquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_HOST).toString());
            }

            if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_PORT)) {
                proxy->setPort(liquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_PORT).toInt());
            }

            if (liquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USE_AUTH, false).toBool()) {
                if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_NAME)) {
                    proxy->setUser(liquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_NAME).toString());
                }

                if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD)) {
                    proxy->setPassword(liquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD).toString());
                }
            }
        } else {
            proxy->setType(QNetworkProxy::NoProxy);
        }

        QNetworkProxy::setApplicationProxy(*proxy);
    }

    // Remove window manager's frame
    {
        if (liquidAppConfig->value(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME, false).toBool()) {
            setWindowFlags(Qt::FramelessWindowHint);
        }
    }

    // Set the page's background color behind the document's body
    {
        if (liquidAppConfig->value(LQD_CFG_KEY_NAME_USE_CUSTOM_BG, false).toBool() && liquidAppConfig->contains(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR)) {
            const QColor backgroundColor = QColor(liquidAppConfig->value(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR).toString());

            if (backgroundColor.alpha() < 255) {
                // Make window background transparent
                setAttribute(Qt::WA_TranslucentBackground);
            }

            page()->setBackgroundColor(backgroundColor);
        } else {
            page()->setBackgroundColor(LQD_DEFAULT_BG_COLOR);
        }
    }

    // Determine where this Liquid app is allowed to navigate, and what should be opened in external browser
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS)) {
        liquidAppWebPage->addAllowedDomains(
            liquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS).toString().split(" ")
        );
    }

    // Deal with Cookies
    {
        LiquidAppCookieJar *liquidAppCookieJar = new LiquidAppCookieJar(this);
        QWebEngineCookieStore *cookieStore = page()->profile()->cookieStore();

        connect(cookieStore, &QWebEngineCookieStore::cookieAdded, liquidAppCookieJar, &LiquidAppCookieJar::upsertCookie);
        connect(cookieStore, &QWebEngineCookieStore::cookieRemoved, liquidAppCookieJar, &LiquidAppCookieJar::removeCookie);

        liquidAppCookieJar->restoreCookies(cookieStore);
    }

    // Restore window geometry
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_WIN_GEOM)) {
        restoreGeometry(QByteArray::fromHex(
            liquidAppConfig->value(LQD_CFG_KEY_NAME_WIN_GEOM).toByteArray()
        ));
    } else {
        const QRect currentScreenSize = QGuiApplication::primaryScreen()->availableGeometry();
        const int currentScreenWidth = currentScreenSize.width();
        const int currentScreenHeight = currentScreenSize.height();
        setGeometry(currentScreenWidth / 4, currentScreenHeight / 4, currentScreenWidth / 2, currentScreenHeight / 2);
    }

    // Toggle JavaScript on if enabled in application config
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ENABLE_JS)) {
        settings()->setAttribute(
            QWebEngineSettings::JavascriptEnabled,
            liquidAppConfig->value(LQD_CFG_KEY_NAME_ENABLE_JS).toBool()
        );
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    // Hide scrollbars
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS)) {
        settings()->setAttribute(
            QWebEngineSettings::ShowScrollBars,
            !liquidAppConfig->value(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS).toBool()
        );
    }
#endif

    // Mute audio if muted in application config
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_MUTE_AUDIO)) {
        page()->setAudioMuted(liquidAppConfig->value(LQD_CFG_KEY_NAME_MUTE_AUDIO).toBool());
    }

    // Restore web view zoom level
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ZOOM_LVL)) {
        attemptToSetZoomFactorTo(liquidAppConfig->value(LQD_CFG_KEY_NAME_ZOOM_LVL).toDouble());

        // There's a bug in Qt, using QTimer seems to be the only solution
        QTimer::singleShot(1000, [&](){
            if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ZOOM_LVL)) {
                attemptToSetZoomFactorTo(liquidAppConfig->value(LQD_CFG_KEY_NAME_ZOOM_LVL).toDouble());
            }
        });
    }

    // Lock for the app's window's geometry
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_LOCK_WIN_GEOM)) {
        if (liquidAppConfig->value(LQD_CFG_KEY_NAME_LOCK_WIN_GEOM).toBool()) {
            toggleWindowGeometryLock();
            windowGeometryIsLocked = true;
        }
    }

    // Custom user-agent string
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_USER_AGENT)) {
        liquidAppWebProfile->setHttpUserAgent(liquidAppConfig->value(LQD_CFG_KEY_NAME_USER_AGENT).toString());
    }

    // Additional user-defined CSS (does't require JavaScript enabled in order to work)
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_CSS)) {
        QString additionalCss = liquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_CSS).toString();
        const QString js = QString("(()=>{"\
                                       "const styleEl = document.createElement('style');"\
                                       "const cssTextNode = document.createTextNode('%1');"\
                                       "styleEl.appendChild(cssTextNode);"\
                                       "document.head.appendChild(styleEl)"\
                                   "})()").arg(additionalCss.replace("\n", " ").replace("'", "\\'"));
        QWebEngineScript script;
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setRunsOnSubFrames(false);
        script.setSourceCode(js);
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        liquidAppWebPage->scripts().insert(script);
    }

    // Additional user-defined JS (does't require JavaScript enabled in order to work)
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_JS)) {
        QString js = liquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_JS).toString();
        QWebEngineScript script;
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setRunsOnSubFrames(false);
        script.setSourceCode(js);
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        liquidAppWebPage->scripts().insert(script);
    }

#if !defined(Q_OS_LINUX) // This doesn't work on X11
    // Set window icon
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ICON)) {
        QPixmap pixmap;
        pixmap.loadFromData(QByteArray::fromBase64(liquidAppConfig->value(LQD_CFG_KEY_NAME_ICON).toByteArray().remove(0, 22)), "PNG");
        window()->setWindowIcon(QIcon(pixmap));
    }
#endif
}

void LiquidAppWindow::loadStarted(void)
{
    pageIsLoading = true;
    pageHasCertificateError = false;
    pageHasError = false;

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

    iconToSave = icon;
}

void LiquidAppWindow::resizeEvent(QResizeEvent* event)
{
    // Remember window size (unless in full-screen mode)
    if (event->spontaneous() && !isFullScreen()) {
        // Pause here to wait for any kind of window resize animations to finish
        Liquid::sleep(200);

        liquidAppWindowGeometry = saveGeometry();
    }

    QWebEngineView::resizeEvent(event);
}

void LiquidAppWindow::run(void)
{
    // Prevent window from getting way too tiny
    setMinimumSize(LQD_APP_WIN_MIN_SIZE_W, LQD_APP_WIN_MIN_SIZE_H);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // Ensure dark mode is enabled on the web page in case the system theme is dark
    if (Liquid::detectDarkMode()) {
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--force-dark-mode --blink-settings=darkMode=4 --blink-settings=forceDarkModeEnabled=true --blink-settings=darkModeEnabled=true");
    }
#endif

    // Set default icon
#if !defined(Q_OS_LINUX) // This doesn't work on X11
    setWindowIcon(QIcon(":/images/" PROG_NAME ".svg"));
#endif

    // Disable default QWebEngineView's context menu
    setContextMenuPolicy(Qt::PreventContextMenu);

    liquidAppConfig = new QSettings(QSettings::IniFormat,
                                    QSettings::UserScope,
                                    QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                    *liquidAppName,
                                    Q_NULLPTR);

    // These default settings affect everything (including sub-frames)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    LiquidAppWebPage::setWebSettingsToDefault(QWebEngineSettings::globalSettings());
#endif

    liquidAppWebProfile = new QWebEngineProfile(QString(), this);
    liquidAppWebProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    liquidAppWebProfile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    if (!liquidAppWebProfile->isOffTheRecord()) {
        qDebug().noquote() << "Web profile is not off-the-record!";
        // Privacy is paramount for this program, separate apps need to be completely siloed
        exit(EXIT_FAILURE);
    }

    Liquid::applyQtStyleSheets(this);

    liquidAppWebPage = new LiquidAppWebPage(liquidAppWebProfile, this);
    setPage(liquidAppWebPage);

    liquidAppWebSettings = liquidAppWebPage->settings();

    // Set default window title
    liquidAppWindowTitle = *liquidAppName;

    updateWindowTitle(*liquidAppName);

    // Pre-fill all possible zoom factors to snap desired zoom level to
    {
        for (qreal z = 1.0 - LQD_ZOOM_LVL_STEP_FINE; z >= LQD_ZOOM_LVL_MIN - LQD_ZOOM_LVL_STEP_FINE && z > 0; z -= LQD_ZOOM_LVL_STEP_FINE) {
            if (z >= LQD_ZOOM_LVL_MIN) {
                zoomFactors.prepend(z);
            } else {
                zoomFactors.prepend(LQD_ZOOM_LVL_MIN);
            }
        }

        if (LQD_ZOOM_LVL_MIN <= 1 && LQD_ZOOM_LVL_MAX >= 1) {
            zoomFactors.append(1.0);
        }

        for (qreal z = 1.0 + LQD_ZOOM_LVL_STEP_FINE; z <= LQD_ZOOM_LVL_MAX + LQD_ZOOM_LVL_STEP_FINE; z += LQD_ZOOM_LVL_STEP_FINE) {
            if (z <= LQD_ZOOM_LVL_MAX) {
                zoomFactors.append(z);
            } else {
                zoomFactors.append(LQD_ZOOM_LVL_MAX);
            }
        }
    }

    const QUrl startingUrl(liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString());

    if (!startingUrl.isValid()) {
        qDebug().noquote() << "Invalid Liquid application URL:" << startingUrl;
        return;
    }

    liquidAppWebPage->addAllowedDomain(startingUrl.host());

    loadLiquidAppConfig();

    // Connect keyboard shortcuts
    bindKeyboardShortcuts();

    // Initialize context menu
    setupContextMenu();

    // Trigger window title update if <title> changes
    connect(this, &QWebEngineView::titleChanged, this, &LiquidAppWindow::updateWindowTitle);

    // Update Liquid app's icon using the one provided by the website
    connect(liquidAppWebPage, &QWebEnginePage::iconChanged, this, &LiquidAppWindow::onIconChanged);

    // Catch loading's start
    connect(liquidAppWebPage, &QWebEnginePage::loadStarted, this, &LiquidAppWindow::loadStarted);

    // Catch loading's end
    connect(liquidAppWebPage, &QWebEnginePage::loadFinished, this, &LiquidAppWindow::loadFinished);

    // Reveal Liquid app's window
    show();
    raise();
    activateWindow();

    // Load Liquid app's starting URL
    load(startingUrl);
}

void LiquidAppWindow::saveLiquidAppConfig(void)
{
    if (qFuzzyCompare(zoomFactor(), 1.0)) {
        if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ZOOM_LVL)) {
            liquidAppConfig->remove(LQD_CFG_KEY_NAME_ZOOM_LVL);
        }
    } else {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_ZOOM_LVL, zoomFactor());
    }

    if (page()->isAudioMuted()) {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_MUTE_AUDIO, true);
    } else {
        if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_MUTE_AUDIO)) {
            liquidAppConfig->remove(LQD_CFG_KEY_NAME_MUTE_AUDIO);
        }
    }

    // Save icon data as base64 string
    {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        iconToSave.pixmap(iconToSave.availableSizes()[0]).save(&buffer, "PNG");
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_ICON, QString("data:image/png;base64,%1").arg(QString(buffer.data().toBase64())));
    }

    if (!isFullScreen()) {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_WIN_GEOM, QString(liquidAppWindowGeometry.toHex()));
    }

    if (windowGeometryIsLocked) {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_LOCK_WIN_GEOM, true);
    } else {
        if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_LOCK_WIN_GEOM)) {
            liquidAppConfig->remove(LQD_CFG_KEY_NAME_LOCK_WIN_GEOM);
        }
    }

    liquidAppConfig->sync();
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

void LiquidAppWindow::setForgiveNextPageLoadError(const bool ok)
{
    forgiveNextPageLoadError = ok;
}

void LiquidAppWindow::stopLoadingOrExitFullScreenMode(void)
{
    if (pageIsLoading) {
        triggerPageAction(QWebEnginePage::Stop);
    } else {
        exitFullScreenMode();
    }
}

void LiquidAppWindow::takeSnapshotSlot(void)
{
    takeSnapshot(false);
}
void LiquidAppWindow::takeSnapshotFullPageSlot(void)
{
    takeSnapshot(true);
}

void LiquidAppWindow::takeSnapshot(const bool fullPage)
{
    const bool vector = false; // NOTE: experimental feature
    const int ratio = QPaintDevice::devicePixelRatio();
    const QSize snapshotSize = (fullPage) ? (page()->contentsSize().toSize() / ratio) : contentsRect().size();

    // Ensure the target directory exists
    const QString path = QDir::homePath() + QDir::separator() + "Pictures";
    {
        QDir dir(path);
        if (!dir.exists()) {
            dir.mkdir(".");
        }
    }
    // Compose snapshot file name
    const QString fileName = QString("%1 %2 (%3)")
                                .arg(*liquidAppName)
                                .arg(tr((fullPage) ? "full-page snapshot" : "snapshot"))
                                .arg(Liquid::getReadableDateTimeString());

    if (vector) {
        QFile jsFile(":/js/html2svg.js");
        jsFile.open(QIODevice::ReadOnly);
        auto data = jsFile.readAll();
        const QString js = QString(data)
                            .arg(snapshotSize.width())
                            .arg(snapshotSize.height())
                            .arg(colorToRgba(page()->backgroundColor()))
                            .arg(fullPage);

        qDebug().noquote() << js;

        page()->runJavaScript(QString("(()=>{%1})()").arg(js), QWebEngineScript::ApplicationWorld, [path, fileName](const QVariant& res){
            qDebug().noquote() << res.toString();

            // Save vector image to disk
            {
                QFile file(path + QDir::separator() + fileName + ".svg");
                if (file.open(QIODevice::ReadWrite)) {
                    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                    stream << res.toString() << Qt::endl;
#else
                    stream << res.toString() << endl;
#endif
                }
            }
        });
    } else {
        const QImage::Format format = QImage::Format_ARGB32;
        QImage* image = new QImage(snapshotSize * ratio, format);
        image->setDevicePixelRatio(ratio);
        image->fill(Qt::transparent);

        QPainter* painter = new QPainter(image);
        // TODO: make fonts appear less blurry (potentially use painter->scale(ratio, ratio)
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        painter->setRenderHint(QPainter::HighQualityAntialiasing);
        painter->setRenderHint(QPainter::NonCosmeticDefaultPen);
#endif

        if (fullPage) {
            const QSize origWindowSize = size();
            const bool wasFullScreen = isFullScreen();
            // Remember initial scroll position to be able to come back to it after the whole page is captured
            const QPointF origScrollPos = page()->scrollPosition() / ratio;

            // Resize the window to fit all of its content
            {
                setAttribute(Qt::WA_DontShowOnScreen, true);
                show();

                resize(snapshotSize);
            }

            // Render contents of QWidget into QPainter
            render(painter);

            // Restore the window back to be exactly how it was
            {
                // Resize back to what it was
                resize(origWindowSize);

                // Reveal the window
                hide();
                setAttribute(Qt::WA_DontShowOnScreen, false);
                show();

                // Restore window's full-screen mode
                if (wasFullScreen && !isFullScreen()) {
                    toggleFullScreenMode();
                }

                // Scroll the web view back to where it was before we started taking full-page snapshot
                static const QString js = "window.scrollTo(%1, %2);";
                page()->runJavaScript(QString(js).arg(origScrollPos.x()).arg(origScrollPos.y()), QWebEngineScript::ApplicationWorld);
            }
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
            const bool hadScrollBarsShown = liquidAppWebSettings->testAttribute(QWebEngineSettings::ShowScrollBars);

            if (hadScrollBarsShown) {
                // Hide scrollbars before taking snapshot
                liquidAppWebSettings->setAttribute(QWebEngineSettings::ShowScrollBars, false);

                // Wait a little bit for scrollbars to disappear
                Liquid::sleep(1500);

                // qDebug() << liquidAppWebPage->renderProcessPid; // Qt 5.15 could allow us to trigger immediate page re-render by sending signal to that process
                // liquidAppWebPage->setVisible(false); liquidAppWebPage->setVisible(true); // Try this out once Qt 5.14 is more widely available

                // This is another approach for temporarily hiding scrollbars
                // static const QString js = "'undefined' != typeof document.styleSheets && 0 < document.styleSheets.length && document.styleSheets[0].addRule('::-webkit-scrollbar', 'width: 0 !important; height: 0 !important', 0);";
                // page()->runJavaScript(QString(js), QWebEngineScript::ApplicationWorld, [&](const QVariant& res){
                //     // Liquid::sleep(100);
                // });
            }
#endif

            // Render contents of QWidget into QPainter
            render(painter);

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
            if (hadScrollBarsShown) {
                // Bring scrollbars back after taking snapshot
                liquidAppWebSettings->setAttribute(QWebEngineSettings::ShowScrollBars, true);
            }
#endif
        }

        painter->end();
        delete painter;

        // Save raster image to disk
        image->scaled(snapshotSize.width(), snapshotSize.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .save(path + QDir::separator() + fileName + ".png", "PNG");

        // TODO: add EXIF?

        delete image;
    }

    // TODO: add camera flash visual effect
    // TODO: add shutter sound
}

void LiquidAppWindow::toggleFullScreenMode(void)
{
    if (isFullScreen()) {
        exitFullScreenMode();
    } else {
        // Make it temporarily possible to resize the window if geometry is locked
        if (windowGeometryIsLocked) {
            setMinimumSize(LQD_APP_WIN_MIN_SIZE_W, LQD_APP_WIN_MIN_SIZE_H);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
        // Enter the full-screen mode
        setWindowState(windowState() | Qt::WindowFullScreen);
    }
}

void LiquidAppWindow::toggleWindowGeometryLock(void)
{
    // Prevent toggling window geometry lock while in full-screen mode
    if (!isFullScreen()) {
        if (windowGeometryIsLocked) {
            // Open up resizing restrictions
            setMinimumSize(LQD_APP_WIN_MIN_SIZE_W, LQD_APP_WIN_MIN_SIZE_H);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            windowGeometryIsLocked = false;
        } else {
            // Lock down resizing
            setMinimumSize(width(), height());
            setMaximumSize(width(), height());
            windowGeometryIsLocked = true;
        }

        liquidAppConfig->sync();
    }

    updateWindowTitle(title());
}

void LiquidAppWindow::updateWindowTitle(const QString title)
{
    QString textIcons;

    if (!liquidAppWindowTitleIsReadOnly) {
        if (title.size() > 0) {
            liquidAppWindowTitle = title;
        } else {
            liquidAppWindowTitle = *liquidAppName;
        }
    }

    // Append unicode icons
    if (pageHasCertificateError) {
        textIcons.append(LQD_ICON_WARNING);
    }
    if (windowGeometryIsLocked) {
        textIcons.append(LQD_ICON_LOCKED);
    }
    if (page()->isAudioMuted()) {
        textIcons.append(LQD_ICON_MUTED);
    }
    if (pageIsLoading) {
        textIcons.append(LQD_ICON_LOADING);
    } else {
        if (pageHasError) {
            textIcons.append(LQD_ICON_ERROR);
        }
    }

    if (textIcons != "") {
        textIcons = " " + textIcons;
    }

    setWindowTitle(liquidAppWindowTitle + textIcons);
}

void LiquidAppWindow::zoomIn(const bool fine = false)
{
    attemptToSetZoomFactorTo(zoomFactor() + ((fine) ? LQD_ZOOM_LVL_STEP_FINE : LQD_ZOOM_LVL_STEP));
}

void LiquidAppWindow::zoomOut(const bool fine = false)
{
    attemptToSetZoomFactorTo(zoomFactor() - ((fine) ? LQD_ZOOM_LVL_STEP_FINE : LQD_ZOOM_LVL_STEP));
}

void LiquidAppWindow::zoomReset(void)
{
    attemptToSetZoomFactorTo(1.0);
}
