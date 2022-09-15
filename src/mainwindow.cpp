#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "liquid.hpp"
#include "liquidappconfigwindow.hpp"
#include "lqd.h"
#include "mainwindow.hpp"

MainWindow::MainWindow() : QScrollArea()
{
    {
        QString instanceName = QApplication::applicationName();
        singleInstance = new SingleInstance((QWidget*)this, &instanceName);
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::bindShortcuts(void)
{
    // Connect the exit shortcut
    quitAction = new QAction();
    quitAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_QUIT)));
    addAction(quitAction);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Remember window geometry and position
    saveSettings();

    event->accept();
}

void MainWindow::flushTable(void)
{
    for (int i = appListTable->rowCount(); i > -1 ; i--) {
        appListTable->removeRow(i);
    }
}

bool MainWindow::isAlreadyRunning(void)
{
    const bool raiseExisting = true;
    const bool isAnotherInstanceRunning = singleInstance->isAlreadyRunning(raiseExisting);

    return isAnotherInstanceRunning;
}

void MainWindow::populateTable(void)
{
    foreach (const QString liquidAppName, Liquid::getLiquidAppsList()) {
        const int i = appListTable->rowCount();

        appListTable->insertRow(i);

        QSettings* liquidAppConfig = new QSettings(QSettings::IniFormat,
                                                   QSettings::UserScope,
                                                   QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                                   liquidAppName,
                                                   Q_NULLPTR);

        //////////////////
        // First column //
        //////////////////

        QTableWidgetItem* appItemWidgetFirstColumn = new QTableWidgetItem();
        // Make them read-only (no text edit upon double-click)
        appItemWidgetFirstColumn->setFlags(appItemWidgetFirstColumn->flags() ^ Qt::ItemIsEditable);
        if (liquidAppConfig->contains(LQD_CFG_KEY_NAME_ICON)) {
            QPixmap pixmap;
            pixmap.loadFromData(QByteArray::fromBase64(liquidAppConfig->value(LQD_CFG_KEY_NAME_ICON).toByteArray().remove(0, 22)), "PNG");
            appItemWidgetFirstColumn->setIcon(QIcon(pixmap));
        } else {
            appItemWidgetFirstColumn->setIcon(QIcon(":/images/" PROG_NAME ".svg"));
        }
        appItemWidgetFirstColumn->setText(liquidAppName);
        appListTable->setItem(i, 0, appItemWidgetFirstColumn);

        ///////////////////
        // Second column //
        ///////////////////

        QWidget* appItemActionButtonsWidget = new QWidget(this);
        QHBoxLayout *appItemLayout = new QHBoxLayout();
        appItemLayout->setSpacing(0);
        appItemLayout->setContentsMargins(0, 0, 0, 0);
        appItemActionButtonsWidget->setLayout(appItemLayout);

        // Delete button
        QPushButton* deleteButton = new QPushButton(tr(LQD_ICON_DELETE), this);
        deleteButton->setCursor(Qt::PointingHandCursor);
        deleteButton->setProperty("class", "liquidAppsListButtonDelete");
        connect(deleteButton, &QPushButton::clicked, [this, liquidAppName, liquidAppConfig]() {
            const QString text = QString("Are you sure you want to delete Liquid app “%1”?").arg(liquidAppName);
            const QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirmation", text, QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                Liquid::removeDesktopFile(liquidAppName);

                // Shred and unlink Liquid app settings file
                QFile liquidAppConfigFile(liquidAppConfig->fileName());
                // Open file handle
                if (liquidAppConfigFile.open(QIODevice::ReadWrite)) {
                    // Determine file length
                    const int liquidAppConfigFileSize = liquidAppConfigFile.size();
                    // Shred (especially important if it contains Cookie data)
                    for (int i = 0, imax = 5; i < imax; i++) {
                        // Write randomly generated array of bytes to disk
                        liquidAppConfigFile.write(Liquid::generateRandomByteArray(liquidAppConfigFileSize), liquidAppConfigFileSize);
                        // Close file handle
                        liquidAppConfigFile.close();

                        if (i < imax - 1) {
                            // Put cursor back to start (to write again across the same byte range instead of appending data upon next iteration)
                            liquidAppConfigFile.open(QIODevice::ReadWrite);
                        } else {
                            // Unlink file
                            liquidAppConfigFile.remove();
                            qDebug().noquote() << QString("Removed config file for Liquid app %1").arg(liquidAppName);
                        }
                    }
                } else {
                    qDebug().noquote() << QString("Unable to open file %1 in Read/Write mode").arg(liquidAppConfig->fileName());
                }

                // Refresh table
                flushTable();
                populateTable();
            }
        });
        appItemLayout->addWidget(deleteButton);

        // Edit button
        QPushButton* editButton = new QPushButton(tr(LQD_ICON_EDIT), this);
        editButton->setCursor(Qt::PointingHandCursor);
        editButton->setProperty("class", "liquidAppsListButtonEdit");
        connect(editButton, &QPushButton::clicked, [this, liquidAppName]() {
            LiquidAppConfigDialog LiquidAppConfigDialog(this, liquidAppName);
            switch (LiquidAppConfigDialog.exec()) {
                case QDialog::Accepted:
                    // Give some time to the filesystem before scanning for the newly created app
                    {
                        QTime proceedAfter = QTime::currentTime().addMSecs(100);
                        while (QTime::currentTime() < proceedAfter) {
                            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                        }
                    }
                    flushTable();
                    populateTable();
                break;
            }
        });
        appItemLayout->addWidget(editButton);

        // Run button
        QPushButton* runButton = new QPushButton(tr(LQD_ICON_RUN), this);
        runButton->setCursor(Qt::PointingHandCursor);
        runButton->setProperty("class", "liquidAppsListButtonRun");
        appItemLayout->addWidget(runButton);
        connect(runButton, &QPushButton::clicked, [liquidAppName]() {
            Liquid::runLiquidApp(liquidAppName);
        });

        appListTable->setCellWidget(i, 1, appItemActionButtonsWidget);
    }
}

