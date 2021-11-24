#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QMenu>
#include <QNetworkProxy>
#include <QSettings>
#include <QShortcut>
#include <QWebEngineView>
#include <QWebEngineFullScreenRequest>

#include "liquidappwebpage.hpp"

class LiquidAppWebPage;

class LiquidAppWindow : public QWebEngineView
{
    Q_OBJECT

public:
    explicit LiquidAppWindow(QString* name);
    ~LiquidAppWindow(void);

    void setForgiveNextPageLoadError(const bool ok);

    QSettings* liquidAppConfig;

public slots:
    void certificateError(void);
    void exitFullScreenMode(void);
    void hardReload(void);
    void loadFinished(bool ok);
    void loadStarted(void);
    void onIconChanged(QIcon icon);
    void stopLoadingOrExitFullScreenMode(void);
    void takeSnapshotSlot(void);
    void takeSnapshotFullPageSlot(void);
    void toggleFullScreenMode(void);
    void toggleWindowGeometryLock(void);
    void updateWindowTitle(const QString title);
    void zoomIn(void);
    void zoomOut(void);
    void zoomReset(void);

protected:
    void attemptToSetZoomFactorTo(const qreal desiredZoomFactor);
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool handleWheelEvent(QWheelEvent* event);
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void takeSnapshot(const bool fullPage);
    const QString colorToRgba(const QColor color);

    QString* liquidAppName;

    QString liquidAppWindowTitle;

    LiquidAppWebPage* liquidAppWebPage = Q_NULLPTR;
    QWebEngineProfile* liquidAppWebProfile = Q_NULLPTR;
    QByteArray liquidAppWindowGeometry;

    bool liquidAppWindowTitleIsReadOnly = false;
    bool forgiveNextPageLoadError = false;
    bool pageHasCertificateError = false;
    bool pageHasError = false;
    bool pageIsLoading = false;
    bool windowGeometryIsLocked = false;
    QList<qreal> zoomFactors;

    QNetworkProxy* proxy = Q_NULLPTR;

    // Keyboard shortcuts' actions
    QAction* backAction;
    QAction* backAction2;
    QAction* forwardAction;
    QAction* hardReloadAction;
    QAction* muteAudioAction;
    QAction* quitAction;
    QAction* quitAction2;
    QAction* reloadAction;
    QAction* reloadAction2;
    QAction* stopLoadingOrExitFullScreenModeAction;
    QAction* takeSnapshotAction;
    QAction* takeSnapshotFullPageAction;
    QAction* toggleFullScreenModeAction;
    QAction* toggleFullScreenModeAction2;
    QAction* toggleGeometryLockAction;
    QAction* zoomInAction;
    QAction* zoomOutAction;
    QAction* zoomResetAction;

    // Context menu and its actions
    QMenu* contextMenu;
    QAction* contextMenuCopyUrlAction;
    QAction* contextMenuReloadAction;
    QAction* contextMenuBackAction;
    QAction* contextMenuForwardAction;
    QAction* contextMenuCloseAction;

    void bindKeyboardShortcuts(void);
    void loadLiquidAppConfig(void);
    void saveLiquidAppConfig(void);
    void setupContextMenu(void);
};
