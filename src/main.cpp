#include <csignal>

#include <QApplication>
#include <QDir>
#include <QSettings>

#include "globals.h"

#include "liquidappcreateeditdialog.hpp"
#include "liquidappwindow.hpp"
#include "mainwindow.hpp"

static QSharedMemory *sharedMemory = nullptr;

LiquidAppWindow *liquidAppWindow;
MainWindow *mainWindow;

QString getUserName()
{
    QString name = qgetenv("USER");

    if (name.isEmpty()) {
        name = qgetenv("USERNAME");
    }

    return name;
}

static void onSignalHandler(int signum)
{
    if (sharedMemory) {
        delete sharedMemory;
        sharedMemory = nullptr;
    }

    if (liquidAppWindow) {
        liquidAppWindow->close();
        delete liquidAppWindow;
        liquidAppWindow = nullptr;
    }

    if (mainWindow) {
        mainWindow->close();
        delete mainWindow;
        mainWindow = nullptr;
    }

    qDebug() << "Terminated with signal" << signum;

    exit(128 + signum);
}

int main(int argc, char **argv)
{
#if defined(__GNUC__) && defined(Q_OS_LINUX)
    // Handle any further termination signals to ensure the
    // QSharedMemory block is deleted even if the process crashes
    signal(SIGHUP,  onSignalHandler);
    signal(SIGINT,  onSignalHandler);
    signal(SIGQUIT, onSignalHandler);
    signal(SIGILL,  onSignalHandler);
    signal(SIGABRT, onSignalHandler);
    signal(SIGFPE,  onSignalHandler);
    signal(SIGBUS,  onSignalHandler);
    signal(SIGSEGV, onSignalHandler);
    signal(SIGSYS,  onSignalHandler);
    signal(SIGPIPE, onSignalHandler);
    signal(SIGALRM, onSignalHandler);
    signal(SIGTERM, onSignalHandler);
    signal(SIGXCPU, onSignalHandler);
    signal(SIGXFSZ, onSignalHandler);
#endif

    // Account for running on high-DPI displays
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication app(argc, argv);

    // Set default Liquid window icon
    QIcon windowIcon(":/images/" PROG_NAME ".svg");
    app.setWindowIcon(windowIcon);

    // Process arguments
    if (argc < 2) {
        // // Allow only one instance
        sharedMemory = new QSharedMemory(getUserName() + "_Liquid");
        if (!sharedMemory->create(4, QSharedMemory::ReadOnly)) {
            delete sharedMemory;
            qDebug() << "Only one instance of Liquid is allowed";
            exit(-42);
        }

        // Show main program window
        mainWindow = new MainWindow;
    } else  { // App name provided
        QString liquidAppName(argv[1]);
        // Replace directory separators (slashes) with underscores
        // to ensure no sub-directories would get created
        liquidAppName = liquidAppName.replace(QDir::separator(), "_");

init:
        // Attempt to load Liquid app's config file
        QSettings *tempAppSettings = new QSettings(QSettings::IniFormat,
                                                   QSettings::UserScope,
                                                   QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                                   liquidAppName,
                                                   nullptr);
        // Attempt to load app settings from a config file
        if (tempAppSettings->contains(LQD_CFG_KEY_URL)) {
            // // Allow only one instance
            sharedMemory = new QSharedMemory(getUserName() + "_Liquid_app_" + liquidAppName);
            if (!sharedMemory->create(4, QSharedMemory::ReadOnly)) {
                delete sharedMemory;
                qDebug() << "Only one instance of Liquid app is allowed";
                exit(-42);
            }

            // Found existing liquid app settings file, show it
            liquidAppWindow = new LiquidAppWindow(&liquidAppName);
        } else {
            // No such Liquid app found, open Liquid app creation dialog
            LiquidAppCreateEditDialog liquidAppCreateEditDialog(mainWindow, liquidAppName);

            // Reveal Liquid app creation dialog
            liquidAppCreateEditDialog.show();
            switch (liquidAppCreateEditDialog.exec()) {
                case QDialog::Rejected:
                    // Exit the program
                    exit(0);
                break;

                case QDialog::Accepted:
                    // Ensure the new Liquid app's settings file is named based on the input field,
                    // and not on what was initially provided via CLI
                    liquidAppName = liquidAppCreateEditDialog.getName();
                    // Replace directory separators (slashes) with underscores
                    // to ensure no sub-directories would get created
                    liquidAppName = liquidAppName.replace(QDir::separator(), "_");

                    // Launch the newly created Liquid app
                    goto init;
                break;
            }
        }
    }

    int ret = app.exec();

    delete sharedMemory;
    sharedMemory = NULL;

    return ret;
}