void MainWindow::run(void)
{
    setWindowTitle(LQD_PROG_TITLE);

    setMinimumSize(LQD_WIN_MIN_SIZE_W, LQD_WIN_MIN_SIZE_H);
    setWidgetResizable(true);

    // Set icon
#if !defined(Q_OS_LINUX) // This doesn't work on X11
    setWindowIcon(QIcon(":/images/" PROG_NAME ".svg"));
#endif

    settings = new QSettings(PROG_NAME, PROG_NAME);
    if (settings->contains(LQD_CFG_KEY_NAME_WIN_GEOM)) {
        QByteArray geometry = QByteArray::fromHex(
            settings->value(LQD_CFG_KEY_NAME_WIN_GEOM).toByteArray()
        );
        restoreGeometry(geometry);
    }

    Liquid::applyQtStyleSheets(this);

    QWidget* mainWindowWidget = new QWidget();
    setWidget(mainWindowWidget);

    QVBoxLayout* mainWindowLayout = new QVBoxLayout();
    mainWindowLayout->setSpacing(0);
    mainWindowLayout->setContentsMargins(0, 0, 0, 0);

    appListTable = new QTableWidget();
    appListTable->setColumnCount(2);
    appListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    appListTable->horizontalHeader()->hide();
    appListTable->verticalHeader()->hide();
    appListTable->setShowGrid(false);
    appListTable->setFocusPolicy(Qt::NoFocus);
    // appListTable->setSelectionMode(QAbstractItemView::SingleSelection);
    appListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainWindowLayout->addWidget(appListTable);

    // Add new liquid app button
    createNewLiquidAppButton = new QPushButton(tr(LQD_ICON_ADD));
    createNewLiquidAppButton->setCursor(Qt::PointingHandCursor);
    connect(createNewLiquidAppButton, &QPushButton::clicked, [&]() {
        LiquidAppConfigDialog LiquidAppConfigDialog(this, "");
        switch (LiquidAppConfigDialog.exec()) {
            case QDialog::Accepted:
                // Give some time to the filesystem before scanning for the newly created Liquid App
                {
                    QTime proceedAfter = QTime::currentTime().addMSecs(100);
                    while (QTime::currentTime() < proceedAfter) {
                        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                    }
                }
                flushTable();
                populateTable();

                if (LiquidAppConfigDialog.isPlanningToRun()) {
                    Liquid::runLiquidApp(LiquidAppConfigDialog.getName());
                }
            break;
        }
    });
    mainWindowLayout->addWidget(createNewLiquidAppButton);

    mainWindowWidget->setLayout(mainWindowLayout);

    // Run the liquid app upon double-click on its row in the table
    connect(appListTable, &QTableWidget::cellDoubleClicked, [&](int row, int col) {
        Q_UNUSED(col);
        Liquid::runLiquidApp(appListTable->item(row, 0)->text());
    });

    // Reveal Liquid's main window
    show();
    raise();
    activateWindow();

    // Connect keyboard shortcuts
    bindShortcuts();

    // Fill the table
    populateTable();
}

void MainWindow::saveSettings(void)
{
    settings->setValue(LQD_CFG_KEY_NAME_WIN_GEOM, QString(saveGeometry().toHex()));
    settings->sync();
}
