#include <cstring>

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QKeyEvent>
#include <QEventLoop>
#include <QClipboard>
#include <QNetworkProxy>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantAnimation>
#include <QWebEngineHistory>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

#include "liquid.hpp"
#include "liquidappfile.hpp"
#include "liquidappicon.hpp"
#include "liquidapplocalstorage.hpp"
#include "liquidappconfigwindow.hpp"
#include "liquidappcookiejar.hpp"
#include "liquidappwebpage.hpp"
#include "liquidappwindow.hpp"
#include "singleinstance.hpp"
#include "svgsnapshot.hpp"
#include "lqd.h"
#ifdef Q_OS_MAC
#include "SetWindowBackgroundColor.h"
#endif

// White overlay that briefly lights up the window and fades away, imitating a camera flash
class SnapshotFlashOverlay : public QWidget
{
public:
    explicit SnapshotFlashOverlay(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();

        // Stay at full brightness for a moment, then gradually fade out (think flashbang, just quicker)
        static const int totalDuration = LQD_SNAPSHOT_FLASH_HOLD + LQD_SNAPSHOT_FLASH_FADE;
        animation = new QVariantAnimation(this);
        animation->setDuration(totalDuration);
        animation->setKeyValueAt(0.0, 1.0);
        animation->setKeyValueAt((qreal)LQD_SNAPSHOT_FLASH_HOLD / totalDuration, 1.0);
        animation->setKeyValueAt(1.0, 0.0);

        connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            intensity = value.toReal();
            update();
        });
        connect(animation, &QVariantAnimation::finished, this, &QWidget::hide);
    }

    void flash(void)
    {
        // Cover the entire window, on top of the web view
        setGeometry(parentWidget()->rect());
        raise();
        show();

        // Paint at full brightness right away: the program may be too busy (e.g. saving the snapshot to disk)
        // to process events for a while, and the brightest part of the animation would be over by then
        animation->stop();
        intensity = 1;
        repaint();

        // Begin fading out only once the program gets back to processing events
        // (consecutive flashes simply restart the animation)
        QTimer::singleShot(0, animation, [this]() {
            animation->stop();
            animation->start();
        });
    }

    void dismiss(void)
    {
        animation->stop();
        hide();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        QColor color(Qt::white);
        color.setAlphaF(intensity * LQD_SNAPSHOT_FLASH_OPACITY);
        painter.fillRect(rect(), color);
    }

private:
    QVariantAnimation* animation;
    qreal intensity = 0;
};

LiquidAppWindow::LiquidAppWindow(const QString* name) : QWebEngineView()
{
    // Prevent window from getting way too tiny
    setMinimumSize(LQD_APP_WIN_MIN_SIZE_W, LQD_APP_WIN_MIN_SIZE_H);

    // Tune web engine
    // NOTE: GPU rasterization and the GPU blocklist are deliberately left up to the web engine:
    //       forcing GPU rasterization on drivers known to mishandle it (e.g. nouveau) garbles text
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--enable-features=AutoplayIgnoreWebAudio");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--enable-accelerated-video-decode");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--enable-gpu-compositing");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--enable-smooth-scrolling");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--num-raster-threads=4");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--use-fake-ui-for-media-stream");

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // Ensure dark mode is enabled on the web page in case the system theme is dark
    if (Liquid::detectDarkMode()) {
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--force-dark-mode");
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--blink-settings=forceDarkModeEnabled=true");
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--blink-settings=darkMode=4");
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") + " " + "--blink-settings=darkModeEnabled=true");
    }
#endif

    // Set default icon
#if !defined(Q_OS_LINUX) // This doesn't work on X11
    setWindowIcon(QIcon(":/images/" PROG_NAME ".svg"));
#endif

    // Remember window geometry only once it settles (see resizeEvent())
    rememberGeometryTimer = new QTimer(this);
    rememberGeometryTimer->setSingleShot(true);
    rememberGeometryTimer->setInterval(200);
    connect(rememberGeometryTimer, &QTimer::timeout, this, &LiquidAppWindow::rememberGeometry);

    // Disable default QWebEngineView's context menu
    setContextMenuPolicy(Qt::PreventContextMenu);

    // Keep a copy, the string pointed at may not outlive this window
    liquidAppName = *name;

    liquidAppConfig = LiquidAppFile::open(*name);

    // These default settings affect everything (including sub-frames)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    LiquidAppWebPage::setWebSettingsToDefault(QWebEngineSettings::globalSettings());
#endif

    liquidAppWebProfile = new QWebEngineProfile(QString(), this);
    liquidAppWebProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    liquidAppWebProfile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    defaultHttpUserAgent = liquidAppWebProfile->httpUserAgent();

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
    liquidAppWindowTitle = liquidAppName;

    updateWindowTitle(liquidAppName);

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

    // Reveal Liquid app's window and bring it to front
    show();
#ifdef Q_OS_MAC
    // This can only have effect after the window is revealed
    SetWindowBackgroundColor(effectiveWinId(),
        page()->backgroundColor().redF(),
        page()->backgroundColor().greenF(),
        page()->backgroundColor().blueF(),
        page()->backgroundColor().alphaF());
