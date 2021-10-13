#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QMenu>
#include <QSettings>
#include <QShortcut>
#include <QtWebEngineWidgets/QWebEngineScript>
#include <QtWebEngineWidgets/QWebEngineScriptCollection>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEngineCookieStore>
#include <QWebEngineFullScreenRequest>

#include "liquidappwebpage.hpp"

class LiquidAppWindow : public QWebEngineView
{
    Q_OBJECT

public:
    explicit LiquidAppWindow(QString* name);
    ~LiquidAppWindow();

    QSettings* liquidAppSettings;

public slots:
    void exitFullScreen();
    void loadFinished(bool ok);
    void onIconChanged(QIcon icon);
    void refresh();
    void reset();
    void toggleFullScreen();
    void toggleWindowGeometryLock();
    void updateWindowTitle(QString title);
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
    LiquidAppWebPage* liquidAppWebPage;
    QWebEngineProfile* liquidAppWebProfile;
    QByteArray liquidAppWindowGeometry;
    QString liquidAppWindowTitle;

    bool isLoading = false;
    bool isWindowGeometryLocked = false;

    // Keyboard shortcuts
    QAction* backAction;
    QAction* backAction2;
    QAction* forwardAction;
    QAction* fullScreenAction;
    QAction* fullScreenAction2;
    QAction* fullScreenExitAction;
    QAction* quitAction;
    QAction* quitAction2;
    QAction* reloadAction;
    QAction* reloadAction2;
    QAction* resetAction;
    QAction* toggleGeometryLockAction;
    QAction* zoomInAction;
    QAction* zoomOutAction;
    QAction* zoomResetAction;

    // Context menu
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
