#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QVBoxLayout>

#include "globals.h"

#include "liquidappcreateeditdialog.hpp"
#include "mainwindow.hpp"

MainWindow::MainWindow() : QScrollArea()
{
    setWindowTitle(LQD_PROG_TITLE);

    setMinimumSize(LQD_WIN_MIN_SIZE_W, LQD_WIN_MIN_SIZE_H);
    setWidgetResizable(true);

    settings = new QSettings(PROG_NAME, PROG_NAME);
    if (settings->contains(LQD_CFG_KEY_WIN_GEOM)) {
        QByteArray geometry = QByteArray::fromHex(
            settings->value(LQD_CFG_KEY_WIN_GEOM).toByteArray()
        );
        restoreGeometry(geometry);
    }

    loadStyleSheet();

    QWidget* widget = new QWidget();
    setWidget(widget);

    QVBoxLayout* layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(0);

    appListTable = new QTableWidget();
    appListTable->setColumnCount(2);
    appListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    appListTable->horizontalHeader()->hide();
    appListTable->verticalHeader()->hide();
    appListTable->setShowGrid(false);
    appListTable->setFocusPolicy(Qt::NoFocus);
    // appListTable->setSelectionMode(QAbstractItemView::SingleSelection);
    appListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(appListTable);

    // Add new liquid app button
    createNewLiquidAppButton = new QPushButton(tr(LQD_ICON_ADD));
    createNewLiquidAppButton->setCursor(Qt::PointingHandCursor);
    createNewLiquidAppButton->setFlat(true);
    QObject::connect(createNewLiquidAppButton, &QPushButton::clicked, [=]() {
        LiquidAppCreateEditDialog liquidAppCreateEditDialog(this, "");
        switch (liquidAppCreateEditDialog.exec()) {
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
    layout->addWidget(createNewLiquidAppButton);

    widget->setLayout(layout);

    // Launch the liquid app upon double-click on its row in the table
    QObject::connect(appListTable, &QTableWidget::cellDoubleClicked, [=](int r, int c) {
        (void)(c);
        launchLiquidApp(appListTable->item(r, 0)->text());
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
    saveSettings();
}

void MainWindow::bindShortcuts()
{
    // Connect the exit shortcut
    quitAction.setShortcut(QKeySequence(tr(LQD_KBD_SEQ_QUIT)));
    addAction(&quitAction);
    connect(&quitAction, SIGNAL(triggered()), this, SLOT(close()));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Remember window position
    saveSettings();

    event->accept();
}

void MainWindow::createDesktopFile(const QString liquidAppName, const QUrl liquidAppStartingUrl)
{
#ifdef Q_OS_LINUX
    // Compose content
    QString context = "#!/usr/bin/env xdg-open\n\n";
    context += "[Desktop Entry]\n";
    context += "Type=Application\n";
    context += "Name=" + liquidAppName + "\n";
    context += "Icon=internet-web-browser\n";
    context += "Exec=liquid " + liquidAppName + "\n";
    context += "Comment=" + liquidAppStartingUrl.toString() + "\n";
    context += "Categories=Network;WebBrowser;\n";

    // Construct directory path
    // QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString desktopPath = QDir::homePath() + QDir::separator() + "Desktop";

    // Check if the desktop path exists
    QDir dir(desktopPath);
    if (dir.exists()) {
        // Create file
        QFile file(desktopPath + QDir::separator() + liquidAppName + ".desktop");
        file.open(QIODevice::WriteOnly);
        file.write(context.toStdString().c_str());
        file.setPermissions(QFileDevice::ReadUser
                           |QFileDevice::WriteUser
                           |QFileDevice::ExeUser
                           |QFileDevice::ReadGroup
                           |QFileDevice::ReadOther);
        file.flush();
        file.close();
    }
#endif
}

void MainWindow::flushTable()
{
    for (int i = appListTable->rowCount(); i > -1 ; i--) {
        appListTable->removeRow(i);
    }
}

QByteArray MainWindow::generateRandomByteArray(const int byteLength)
{
    std::vector<quint32> buf;

    qsrand(QTime::currentTime().msec());

    for(int i = 0; i < byteLength; ++i) {
        buf.push_back(qrand());
    }

    return QByteArray(reinterpret_cast<const char*>(buf.data()), byteLength);
}

QString MainWindow::getLiquidAppsDirPath()
{
    QSettings *dummyLiquidAppSettings = new QSettings(QSettings::IniFormat,
                                                      QSettings::UserScope,
                                                      QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                                      "dummy597a32d8cf3b253",
                                                      nullptr);
    QFileInfo dummyLiquidAppFileInfo(dummyLiquidAppSettings->fileName());

    return dummyLiquidAppFileInfo.absolutePath();
}

void MainWindow::launchLiquidApp(const QString liquidAppName)
{
    const QString liquidAppFilePath(QCoreApplication::applicationFilePath());
    QProcess process;

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    process.setProgram(liquidAppFilePath);
    process.setArguments(QStringList() << QStringLiteral("%1").arg(liquidAppName));
    process.startDetached();
#else
    process.startDetached(liquidAppFilePath, QStringList() << QStringLiteral("%1").arg(liquidAppName));
#endif
}

void MainWindow::loadStyleSheet()
{
    QString styleSheet;

    // Load built-in stylesheet
    QFile styleFile(":/styles/" PROG_NAME ".qss");
    styleFile.open(QFile::ReadOnly);
    styleSheet = QLatin1String(styleFile.readAll());
    styleFile.close();

    // Load custom stylesheet
    QSettings *settings = new QSettings(QSettings::IniFormat,
                                        QSettings::UserScope,
                                        PROG_NAME,
                                        PROG_NAME,
                                        nullptr);
    QFileInfo settingsFileInfo(settings->fileName());
    QFile customStyleFile(settingsFileInfo.absolutePath() + QDir::separator() + PROG_NAME ".qss");
    if (customStyleFile.open(QFile::ReadOnly)) {
        styleSheet += QLatin1String(customStyleFile.readAll());
        customStyleFile.close();
    }

    setStyleSheet(styleSheet);
}

void MainWindow::populateTable()
{
    QDir appsDir(getLiquidAppsDirPath());

    QFileInfoList liquidAppFiles = appsDir.entryInfoList(QStringList() << "*.ini",
                                                         QDir::Files | QDir::NoDotAndDotDot,
                                                         QDir::Name | QDir::IgnoreCase);

    foreach (QFileInfo liquidAppFile, liquidAppFiles) {
        int i = appListTable->rowCount();

        appListTable->insertRow(i);

        QString liquidAppName = liquidAppFile.completeBaseName();
        QSettings *liquidAppSettings = new QSettings(QSettings::IniFormat,
                                                     QSettings::UserScope,
                                                     QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                                     liquidAppName,
                                                     nullptr);

        //////////////////
        // First column //
        //////////////////

        QTableWidgetItem *appItemWidgetFirstColumn = new QTableWidgetItem();
        // Make them read-only (no text edit upon double-click)
        appItemWidgetFirstColumn->setFlags(appItemWidgetFirstColumn->flags() ^ Qt::ItemIsEditable);
        QIcon liquidAppIcon(":/images/" PROG_NAME ".svg");
        if (liquidAppSettings->contains(LQD_CFG_KEY_ICON)) {
            QByteArray byteArray = QByteArray::fromHex(
                liquidAppSettings->value(LQD_CFG_KEY_ICON).toByteArray()
            );
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::ReadOnly);
            QDataStream in(&buffer);
            in >> liquidAppIcon;
            buffer.close();
        }
        appItemWidgetFirstColumn->setIcon(liquidAppIcon);
        appItemWidgetFirstColumn->setText(liquidAppName);
        appListTable->setItem(i, 0, appItemWidgetFirstColumn);

        ///////////////////
        // Second column //
        ///////////////////

        QWidget *appItemActionButtonsWidget = new QWidget();
        QHBoxLayout *appItemLayout = new QHBoxLayout();
        appItemLayout->setSpacing(0);
        appItemLayout->setMargin(0);
        appItemActionButtonsWidget->setLayout(appItemLayout);

        // Delete button
        QPushButton *deleteButton = new QPushButton(tr(LQD_ICON_REMOVE));
        deleteButton->setCursor(Qt::PointingHandCursor);
        deleteButton->setProperty("class", "btnDelete");
        QObject::connect(deleteButton, &QPushButton::clicked, [=]() {
            const QString text = QString("Are you sure you want to remove Liquid app “%1”?").arg(liquidAppName);
            const QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirmation", text, QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                removeDesktopFile(liquidAppName);

                // Shred and unlink Liquid app settings file
                QFile liquidAppSettingsFile(liquidAppSettings->fileName());
                // Open file handle
                if (liquidAppSettingsFile.open(QIODevice::ReadWrite)) {
                    // Determine file length
                    const int liquidAppSettingsFileSize = liquidAppSettingsFile.size();
                    // Shred (especially important if it contains Cookie data)
                    for (int i = 0, imax = 5; i < imax; i++) {
                        // Write randomly generated array of bytes to disk
                        liquidAppSettingsFile.write(generateRandomByteArray(liquidAppSettingsFileSize), liquidAppSettingsFileSize);
                        // Close file handle
                        liquidAppSettingsFile.close();

                        if (i < imax - 1) {
                            // Put cursor back to start (to write again across the same byte range instead of appending data upon next iteration)
                            liquidAppSettingsFile.open(QIODevice::ReadWrite);
                        } else {
                            // Unlink file
                            liquidAppSettingsFile.remove();
                            qDebug().noquote() << QString("Removed config file for Liquid app %1").arg(liquidAppName);
                        }
                    }
                } else {
                    qDebug().noquote() << QString("Unable to open file %1 in Read/Write mode").arg(liquidAppSettings->fileName());
                }

                // Refresh table
                flushTable();
                populateTable();
            }
        });
        appItemLayout->addWidget(deleteButton);

        // Edit button
        QPushButton *editButton = new QPushButton(tr(LQD_ICON_EDIT));
        editButton->setCursor(Qt::PointingHandCursor);
        editButton->setProperty("class", "btnEdit");
        QObject::connect(editButton, &QPushButton::clicked, [=]() {
            LiquidAppCreateEditDialog liquidAppCreateEditDialog(this, liquidAppName);
            switch (liquidAppCreateEditDialog.exec()) {
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

        // Launch button
        QPushButton *runButton = new QPushButton(tr(LQD_ICON_RUN));
        runButton->setCursor(Qt::PointingHandCursor);
        runButton->setProperty("class", "btnRun");
        appItemLayout->addWidget(runButton);
        QObject::connect(runButton, &QPushButton::clicked, [=]() {
            launchLiquidApp(liquidAppName);
        });

        appListTable->setCellWidget(i, 1, appItemActionButtonsWidget);
    }
}

void MainWindow::saveSettings()
{
    settings->setValue(LQD_CFG_KEY_WIN_GEOM, QString(saveGeometry().toHex()));
    settings->sync();
}

void MainWindow::removeDesktopFile(const QString liquidAppName)
{
#ifdef Q_OS_LINUX
    // Construct directory path
    // QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString desktopPath = QDir::homePath() + QDir::separator() + "Desktop";

    // Check if the desktop path exists
    QDir dir(desktopPath);
    if (dir.exists()) {
        // Unlink file
        QFile file(desktopPath + QDir::separator() + liquidAppName + ".desktop");
        file.remove();
    }
#endif
}