#endif
    raise();
    activateWindow();

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

    // Put saved local storage items in place before the first page gets loaded
    liquidAppLocalStorage = new LiquidAppLocalStorage(liquidAppWebPage, liquidAppConfig, this);
    connect(liquidAppLocalStorage, &LiquidAppLocalStorage::sessionStorageChanged, this, [this]() {
        for (const QPointer<QLocalSocket>& connection : sessionStorageWatchers) {
            if (connection) {
                sendSessionStorage(connection);
            }
        }
    });

    // Load Liquid app's starting URL
    load(startingUrl);
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

    // Connect "take vector snapshot" shortcut
    takeSnapshotVectorAction = new QAction;
    takeSnapshotVectorAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_TAKE_SNAPSHOT_VECTOR)));
    addAction(takeSnapshotVectorAction);
    connect(takeSnapshotVectorAction, &QAction::triggered, this, &LiquidAppWindow::takeVectorSnapshot);

    // Connect "edit settings" shortcut
    editSettingsAction = new QAction;
    editSettingsAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_EDIT_SETTINGS)));
    addAction(editSettingsAction);
    connect(editSettingsAction, &QAction::triggered, this, &LiquidAppWindow::editSettings);

    // Connect "save page" shortcut
    savePageAction = new QAction;
    savePageAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_SAVE_PAGE)));
    addAction(savePageAction);
    connect(savePageAction, &QAction::triggered, this, [this](){
        // TODO: make it save the page along with its custom CSS/JS, as one monolithic HTML file (instead of MHT)
        // Save into the OS-specific downloads folder (falls back to home directory if there's none)
        QString path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (path.isEmpty()) {
            path = QDir::homePath();
        }
        QDir().mkpath(path);

        page()->save(Liquid::getUniqueFilePath(path, liquidAppName, "mhtml"));
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
    // Closing the window in the middle of taking a snapshot would pull the rug from under it
    if (isTakingSnapshot) {
        event->ignore();
        return;
    }

    // Settings being edited (e.g. when told to quit by the main window) get discarded, and their preview undone
    if (settingsDialog) {
        settingsDialog->reject();
    }

    // Whatever has changed in local storage since it's last been checked
    if (liquidAppLocalStorage->isEnabled()) {
        const QVariant result = runJavaScriptSync(LiquidAppLocalStorage::getCaptureScript(), LQD_LOCAL_STORAGE_QUIT_TIMEOUT);
        liquidAppLocalStorage->onCaptured(result.toString());
    }

    event->accept();
    deleteLater();
}

void LiquidAppWindow::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event);

    // Reflect current state (keyboard shortcuts work regardless, so it's only updated here)
    backAction->setEnabled(history()->canGoBack());
    forwardAction->setEnabled(history()->canGoForward());
    muteAudioAction->setChecked(page()->isAudioMuted());
    toggleFullScreenModeAction->setText((isFullScreen()) ? tr("Exit Full Screen") : tr("Enter Full Screen"));
    toggleFullScreenModeAction->setIcon(QIcon::fromTheme((isFullScreen()) ? QStringLiteral("view-restore") : QStringLiteral("view-fullscreen")));
    toggleGeometryLockAction->setChecked(windowGeometryIsLocked);
    toggleGeometryLockAction->setText((windowGeometryIsLocked) ? tr("Unfreeze Frame Size") : tr("Freeze Frame Size"));
    toggleGeometryLockAction->setEnabled(!isFullScreen()); // Frame size can't be frozen while in full-screen mode

    contextMenu->exec(QCursor::pos());

    // Disabled actions don't respond to keyboard shortcuts either, and the state they reflect changes all the time
    backAction->setEnabled(true);
    forwardAction->setEnabled(true);
    toggleGeometryLockAction->setEnabled(true);
}

bool LiquidAppWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (isTakingSnapshot) {
        // Hitting Escape cancels taking the snapshot
        if (isEscapeKeyEvent(event)) {
            snapshotCancelled = true;
            return true;
        }

        // Ignore all other user input while a full-page snapshot is being taken
        // (clicking, selecting text, scrolling, or triggering keyboard shortcuts would mess it up)
        if (isUserInputEvent(event->type())) {
            return true;
        }
    }

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
                               "})();");
    page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);

    // Ensure that while this Liquid App is being reset, the window title remains to be set to this Liquid App's name
    // to mimic the same experience that happens when the user initially runs this Liquid app
    if (!liquidAppWindowTitleIsReadOnly) {
        liquidAppWindowTitle = liquidAppName;

        const QString js = QString("(()=>{"\
                                       "let e=document.createElement('title');"\
                                       "e.innerText='%1';"\
                                       "document.appendChild(e)"\
                                   "})();").arg(liquidAppWindowTitle.replace("'", "\\'"));
        page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);
    }

    updateWindowTitle(title());

    // TODO: reset localStorage / Cookies in case they're disabled?

    // TODO: clear any type of cache, if possible

    QUrl url(liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString(), QUrl::StrictMode);
    setUrl(url);
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

    // Get the website's icon, if the Liquid app doesn't have one yet
    if (ok && !pageHasError) {
        maybeFetchIcon();
    }
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
            setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        }
    }

    // Set the page's background color behind the document's body (and make the window translucent if needed)
    applyBackgroundColor();

    // Determine where this Liquid app is allowed to navigate, and what should be opened in external browser
    applyAllowedDomains();

    // Deal with Cookies
    {
        liquidAppCookieJar = new LiquidAppCookieJar(this);
        QWebEngineCookieStore *cookieStore = page()->profile()->cookieStore();

        connect(cookieStore, &QWebEngineCookieStore::cookieAdded, liquidAppCookieJar, &LiquidAppCookieJar::upsertCookie);
        connect(cookieStore, &QWebEngineCookieStore::cookieRemoved, liquidAppCookieJar, &LiquidAppCookieJar::removeCookie);

        liquidAppCookieJar->restoreCookies(cookieStore);
    }

    // Restore window geometry
    {
        const QRect primaryScreenArea = QGuiApplication::primaryScreen()->availableGeometry();
        // Defaults to a quarter of the primary screen, centered
        QRect windowGeometry(0, 0, primaryScreenArea.width() / 2, primaryScreenArea.height() / 2);
        windowGeometry.moveCenter(primaryScreenArea.center());

        if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_WIN_WIDTH) && liquidAppConfig->contains(LQD_CFG_KEY_NAME_WIN_HEIGHT)) {
            const int savedWidth = liquidAppConfig->value(LQD_CFG_KEY_NAME_WIN_WIDTH).toInt();
            const int savedHeight = liquidAppConfig->value(LQD_CFG_KEY_NAME_WIN_HEIGHT).toInt();

            if (savedWidth > 0 && savedHeight > 0) {
                windowGeometry.setSize(QSize(savedWidth, savedHeight).expandedTo(minimumSize()));
                windowGeometry.moveCenter(primaryScreenArea.center());
            }
        }

        if (!isPositionManagedByWindowSystem()
            && liquidAppConfig->contains(LQD_CFG_KEY_NAME_WIN_X) && liquidAppConfig->contains(LQD_CFG_KEY_NAME_WIN_Y)) {
            windowGeometry.moveTopLeft(QPoint(liquidAppConfig->value(LQD_CFG_KEY_NAME_WIN_X).toInt(),
                                              liquidAppConfig->value(LQD_CFG_KEY_NAME_WIN_Y).toInt()));
        }

        // Make sure the window ends up on a screen, and fits on it (the screen setup may have changed since,
        // windows that are way too large may not show up at all on some platforms, e.g. Wayland)
        QScreen* screen = QGuiApplication::screenAt(windowGeometry.center());
        if (screen == Q_NULLPTR) {
            screen = QGuiApplication::primaryScreen();
            windowGeometry.moveCenter(screen->availableGeometry().center());
        }

        const QRect screenArea = screen->availableGeometry();
        if (windowGeometry.width() > screenArea.width() || windowGeometry.height() > screenArea.height()) {
            qDebug().noquote() << "Saved window size doesn't fit on screen, shrinking it";
            windowGeometry.setSize(windowGeometry.size().boundedTo(screenArea.size()));
            windowGeometry.moveCenter(screenArea.center());
        }

        setGeometry(windowGeometry);
        rememberedGeometry = windowGeometry;

        if (liquidAppConfig->value(LQD_CFG_KEY_NAME_WIN_MAXIMIZED, false).toBool()) {
            setWindowState(windowState() | Qt::WindowMaximized);
        }

        if (liquidAppConfig->value(LQD_CFG_KEY_NAME_WIN_FULLSCREEN, false).toBool()) {
            setWindowState(windowState() | Qt::WindowFullScreen);
        }
    }

    // JavaScript, scrollbars, user-agent string
    applyWebSettings();

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

    // Additional user-defined CSS and JS
    applyAdditionalCssAndJs();

    // Liquid app's own icon (if it has one already)
    applyIcon();
    LiquidAppIcon::ensureIconFile(liquidAppConfig, liquidAppName);
}

