#pragma once

#include <QAction>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QTableWidget>

class MainWindow : public QScrollArea
{
public:
    MainWindow();
    ~MainWindow();

    static void createDesktopFile(const QString liquidAppName, const QUrl liquidAppStartingUrl);
    static void removeDesktopFile(const QString liquidAppName);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void bindShortcuts();
    QString getLiquidAppsDirPath();
    void flushTable();
    void launchLiquidApp(const QString liquidAppName);
    void loadStyleSheet();
    void populateTable();
    void saveSettings();
    static QByteArray generateRandomByteArray(const int byteLength);

    QTableWidget *appListTable;
    QPushButton *createNewLiquidAppButton;
    QSettings *settings;
    QAction quitAction;
};
