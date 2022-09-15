#pragma once

#include <QAction>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QTableWidget>

#include "singleinstance.hpp"

class MainWindow : public QScrollArea
{
public:
    MainWindow();
    ~MainWindow();

    bool isAlreadyRunning(void);
    void run(void);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void bindShortcuts(void);
    void flushTable(void);
    void populateTable(void);
    void runLiquidApp(const QString liquidAppName);
    void saveSettings(void);

    QTableWidget* appListTable;
    QPushButton* createNewLiquidAppButton;
    QAction* quitAction;
    QSettings* settings;
    SingleInstance *singleInstance;
};