void LiquidAppWindow::applyAllowedDomains(void)
{
    // Determine where this Liquid app is allowed to navigate, and what should be opened in external browser
    QStringList domains;
    domains << QUrl(liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString()).host();

    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS)) {
        domains << liquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS).toString().split(' ', Qt::SkipEmptyParts);
    }

    liquidAppWebPage->setAllowedDomains(domains);
}

void LiquidAppWindow::applyBackgroundColor(void)
{
    if (liquidAppConfig->value(LQD_CFG_KEY_NAME_USE_CUSTOM_BG, false).toBool() && liquidAppConfig->contains(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR)) {
        setBackground(QColor(liquidAppConfig->value(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR).toString()));
    } else {
        setBackground(QColor());
    }
}

void LiquidAppWindow::setBackground(const QColor& color)
{
    // Invalid color means the default one
    const QColor backgroundColor = (color.isValid()) ? color : QColor(LQD_DEFAULT_BG_COLOR);
    const bool needsTranslucency = backgroundColor.alpha() < 255;

    // Set the page's background color behind the document's body
    page()->setBackgroundColor(backgroundColor);

    // Whether the window can be see-through or not gets decided upon creation of its native window,
    // so if that needs to change for a window that's already shown, its native window has to be recreated
    if (testAttribute(Qt::WA_TranslucentBackground) != needsTranslucency) {
        setAttribute(Qt::WA_TranslucentBackground, needsTranslucency);
        if (!needsTranslucency) {
            setAttribute(Qt::WA_NoSystemBackground, false); // Gets set along with translucency, but not unset
        }

        if (isVisible()) {
            recreateNativeWindow();
        }
    }

#ifdef Q_OS_MAC
    if (isVisible()) {
        SetWindowBackgroundColor(effectiveWinId(), backgroundColor.redF(), backgroundColor.greenF(), backgroundColor.blueF(), backgroundColor.alphaF());
    }
#endif
}

void LiquidAppWindow::recreateNativeWindow(void)
{
    // Re-applying window flags makes Qt replace the native window (which also hides it); the web view and the page
    // it shows remain intact (on some platforms, e.g. Wayland, the window may end up at a different position though)
    const Qt::WindowStates states = windowState();

    setWindowFlags(windowFlags());
    setWindowState(states);
    show();

    // The new native window doesn't come up on top of others by itself (window managers treat it as one that just
    // appeared on its own, out of nowhere, while the user is busy with another window), so bring it to front the same way
    // a double-click in the main window does, once the window manager has had a moment to put the new one on screen
    if (!states.testFlag(Qt::WindowMinimized)) {
        QTimer::singleShot(LQD_RECREATED_WINDOW_RAISE_DELAY, this, [this]() {
            if (isVisible() && !isMinimized()) {
                Liquid::raiseWindow(this, QString());
            }
        });
    }
}

void LiquidAppWindow::applyWebSettings(void)
{
    settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                             liquidAppConfig->value(LQD_CFG_KEY_NAME_ENABLE_JS, false).toBool());

    setScrollBarsHidden(liquidAppConfig->value(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS, false).toBool());

    const QString customUserAgent = liquidAppConfig->value(LQD_CFG_KEY_NAME_USER_AGENT).toString();
    liquidAppWebProfile->setHttpUserAgent((customUserAgent.isEmpty()) ? defaultHttpUserAgent : customUserAgent);
}

void LiquidAppWindow::applyAdditionalCssAndJs(void)
{
    QWebEngineScriptCollection& scripts = liquidAppWebPage->scripts();

    // Remove previously added ones
    for (const QString& name : { QStringLiteral(LQD_SCRIPT_NAME_ADDITIONAL_CSS), QStringLiteral(LQD_SCRIPT_NAME_ADDITIONAL_JS) }) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QList<QWebEngineScript> existingScripts = scripts.find(name);
#else
        const QList<QWebEngineScript> existingScripts = scripts.findScripts(name);
#endif
        for (const QWebEngineScript& existingScript : existingScripts) {
            scripts.remove(existingScript);
        }
    }

    // Additional user-defined CSS (does't require JavaScript enabled in order to work)
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_CSS)) {
        QWebEngineScript script;
        script.setName(LQD_SCRIPT_NAME_ADDITIONAL_CSS);
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setRunsOnSubFrames(false);
        script.setSourceCode(getAdditionalCssScript(liquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_CSS).toString()));
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        scripts.insert(script);
    }

    // Additional user-defined JS (does't require JavaScript enabled in order to work)
    if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_JS)) {
        QWebEngineScript script;
        script.setName(LQD_SCRIPT_NAME_ADDITIONAL_JS);
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setRunsOnSubFrames(false);
        script.setSourceCode(liquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_JS).toString());
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        scripts.insert(script);
    }
}

