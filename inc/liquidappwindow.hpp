#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QMenu>
#include <QSettings>
#include <QShortcut>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWebEngineFullScreenRequest>

#include "liquidappwebpage.hpp"

class LiquidAppWindow : public QWebEngineView
{
    Q_OBJECT

public:
    explicit LiquidAppWindow(QString* name);
    ~LiquidAppWindow();

    QSettings* liquidAppConfig;

public slots:
    void exitFullScreenMode();
    void hardReload();
    void loadFinished();
    void loadStarted();
    void onIconChanged(QIcon icon);
    void stopLoadingOrExitFullScreenMode();
    void toggleFullScreenMode();
    void toggleWindowGeometryLock();
    void updateWindowTitle(QString title = "");
    void zoomIn();
    void zoomOut();
    void zoomReset();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool handleWheelEvent(QWheelEvent* event);
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void zoomBy(qreal factor);
    void contextMenuEvent(QContextMenuEvent* event);

private:
    QString* liquidAppName;

    QString liquidAppWindowTitle;

    LiquidAppWebPage* liquidAppWebPage;
    QWebEngineProfile* liquidAppWebProfile;
    QByteArray liquidAppWindowGeometry;

    bool liquidAppWindowTitleIsReadOnly = false;
    bool pageIsLoading = false;
    bool windowGeometryIsLocked = false;

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
    void setupContextMenu(void);
    void setWebSettingsToDefault(QWebEngineSettings* webSettings);
};
