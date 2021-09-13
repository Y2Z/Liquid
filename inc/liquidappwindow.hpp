#pragma once

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QSettings>
#include <QShortcut>
#include <QtWebKit>
#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebView>

class LiquidAppWindow : public QWebView
{
    Q_OBJECT

public:
    explicit LiquidAppWindow(QWidget *parent = 0);
    ~LiquidAppWindow();

    void runLiquidApp(QString *name);
    QSettings *liquidAppSettings;

public slots:
    void exitFullScreen();
    void loadFinished(bool ok);
    void refresh();
    void reset();
    void toggleFullScreen();
    void toggleWindowGeometryLock();
    void updateWindowTitle(QString title);
    void zoomIn();
    void zoomOut();
    void zoomReset();

protected:
    void bindShortcuts();
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void zoomBy(qreal factor);

private:
    QString liquidAppWindowTitle;
    QString *liquidAppName;
    QByteArray liquidAppWindowGeometry;

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
};
