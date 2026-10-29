#include <QApplication>
#include <QDir>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QUrl>
#include <QElapsedTimer>
#include <QScopedPointer>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QStyle>
#include <QVBoxLayout>

#include "liquid.hpp"
#include "liquidappfile.hpp"
#include "liquidappicon.hpp"
#include "liquidapplauncher.hpp"
#include "liquidappconfigwindow.hpp"
#include "lqd.h"
#include "mainwindow.hpp"
#include "singleinstance.hpp"

MainWindow::MainWindow() : QScrollArea()
{
    setWindowTitle(LQD_PROG_TITLE);

    setMinimumSize(LQD_WIN_MIN_SIZE_W, LQD_WIN_MIN_SIZE_H);
    setWidgetResizable(true);

    // Set window icon
#if !defined(Q_OS_LINUX) // This doesn't work on X11
    setWindowIcon(QIcon(":/images/" PROG_NAME ".svg"));
#endif

    settings = new QSettings(Liquid::getDataDir().filePath(PROG_NAME ".ini"), QSettings::IniFormat);
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
    if (LiquidAppLauncher::isSupported()) {
        appListTable->viewport()->installEventFilter(this);
    }
    mainWindowLayout->addWidget(appListTable);

    // Add new liquid app button
    createNewLiquidAppButton = new QPushButton(tr(LQD_ICON_ADD));
    createNewLiquidAppButton->setCursor(Qt::PointingHandCursor);
    connect(createNewLiquidAppButton, &QPushButton::clicked, [&]() {
        LiquidAppConfigDialog LiquidAppConfigDialog(this, "");
        switch (LiquidAppConfigDialog.exec()) {
            case QDialog::Accepted:
                // Give some time to the filesystem before scanning for the newly created Liquid App
                Liquid::sleep(100);
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

    // Run the liquid app upon double-click on its row in the table (or bring it to front if it's already running)
    connect(appListTable, &QTableWidget::cellDoubleClicked, [&](int row, int col) {
        Q_UNUSED(col);
        const QString liquidAppName = appListTable->item(row, 0)->text();

        // The token lets the Liquid app's window come to front on Wayland
        Liquid::requestActivationToken(this, [this, liquidAppName](const QString& activationToken) {
            if (isLiquidAppRunning(liquidAppName)) {
                sendToLiquidApp(liquidAppName, Liquid::makeRaiseMessage(activationToken));
            } else {
                Liquid::runLiquidApp(liquidAppName, activationToken);
            }
        });
    });

    // Connect keyboard shortcuts
    bindShortcuts();

    show();
    raise();
    activateWindow();

    // Fill the table
    populateTable();
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

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Not only the list's events come through here: QScrollArea filters its own widget's too (from before the list exists)
    if (appListTable != Q_NULLPTR && watched == appListTable->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            const QTableWidgetItem* item = appListTable->itemAt(mouseEvent->pos());

            draggedLiquidAppName.clear();
            if (mouseEvent->button() == Qt::LeftButton && item != Q_NULLPTR) {
                dragStartPosition = mouseEvent->pos();
                draggedLiquidAppName = appListTable->item(item->row(), 0)->text();
            }
        } else if (event->type() == QEvent::MouseMove && !draggedLiquidAppName.isEmpty()) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

            if (!(mouseEvent->buttons() & Qt::LeftButton)) {
                draggedLiquidAppName.clear();
            } else if ((mouseEvent->pos() - dragStartPosition).manhattanLength() >= QApplication::startDragDistance()) {
                const QString liquidAppName = draggedLiquidAppName;
                draggedLiquidAppName.clear();
                dragLiquidAppOut(liquidAppName);

                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            draggedLiquidAppName.clear();
        }
    }

    return QScrollArea::eventFilter(watched, event);
}

void MainWindow::dragLiquidAppOut(const QString& liquidAppName)
{
    QScopedPointer<QSettings> liquidAppConfig(LiquidAppFile::open(liquidAppName));

    // Whatever it gets dropped onto (the desktop, a folder) makes a copy of it
    const QString launcherFilePath = LiquidAppLauncher::createForDragging(liquidAppName, liquidAppConfig.data());
    if (launcherFilePath.isEmpty()) {
        return;
    }

    QMimeData* mimeData = new QMimeData;
    mimeData->setUrls(QList<QUrl>() << QUrl::fromLocalFile(launcherFilePath));

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    const QIcon icon = LiquidAppIcon::loadIcon(liquidAppConfig.data());
    const int iconSize = 48;
    drag->setPixmap(icon.pixmap(iconSize, iconSize));
    drag->setHotSpot(QPoint(iconSize / 2, iconSize / 2));
    drag->exec(Qt::CopyAction, Qt::CopyAction);
}

void MainWindow::flushTable(void)
{
    runButtons.clear();

    for (int i = appListTable->rowCount(); i > -1 ; i--) {
        appListTable->removeRow(i);
    }
}

void MainWindow::populateTable(void)
{
    foreach (const QString liquidAppName, Liquid::getLiquidAppsList()) {
        const int i = appListTable->rowCount();

        appListTable->insertRow(i);

        QSettings* liquidAppConfig = LiquidAppFile::open(liquidAppName);

        //////////////////
        // First column //
        //////////////////

        QTableWidgetItem* appItemWidgetFirstColumn = new QTableWidgetItem();
        // Make them read-only (no text edit upon double-click)
        appItemWidgetFirstColumn->setFlags(appItemWidgetFirstColumn->flags() ^ Qt::ItemIsEditable);
        appItemWidgetFirstColumn->setIcon(LiquidAppIcon::loadIcon(liquidAppConfig));
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
                // Close the Liquid app first (otherwise it'd write its file back to disk upon quitting)
                if (isLiquidAppRunning(liquidAppName) && !stopLiquidApp(liquidAppName, true)) {
                    QMessageBox::warning(this, tr("Unable to delete"),
                                         tr("Liquid app “%1” didn't quit (it may be busy), please try again.").arg(liquidAppName));
                    return;
                }

                LiquidAppLauncher::removeAll(liquidAppName);
                LiquidAppIcon::removeIconFile(liquidAppName);

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
                    Liquid::sleep(100);
                    flushTable();
                    populateTable();
                break;
            }
        });
        appItemLayout->addWidget(editButton);

        // Run/Stop button
        QPushButton* runButton = new QPushButton(tr(LQD_ICON_RUN), this);
        runButton->setCursor(Qt::PointingHandCursor);
        appItemLayout->addWidget(runButton);
        connect(runButton, &QPushButton::clicked, [this, liquidAppName]() {
            if (isLiquidAppRunning(liquidAppName)) {
                stopLiquidApp(liquidAppName, false);
            } else {
                Liquid::requestActivationToken(this, [liquidAppName](const QString& activationToken) {
                    Liquid::runLiquidApp(liquidAppName, activationToken);
                });
            }
        });
        runButtons.insert(liquidAppName, runButton);

        appListTable->setCellWidget(i, 1, appItemActionButtonsWidget);

        // Find out whether it's running (and keep track of it from now on)
        watchLiquidApp(liquidAppName);
        updateRunButton(liquidAppName);
    }
}

