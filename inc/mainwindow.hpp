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
    void flushTable();
    void loadStyleSheet();
    void populateTable();
    void runLiquidApp(const QString liquidAppName);
    void saveSettings();

    QSettings* settings;

    QAction* quitAction;

    QTableWidget* appListTable;
    QPushButton* createNewLiquidAppButton;
};
