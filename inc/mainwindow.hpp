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
    static QByteArray generateRandomByteArray(const int byteLength);
    void loadStyleSheet();
    void populateTable();
    void runLiquidApp(const QString liquidAppName);
    void saveSettings();

    QTableWidget* appListTable;
    QPushButton* createNewLiquidAppButton;
    QAction* quitAction;
    QSettings* settings;
};