QString LiquidAppWindow::getAdditionalCssScript(const QString& css)
{
    // Puts the CSS into a <style> element of its own, creating it if needed, or updating it if it's already there
    // (the CSS gets encoded as JSON, which makes it safe to embed into JavaScript code, whatever it contains)
    const QString cssAsJson = QString::fromUtf8(QJsonDocument(QJsonArray() << css).toJson(QJsonDocument::Compact));

    return QString("(() => {"
                       "const css = %1[0];"
                       "let styleElement = document.querySelector('style[data-liquid-additional-css]');"
                       "if (!styleElement) {"
                           "styleElement = document.createElement('style');"
                           "styleElement.setAttribute('data-liquid-additional-css', '');"
                           "(document.head || document.documentElement).appendChild(styleElement);"
                       "}"
                       "styleElement.textContent = css;"
                   "})();").arg(cssAsJson);
}

void LiquidAppWindow::setAdditionalCss(const QString& css)
{
    // Applies to the page currently shown, right away (no reloading needed)
    page()->runJavaScript(getAdditionalCssScript(css), QWebEngineScript::ApplicationWorld);
}

void LiquidAppWindow::reloadLiquidAppConfig(void)
{
    // Remember settings that only take effect upon loading the page, to tell whether it needs to be reloaded
    static const QStringList pageAffectingKeys = {
        LQD_CFG_KEY_NAME_ENABLE_JS,
        LQD_CFG_KEY_NAME_USER_AGENT,
        LQD_CFG_KEY_NAME_ADDITIONAL_JS,
    };
    QVariantList previousValues;
    for (const QString& key : pageAffectingKeys) {
        previousValues << liquidAppConfig->value(key);
    }
    const QString previousUrl = liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString();

    // Pick up changes made to the file by another process (e.g. the main window)
    liquidAppConfig->sync();

    // Window title
    liquidAppWindowTitleIsReadOnly = liquidAppConfig->contains(LQD_CFG_KEY_NAME_TITLE);
    if (liquidAppWindowTitleIsReadOnly) {
        liquidAppWindowTitle = liquidAppConfig->value(LQD_CFG_KEY_NAME_TITLE).toString();
    }
    updateWindowTitle(title());

    applyAllowedDomains();
    applyBackgroundColor();
    applyWebSettings();
    applyAdditionalCssAndJs();

    // Icon (settings may have taken it away, to have it fetched from the website again)
    if (!liquidAppConfig->contains(LQD_CFG_KEY_NAME_ICON)) {
        iconFetchAttempted = false;
    }
    applyIcon();
    if (!pageIsLoading && !pageHasError) {
        maybeFetchIcon();
    }

    // Local storage (may have been allowed or disallowed to be remembered, or had its items edited)
    liquidAppLocalStorage->reloadConfig();

    // Cookies removed in settings
    liquidAppCookieJar->removeCookiesMissingFromConfig(page()->profile()->cookieStore());

    // Custom CSS gets applied to the page that's currently shown without reloading it
    // (this also undoes whatever CSS has been previewed while editing settings)
    setAdditionalCss(liquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_CSS).toString());

    // Window frame (compared against the window itself rather than the previously saved value,
    // so that a frame toggled while previewing settings gets put back too)
    setFrameless(liquidAppConfig->value(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME, false).toBool());

    // Load the new starting URL, or reload the page if anything it depends on has changed
    const QString url = liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString();
    if (url != previousUrl) {
        load(QUrl(url));
    } else {
        for (int i = 0; i < pageAffectingKeys.size(); i++) {
            if (liquidAppConfig->value(pageAffectingKeys[i]) != previousValues[i]) {
                reload();
                break;
            }
        }
    }
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
    // Remember window position once it settles (window state may not be up to date yet while getting maximized)
    rememberGeometryTimer->start();

    QWebEngineView::moveEvent(event);
}

void LiquidAppWindow::rememberGeometry(void)
{
    // Only the "normal" geometry gets remembered, maximized and full-screen states are saved separately
    if (!(windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen | Qt::WindowMinimized))) {
        rememberedGeometry = geometry();
    }
}

bool LiquidAppWindow::isPositionManagedByWindowSystem(void)
{
    // On Wayland, programs can neither know nor choose where their windows are
    return QGuiApplication::platformName().startsWith("wayland");
}

void LiquidAppWindow::onIconChanged(QIcon icon)
{
    pageIcon = icon;

    if (!liquidAppConfig->contains(LQD_CFG_KEY_NAME_ICON)) {
        // Show it until the Liquid app has an icon of its own
        applyIcon();

        // Fetching may have found nothing usable before the page's own icon arrived
        if (iconFetchAttempted && iconFetcher == Q_NULLPTR && !icon.isNull() && isShowingOwnWebsite()) {
            useFetchedIcon(LiquidAppIcon());
        }
    }
}

void LiquidAppWindow::applyIcon(void)
{
    const LiquidAppIcon liquidAppIcon = LiquidAppIcon::load(liquidAppConfig);

    if (!liquidAppIcon.isNull()) {
        const QIcon icon = liquidAppIcon.toIcon();

        setWindowIcon(icon);
        // Also used for the Dock on macOS, and for whatever new windows (e.g. dialogs) this Liquid app opens
        QApplication::setWindowIcon(icon);
    } else if (!pageIcon.isNull()) {
        setWindowIcon(pageIcon);
    }
}

bool LiquidAppWindow::isShowingOwnWebsite(void)
{
    // Icons of whatever the Liquid app may pass through first (e.g. sign-in pages elsewhere) are of no interest
    static const QRegularExpression wwwPrefix("^www\\.");
    const QString startingHost = QUrl(liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString()).host().toLower().remove(wwwPrefix);
    const QString currentHost = url().host().toLower().remove(wwwPrefix);

    if (startingHost.isEmpty() || currentHost.isEmpty()) {
        return false;
    }

    return currentHost == startingHost
        || currentHost.endsWith("." + startingHost)
        || startingHost.endsWith("." + currentHost);
}

