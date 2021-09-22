#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QSettings>
#include <QShortcut>
#include <QtWebEngineWidgets/QWebEngineScript>
#include <QtWebEngineWidgets/QWebEngineScriptCollection>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEngineCookieStore>

#include "liquidappwebpage.hpp"

class LiquidAppWindow : public QWebEngineView
{
    Q_OBJECT

public:
    explicit LiquidAppWindow(QString *name);
    ~LiquidAppWindow();

    QSettings *liquidAppSettings;

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
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool handleWheelEvent(QWheelEvent *event);
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void zoomBy(qreal factor);

private:
    QString *liquidAppName;
    LiquidAppWebPage *liquidAppWebPage;
    QWebEngineProfile *liquidAppWebProfile;
    QByteArray liquidAppWindowGeometry;
    QString liquidAppWindowTitle;

    bool isLoading = false;
    bool isWindowGeometryLocked = false;

    QAction backAction;
    QAction backAction2;
    QAction forwardAction;
    QAction fullScreenAction;
    QAction fullScreenAction2;
    QAction fullScreenExitAction;
    QAction quitAction;
    QAction quitAction2;
    QAction reloadAction;
    QAction reloadAction2;
    QAction resetAction;
    QAction toggleGeometryLockAction;
    QAction zoomInAction;
    QAction zoomOutAction;
    QAction zoomResetAction;

    void bindShortcuts();
    void setWebSettingsToDefault(QWebEngineSettings *webSettings);
};
