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

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void bindShortcuts(void);
    void flushTable(void);
    void populateTable(void);
    void runLiquidApp(const QString liquidAppName);
    void saveSettings(void);

    QSettings* settings;

    QAction* quitAction;

    QTableWidget* appListTable;
    QPushButton* createNewLiquidAppButton;
};