void MainWindow::updateLiquidAppIcon(const QString& liquidAppName)
{
    for (int i = 0; i < appListTable->rowCount(); i++) {
        QTableWidgetItem* item = appListTable->item(i, 0);

        if (item != Q_NULLPTR && item->text() == liquidAppName) {
            QScopedPointer<QSettings> liquidAppConfig(LiquidAppFile::open(liquidAppName));
            item->setIcon(LiquidAppIcon::loadIcon(liquidAppConfig.data()));
        }
    }
}

void MainWindow::watchLiquidApp(const QString& liquidAppName)
{
    if (liquidAppConnections.contains(liquidAppName)) {
        return;
    }

    // A Liquid app that's not in the table yet (e.g. just created via CLI)
    if (!runButtons.contains(liquidAppName) && Liquid::getLiquidAppsList().contains(liquidAppName)) {
        flushTable();
        populateTable();

        if (liquidAppConnections.contains(liquidAppName)) {
            return; // Populating the table has taken care of it
        }
    }

    QLocalSocket* connection = new QLocalSocket(this);
    liquidAppConnections.insert(liquidAppName, connection);

    connect(connection, &QLocalSocket::connected, this, [this, liquidAppName]() {
        updateRunButton(liquidAppName);
    });

    // Happens once the Liquid app quits or crashes (or right away, if it isn't running)
    const auto forget = [this, liquidAppName, connection]() {
        if (liquidAppConnections.value(liquidAppName) == connection) {
            liquidAppConnections.remove(liquidAppName);
            connection->deleteLater();
            updateRunButton(liquidAppName);
        }
    };
    connect(connection, &QLocalSocket::disconnected, this, forget);
    connect(connection, &QLocalSocket::errorOccurred, this, forget);

    connection->connectToServer(SingleInstance::serverName(LQD_INSTANCE_ID_APP_PREFIX + liquidAppName));
}

bool MainWindow::isLiquidAppRunning(const QString& liquidAppName)
{
    const QLocalSocket* connection = liquidAppConnections.value(liquidAppName, Q_NULLPTR);

    return connection != Q_NULLPTR && connection->state() == QLocalSocket::ConnectedState;
}

void MainWindow::sendToLiquidApp(const QString& liquidAppName, const QByteArray& message)
{
    if (isLiquidAppRunning(liquidAppName)) {
        QLocalSocket* connection = liquidAppConnections.value(liquidAppName);
        connection->write(message + "\n");
        connection->flush();
    }
}

bool MainWindow::stopLiquidApp(const QString& liquidAppName, const bool waitUntilStopped)
{
    sendToLiquidApp(liquidAppName, LQD_INSTANCE_MSG_QUIT);

    if (waitUntilStopped) {
        QElapsedTimer timer;
        timer.start();

        while (isLiquidAppRunning(liquidAppName) && timer.elapsed() < LQD_INSTANCE_STOP_TIMEOUT) {
            Liquid::sleep(50);
        }
    }

    return !isLiquidAppRunning(liquidAppName);
}

void MainWindow::updateRunButton(const QString& liquidAppName)
{
    QPushButton* runButton = runButtons.value(liquidAppName, Q_NULLPTR);

    if (runButton == Q_NULLPTR) {
        return;
    }

    const bool isRunning = isLiquidAppRunning(liquidAppName);

    runButton->setText((isRunning) ? tr(LQD_ICON_STOP) : tr(LQD_ICON_RUN));
    runButton->setToolTip((isRunning) ? tr("Stop") : tr("Run"));
    runButton->setProperty("class", (isRunning) ? "liquidAppsListButtonStop" : "liquidAppsListButtonRun");

    // Make the stylesheet take the new class into account
    runButton->style()->unpolish(runButton);
    runButton->style()->polish(runButton);
}

void MainWindow::saveSettings(void)
{
    settings->setValue(LQD_CFG_KEY_NAME_WIN_GEOM, QString(saveGeometry().toHex()));
    settings->sync();
}
