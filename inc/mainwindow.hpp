#pragma once

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

class MainWindow : public QScrollArea
{
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    static void createDesktopFile(const QString liquidAppName, const QUrl liquidAppStartingUrl);
    static void removeDesktopFile(const QString liquidAppName);

protected:
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void bindShortcuts();
    QString getLiquidAppsDirPath();
    void flushTable();
    void launchLiquidApp(QString liquidAppName);
    void loadStyleSheet();
    void populateTable();

    QTableWidget *appListTable;
    QPushButton *createNewLiquidAppButton;
    QSettings *settings;
    QAction quitAction;
};