void LiquidAppWindow::maybeFetchIcon(void)
{
    if (iconFetchAttempted || liquidAppConfig->contains(LQD_CFG_KEY_NAME_ICON) || !isShowingOwnWebsite()) {
        return;
    }

    iconFetchAttempted = true;

    iconFetcher = new LiquidAppIconFetcher(page(), this);
    connect(iconFetcher, &LiquidAppIconFetcher::finished, this, [this](const LiquidAppIcon& icon) {
        iconFetcher->deleteLater();
        iconFetcher = Q_NULLPTR;

        useFetchedIcon(icon);
    });
    iconFetcher->start();
}

void LiquidAppWindow::useFetchedIcon(const LiquidAppIcon& icon)
{
    // The icon the page has provided may be better than anything that got downloaded (e.g. if the website needs cookies)
    const LiquidAppIcon pageLiquidAppIcon = LiquidAppIcon::fromIcon(pageIcon);
    const LiquidAppIcon& bestIcon = (pageLiquidAppIcon.score() > icon.score()) ? pageLiquidAppIcon : icon;

    // Settings may have been given an icon in the meantime
    liquidAppConfig->sync();
    if (bestIcon.isNull() || liquidAppConfig->contains(LQD_CFG_KEY_NAME_ICON)) {
        return;
    }

    bestIcon.save(liquidAppConfig, liquidAppName);
    applyIcon();

    // Let the main window (if it's open) show it
    SingleInstance::sendMessage(LQD_INSTANCE_ID_MAIN, QByteArray(LQD_INSTANCE_MSG_ICON_CHANGED) + " " + liquidAppName.toUtf8());
}

