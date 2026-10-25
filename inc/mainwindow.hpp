#pragma once

#include <QAction>
#include <QLocalSocket>
#include <QMap>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QTableWidget>

class MainWindow : public QScrollArea
{
public:
    MainWindow();
    ~MainWindow();

    // Starts keeping track of the Liquid app by the given name (called when Liquid apps report being started)
    void watchLiquidApp(const QString& liquidAppName);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void bindShortcuts(void);
    void flushTable(void);
    void populateTable(void);
    void saveSettings(void);

    bool isLiquidAppRunning(const QString& liquidAppName);
    void sendToLiquidApp(const QString& liquidAppName, const QByteArray& message);
    bool stopLiquidApp(const QString& liquidAppName, const bool waitUntilStopped);
    void updateRunButton(const QString& liquidAppName);

    QTableWidget* appListTable;
    QPushButton* createNewLiquidAppButton;
    QSettings* settings;

    QAction* quitAction;

    // Connections to running Liquid apps (they get closed by the system once a Liquid app quits or crashes)
    QMap<QString, QLocalSocket*> liquidAppConnections;
    // Run/Stop buttons of Liquid apps currently listed in the table
    QMap<QString, QPushButton*> runButtons;
};