void LiquidAppWindow::resizeEvent(QResizeEvent* event)
{
    // Remember window size once it stops changing (e.g. the user is done dragging, or resize animation is over)
    if (event->spontaneous()) {
        rememberGeometryTimer->start();
    }

    QWebEngineView::resizeEvent(event);
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

    // Window geometry
    rememberGeometry();
    if (rememberedGeometry.isValid()) {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_WIN_WIDTH, rememberedGeometry.width());
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_WIN_HEIGHT, rememberedGeometry.height());

        // Position remains untouched where it's unknown (it may still be of use on a different machine)
        if (!isPositionManagedByWindowSystem()) {
            liquidAppConfig->setValue(LQD_CFG_KEY_NAME_WIN_X, rememberedGeometry.x());
            liquidAppConfig->setValue(LQD_CFG_KEY_NAME_WIN_Y, rememberedGeometry.y());
        }
    }

    if (isMaximized()) {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_WIN_MAXIMIZED, true);
    } else {
        liquidAppConfig->remove(LQD_CFG_KEY_NAME_WIN_MAXIMIZED);
    }

    if (isFullScreen()) {
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_WIN_FULLSCREEN, true);
    } else {
        liquidAppConfig->remove(LQD_CFG_KEY_NAME_WIN_FULLSCREEN);
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
    contextMenu = new QMenu(this);

    // The menu reuses actions behind keyboard shortcuts, which makes it show the shortcuts next to them
    const auto present = [](QAction* action, const char* const iconName, const QString& text) {
        action->setIcon(QIcon::fromTheme(QLatin1String(iconName)));
        action->setText(text);
        action->setShortcutVisibleInContextMenu(true);
    };

    contextMenuCopyUrlAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy Current URL"), this);
    connect(contextMenuCopyUrlAction, &QAction::triggered, this, [this](){
        QApplication::clipboard()->setText(page()->url().toString());
    });

    present(backAction, "go-previous", tr("Go Back"));
    present(forwardAction, "go-next", tr("Go Forward"));
    present(reloadAction, "view-refresh", tr("Refresh"));
    present(savePageAction, "document-save", tr("Save Page"));
    present(takeSnapshotAction, "camera-photo", tr("Take Screenshot"));
    present(takeSnapshotFullPageAction, "camera-photo", tr("Take Full-Page Screenshot"));
    present(takeSnapshotVectorAction, "image-x-generic", tr("Save as Vector Image"));
    present(muteAudioAction, "audio-volume-muted", tr("Mute Audio"));
    present(toggleFullScreenModeAction, "view-fullscreen", tr("Enter Full Screen"));
    present(toggleGeometryLockAction, "object-locked", tr("Freeze Frame Size"));
    present(editSettingsAction, "preferences-system", tr("Settings…"));
    present(quitAction, "application-exit", tr("Quit"));

    muteAudioAction->setCheckable(true);
    toggleGeometryLockAction->setCheckable(true);

    contextMenu->addAction(backAction);
    contextMenu->addAction(forwardAction);
    contextMenu->addAction(reloadAction);
    contextMenu->addSeparator();
    contextMenu->addAction(contextMenuCopyUrlAction);
    contextMenu->addAction(savePageAction);
    contextMenu->addSeparator();
    contextMenu->addAction(takeSnapshotAction);
    contextMenu->addAction(takeSnapshotFullPageAction);
    contextMenu->addAction(takeSnapshotVectorAction);
    contextMenu->addSeparator();
    contextMenu->addAction(muteAudioAction);
    contextMenu->addAction(toggleFullScreenModeAction);
    contextMenu->addAction(toggleGeometryLockAction);
    contextMenu->addSeparator();
    contextMenu->addAction(editSettingsAction);
    contextMenu->addAction(quitAction);

    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void LiquidAppWindow::editSettings(void)
{
    // Only one at a time
    if (settingsDialog) {
        Liquid::raiseWindow(settingsDialog);
        return;
    }

    LiquidAppConfigDialog dialog(this, liquidAppName);
    dialog.setOpenedFromLiquidApp(true);
    settingsDialog = &dialog;

    // Changes get previewed while editing (the dialog talks to this Liquid app the same way it would from the main window)
    const int result = dialog.exec();
    settingsDialog = Q_NULLPTR;

    if (result != QDialog::Accepted) {
        return;
    }

    if (dialog.isRestartRequired()) {
        // Quit (saving everything as usual), and have this program run the Liquid app again on its way out
        Liquid::requestRestart(liquidAppName);
        close();
        return;
    }

    reloadLiquidAppConfig();

    // Let the main window (if it's open) show the icon, in case it's changed
    SingleInstance::sendMessage(LQD_INSTANCE_ID_MAIN, QByteArray(LQD_INSTANCE_MSG_ICON_CHANGED) + " " + liquidAppName.toUtf8());
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

QString LiquidAppWindow::getSnapshotsDirPath(void)
{
    const QString path = QDir::homePath() + QDir::separator() + "Pictures";

    // Ensure the target directory exists
    QDir().mkpath(path);

    return path;
}

void LiquidAppWindow::takeSnapshot(const bool fullPage)
{
    const int ratio = QPaintDevice::devicePixelRatio();
    const QSize snapshotSize = contentsRect().size();
    const QString path = getSnapshotsDirPath();

    // Compose snapshot file base name (a unique number gets appended to it upon saving)
    const QString baseName = QString("%1 %2")
                                .arg(liquidAppName)
                                .arg(tr((fullPage) ? "full-page snapshot" : "snapshot"));

    if (fullPage) {
        const QImage image = captureFullPage();

        if (!image.isNull()) {
            // Save raster image to disk (already in logical pixels, the same way as regular snapshots)
            image.save(Liquid::getUniqueFilePath(path, baseName, "png"), "PNG");
        }
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
        renderSnapshot(painter);

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        if (hadScrollBarsShown) {
            // Bring scrollbars back after taking snapshot
            liquidAppWebSettings->setAttribute(QWebEngineSettings::ShowScrollBars, true);
        }
#endif

        painter->end();
        delete painter;

        // Save raster image to disk
        image->scaled(snapshotSize.width(), snapshotSize.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .save(Liquid::getUniqueFilePath(path, baseName, "png"), "PNG");

        // TODO: add EXIF?

        delete image;
    }

    // TODO: add shutter sound
}

void LiquidAppWindow::renderSnapshot(QPainter* painter)
{
    // Make sure that the flash from the previous snapshot (or chunk) doesn't end up in this one
    if (snapshotFlashOverlay != Q_NULLPTR) {
        snapshotFlashOverlay->hide();
    }

    // Render contents of QWidget into QPainter
    render(painter);

    flash();
}

void LiquidAppWindow::flash(void)
{
    if (snapshotFlashOverlay == Q_NULLPTR) {
        snapshotFlashOverlay = new SnapshotFlashOverlay(this);
    }

    // Flash, the way cameras do
    snapshotFlashOverlay->flash();
}

void LiquidAppWindow::takeVectorSnapshot(void)
{
    if (isTakingSnapshot) {
        return;
    }

    QFile scriptFile(":/scripts/html2svg.js");
    if (!scriptFile.open(QFile::ReadOnly | QFile::Text)) {
        qDebug().noquote() << "Unable to load html2svg.js";
        return;
    }
    const QString script = QString::fromUtf8(scriptFile.readAll());

    // Ignore user input until done (see eventFilter()), Escape cancels
    isTakingSnapshot = true;
    snapshotCancelled = false;
    QApplication::setOverrideCursor(Qt::WaitCursor);

    bool isDone = false;

    if (waitForDocumentToFinishLoading()) {
        // The script walks the page asynchronously, and reports back by setting window.lqdHtml2Svg.done to true
        page()->runJavaScript(script, QWebEngineScript::ApplicationWorld);

        QElapsedTimer timer;
        timer.start();

        while (!snapshotCancelled && timer.elapsed() < LQD_VECTOR_SNAPSHOT_TIMEOUT) {
            if (runJavaScriptSync("window.lqdHtml2Svg ? window.lqdHtml2Svg.done === true : false").toBool()) {
                isDone = true;
                break;
            }

            Liquid::sleep(50);
        }
    }

    QByteArray svg;
    QString error;

    if (snapshotCancelled) {
        qDebug().noquote() << "Vector snapshot cancelled";
    } else if (!isDone) {
        qDebug().noquote() << "Timed out taking vector snapshot";
    } else {
        const QVariantList outcome = runJavaScriptSync("[window.lqdHtml2Svg.result, window.lqdHtml2Svg.error]", LQD_VECTOR_SNAPSHOT_TIMEOUT).toList();

        if (outcome.size() != 2) {
            error = "No data received from the page";
        } else if (!outcome[1].isNull() && outcome[1].isValid()) {
            error = outcome[1].toString();
        } else {
            svg = SvgSnapshot::build(outcome[0].toString().toUtf8(), page()->backgroundColor(), zoomFactor(), &error);
        }

        if (!error.isEmpty()) {
            qDebug().noquote() << "Unable to take vector snapshot:" << error;
        }
    }

    // Free up the page's memory
    page()->runJavaScript("delete window.lqdHtml2Svg;", QWebEngineScript::ApplicationWorld);

    if (!svg.isEmpty()) {
        const QString baseName = QString("%1 %2").arg(liquidAppName).arg(tr("vector snapshot"));
        QSaveFile file(Liquid::getUniqueFilePath(getSnapshotsDirPath(), baseName, "svg"));

        if (file.open(QIODevice::WriteOnly) && file.write(svg) == svg.size() && file.commit()) {
            flash();
        } else {
            qDebug().noquote() << "Unable to save vector snapshot:" << file.errorString();
        }
    }

    QApplication::restoreOverrideCursor();
    isTakingSnapshot = false;
}

QImage LiquidAppWindow::captureFullPage(void)
{
    // The page gets captured one viewport-sized tile at a time while scrolling it from top to bottom,
    // which avoids having to resize the window (and hitting GPU texture size limits for long pages)

    const qreal dpr = devicePixelRatioF();
    const qreal zoom = zoomFactor();

    // Remember initial scroll position to be able to come back to it after the whole page is captured
    const QVariantList origScrollPos = runJavaScriptSync("[window.scrollX, window.scrollY]").toList();
    if (origScrollPos.size() != 2) {
        qDebug().noquote() << "Unable to take full-page snapshot: the page did not respond";
        return QImage();
    }

    // Lock the window down for the duration of the capture: no resizing, and no user input (see eventFilter())
    isTakingSnapshot = true;
    snapshotCancelled = false;
    const QSize origMinimumSize = minimumSize();
    const QSize origMaximumSize = maximumSize();
    setFixedSize(size());
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Don't start capturing until the entire page is there (if cancelled while waiting, the capture loop exits right away)
    waitForDocumentToFinishLoading();

    // Hide scrollbars, otherwise they'd end up in every tile
    const bool hadScrollBarsShown = liquidAppWebSettings->testAttribute(QWebEngineSettings::ShowScrollBars);
    if (hadScrollBarsShown) {
        liquidAppWebSettings->setAttribute(QWebEngineSettings::ShowScrollBars, false);
        waitForNextFrame();
    }

    // Measure the viewport (in CSS pixels) only after scrollbars are gone, since hiding them may cause the page to reflow
    const qreal viewportHeight = runJavaScriptSync("window.innerHeight").toReal();
    const QSize tileSize = size() * dpr;
    // Tiles get scaled down to logical pixels right away (that's what snapshots get saved in),
    // otherwise high-DPI screens would use up the size limit several times faster for nothing
    const QSize logicalTileSize = size();

    // Pieces of the page, along with their vertical offsets within the resulting image
    QList<QPair<int, QImage>> pieces;
    int imageHeight = 0;

    if (viewportHeight > 0 && !logicalTileSize.isEmpty()) {
        const qint64 maxImageHeight = LQD_SNAPSHOT_MAX_PIXELS / logicalTileSize.width();

        QImage tile(tileSize, QImage::Format_ARGB32);
        tile.setDevicePixelRatio(dpr);

        // Keep scrolling down until the page can't be scrolled any further
        // (measuring the page's height upfront isn't reliable, pages may grow while being scrolled through)
        for (qreal y = 0; ; y += viewportHeight) {
            // The user may have hit Escape while the program was waiting for the page
            if (snapshotCancelled) {
                break;
            }

            const int destTop = qRound(y * zoom);

            if (destTop >= maxImageHeight) {
                qDebug().noquote() << "Page is too long, full-page snapshot will be cut off at the bottom";
                break;
            }

            // Scroll instantly (bypassing smooth scrolling), then find out where the page actually ended up
            // (it can't scroll past its bottom, so the last tile usually overlaps with the previous one)
            const QVariant actualY = runJavaScriptSync(
                QString("window.scrollTo({left: 0, top: %1, behavior: 'instant'}); window.scrollY").arg(y)
            );
            if (!actualY.isValid()) {
                qDebug().noquote() << "The page stopped responding, full-page snapshot will be incomplete";
                break;
            }

            const int srcTop = qRound((y - actualY.toReal()) * zoom);
            if (srcTop < 0 || srcTop >= logicalTileSize.height()) {
                // Nothing new to capture, the previous tile already reached the bottom of the page
                break;
            }

            waitForNextFrame();

            if (snapshotCancelled) {
                break;
            }

            // Render contents of QWidget into the tile
            tile.fill(Qt::transparent);
            {
                QPainter painter(&tile);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setRenderHint(QPainter::TextAntialiasing);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                renderSnapshot(&painter);
            }

            QImage logicalTile = (tileSize != logicalTileSize)
                ? tile.scaled(logicalTileSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                : tile;
            logicalTile.setDevicePixelRatio(1);

            // Keep only the part of the tile that hasn't been captured yet
            const int rowCount = qMin(logicalTileSize.height() - srcTop, (int)(maxImageHeight - destTop));
            pieces.append(qMakePair(destTop, logicalTile.copy(0, srcTop, logicalTileSize.width(), rowCount)));
            imageHeight = destTop + rowCount;

            if (actualY.toReal() < y - 0.5) {
                // The page couldn't be scrolled as far as requested, meaning that its bottom has been reached
                break;
            }
        }
    }

    // Stitch the pieces together (unless cancelled, in which case nothing gets saved)
    QImage image;
    if (snapshotCancelled) {
        qDebug().noquote() << "Full-page snapshot cancelled";
        pieces.clear();
        if (snapshotFlashOverlay != Q_NULLPTR) {
            snapshotFlashOverlay->dismiss();
        }
    } else if (imageHeight > 0) {
        image = QImage(logicalTileSize.width(), imageHeight, QImage::Format_ARGB32);
    }

    if (image.isNull()) {
        if (!snapshotCancelled) {
            qDebug().noquote() << "Unable to take full-page snapshot";
        }
    } else {
        image.fill(Qt::transparent);

        for (const QPair<int, QImage>& piece : pieces) {
            for (int row = 0; row < piece.second.height() && piece.first + row < image.height(); row++) {
                memcpy(image.scanLine(piece.first + row), piece.second.constScanLine(row), image.bytesPerLine());
            }
        }
    }

    // Bring scrollbars back
    if (hadScrollBarsShown) {
        liquidAppWebSettings->setAttribute(QWebEngineSettings::ShowScrollBars, true);
    }

    // Scroll the web view back to where it was before taking the snapshot
    page()->runJavaScript(
        QString("window.scrollTo({left: %1, top: %2, behavior: 'instant'});").arg(origScrollPos[0].toReal()).arg(origScrollPos[1].toReal()),
        QWebEngineScript::ApplicationWorld
    );

    // Unlock the window
    QApplication::restoreOverrideCursor();
    setMinimumSize(origMinimumSize);
    setMaximumSize(origMaximumSize);
    isTakingSnapshot = false;

    return image;
}

bool LiquidAppWindow::isEscapeKeyEvent(QEvent* event)
{
    switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::ShortcutOverride:
            return static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape;

        // Escape is bound to "stop loading / exit full-screen mode", so it usually arrives as a shortcut
        case QEvent::Shortcut:
            return static_cast<QShortcutEvent*>(event)->key() == QKeySequence(Qt::Key_Escape);

        default:
            return false;
    }
}

bool LiquidAppWindow::isUserInputEvent(const QEvent::Type eventType)
{
    switch (eventType) {
        case QEvent::ContextMenu:
        case QEvent::DragEnter:
        case QEvent::DragLeave:
        case QEvent::DragMove:
        case QEvent::Drop:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
        case QEvent::InputMethod:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::NativeGesture:
        case QEvent::Shortcut:
        case QEvent::ShortcutOverride:
        case QEvent::TabletMove:
        case QEvent::TabletPress:
        case QEvent::TabletRelease:
        case QEvent::TouchBegin:
        case QEvent::TouchCancel:
        case QEvent::TouchEnd:
        case QEvent::TouchUpdate:
        case QEvent::Wheel:
            return true;

        default:
            return false;
    }
}

bool LiquidAppWindow::waitForDocumentToFinishLoading(void)
{
    // Large pages (e.g. huge comment threads) arrive in pieces and get laid out as they do,
    // taking a snapshot before the whole document is there would leave out everything that hasn't arrived yet
    QElapsedTimer timer;
    timer.start();
    bool isWaiting = false;

    while (!snapshotCancelled && timer.elapsed() < LQD_SNAPSHOT_LOAD_WAIT_TIMEOUT) {
        // Scripts in ApplicationWorld run even when JavaScript is disabled for the web page
        if (runJavaScriptSync("document.readyState").toString() != "loading") {
            return true;
        }

        if (!isWaiting) {
            qDebug().noquote() << "Waiting for the page to finish loading before taking snapshot";
            isWaiting = true;
        }

        Liquid::sleep(100);
    }

    if (!snapshotCancelled) {
        qDebug().noquote() << "The page is still loading, snapshot may be incomplete";
    }

    return !snapshotCancelled;
}

QVariant LiquidAppWindow::runJavaScriptSync(const QString& js, const int timeoutMs)
{
    // Shared with the callback, which may get called after this function has already returned (upon timeout)
    struct Reply {
        bool received = false;
        QVariant result;
    };
    const QSharedPointer<Reply> reply(new Reply);

    QEventLoop loop;
    QPointer<QEventLoop> loopPointer(&loop);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);

    // Scripts in ApplicationWorld run even when JavaScript is disabled for the web page
    page()->runJavaScript(js, QWebEngineScript::ApplicationWorld, [reply, loopPointer](const QVariant& result) {
        reply->result = result;
        reply->received = true;

        if (loopPointer) {
            loopPointer->quit();
        }
    });

    if (!reply->received) {
        loop.exec();
    }

    // Results in an invalid QVariant upon timeout
    return reply->result;
}

void LiquidAppWindow::waitForNextFrame(void)
{
    // Animation frame callbacks never run when JavaScript is disabled for the page (not even ones that come
    // from an isolated world), so there's no way to know when the next frame is ready: wait a fixed amount of time
    if (!liquidAppWebSettings->testAttribute(QWebEngineSettings::JavascriptEnabled)) {
        Liquid::sleep(LQD_FRAME_WAIT_WITHOUT_JS);
        return;
    }

    // Ask the page to signal once it has gone through two animation frames
    // (the first one is still being prepared at the moment of the request, the second one has the changes)
    runJavaScriptSync("window.lqdFrameReady = false;"
                      "requestAnimationFrame(() => requestAnimationFrame(() => { window.lqdFrameReady = true; }));");

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < 1000 && !runJavaScriptSync("window.lqdFrameReady === true").toBool()) {
        Liquid::sleep(16);
    }

    // Give the freshly rendered frame a moment to travel from the web engine to the widget
    Liquid::sleep(100);
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
            liquidAppWindowTitle = liquidAppName;
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

void LiquidAppWindow::setFrameless(const bool frameless)
{
    if (windowFlags().testFlag(Qt::FramelessWindowHint) == frameless) {
        return;
    }

    // Changing window flags hides the window, so it has to be shown again
    // (which is why visibility has to be checked beforehand)
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::FramelessWindowHint, frameless);
    if (wasVisible) {
        recreateNativeWindow();
    }
}

void LiquidAppWindow::setScrollBarsHidden(const bool hidden)
{
    if (settings()->testAttribute(QWebEngineSettings::ShowScrollBars) == !hidden) {
        return;
    }

    // Takes effect on the page that's currently shown, no reloading needed
    settings()->setAttribute(QWebEngineSettings::ShowScrollBars, !hidden);

    // ...except that the page keeps its existing scrollbars until something makes it lay them out again, so briefly
    // switch off scrolling of the document (which gets rid of its scrollbars), then put it back, a few frames later
    // (the original inline style is kept in a variable of Liquid's own JS world, which the page can't see,
    // and which goes away along with the document, should it get navigated away from in the meantime)
    if (isVisible()) {
        page()->runJavaScript(
            "(function() {"
                "if (window.liquidOverflow) return;"
                "const s = document.documentElement.style;"
                "window.liquidOverflow = [s.getPropertyValue('overflow'), s.getPropertyPriority('overflow')];"
                "s.setProperty('overflow', 'hidden', 'important');"
                "void document.documentElement.offsetHeight;"
            "})()",
            QWebEngineScript::ApplicationWorld
        );

        QTimer::singleShot(LQD_SCROLLBARS_RELAYOUT_DELAY, this, [this]() {
            page()->runJavaScript(
                "(function() {"
                    "if (!window.liquidOverflow) return;"
                    "const s = document.documentElement.style;"
                    "if (window.liquidOverflow[0]) {"
                        "s.setProperty('overflow', window.liquidOverflow[0], window.liquidOverflow[1]);"
                    "} else {"
                        "s.removeProperty('overflow');"
                    "}"
                    "delete window.liquidOverflow;"
                "})()",
                QWebEngineScript::ApplicationWorld
            );
        });
    }
}

void LiquidAppWindow::watchSessionStorage(QLocalSocket* connection)
{
    sessionStorageWatchers << QPointer<QLocalSocket>(connection);

    connect(connection, &QLocalSocket::disconnected, this, [this]() {
        sessionStorageWatchers.removeAll(QPointer<QLocalSocket>());
        for (int i = sessionStorageWatchers.size() - 1; i >= 0; i--) {
            if (sessionStorageWatchers[i]->state() != QLocalSocket::ConnectedState) {
                sessionStorageWatchers.removeAt(i);
            }
        }

        liquidAppLocalStorage->setWatchingSessionStorage(!sessionStorageWatchers.isEmpty());
    });

    // What's known so far, more comes as it changes
    sendSessionStorage(connection);
    liquidAppLocalStorage->setWatchingSessionStorage(true);
}

void LiquidAppWindow::sendSessionStorage(QLocalSocket* connection)
{
    QJsonObject sessionStorage;
    sessionStorage.insert("origin", liquidAppLocalStorage->getSessionStorageOrigin());
    sessionStorage.insert("items", liquidAppLocalStorage->getSessionStorageItems());

    connection->write(QByteArray(LQD_INSTANCE_MSG_SESSION_STORAGE " ")
                      + QJsonDocument(sessionStorage).toJson(QJsonDocument::Compact).toBase64() + "\n");
    connection->flush();
}

void LiquidAppWindow::removeSessionStorageItems(const QString& origin, const QStringList& keys)
{
    liquidAppLocalStorage->removeSessionStorageItems(origin, keys);
}
