#include <csignal>
#include <QApplication>
#include <QSettings>

#include "config.h"
#include "liquidappcreateeditdialog.hpp"
#include "liquidappwindow.hpp"
#include "mainwindow.hpp"

static QSharedMemory *sharedMemory = NULL;

QString getUserName()
{
    QString name = qgetenv("USER");

    if (name.isEmpty())
        name = qgetenv("USERNAME");

    return name;
}

static void onSignalHandler(int signum)
{
    if (sharedMemory) {
        delete sharedMemory;
        sharedMemory = NULL;
    }

    qDebug() << "Terminated with signal" << signum;

    exit(128 + signum);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MainWindow mainWindow;
    LiquidAppWindow liquidAppWindow;

    // Set default Liquid window icon
    QIcon windowIcon(":/images/" PROG_NAME ".svg");
    app.setWindowIcon(windowIcon);

    // Handle any further termination signals to ensure the
    // QSharedMemory block is deleted even if the process crashes
    signal(SIGHUP,  onSignalHandler); // 1
    signal(SIGINT,  onSignalHandler); // 2
    signal(SIGQUIT, onSignalHandler); // 3
    signal(SIGILL,  onSignalHandler); // 4
    signal(SIGABRT, onSignalHandler); // 6
    signal(SIGFPE,  onSignalHandler); // 8
    signal(SIGBUS,  onSignalHandler); // 10
    signal(SIGSEGV, onSignalHandler); // 11
    signal(SIGSYS,  onSignalHandler); // 12
    signal(SIGPIPE, onSignalHandler); // 13
    signal(SIGALRM, onSignalHandler); // 14
    signal(SIGTERM, onSignalHandler); // 15
    signal(SIGXCPU, onSignalHandler); // 24
    signal(SIGXFSZ, onSignalHandler); // 25

    // Process arguments
    if (argc < 2) {
        // // Allow only one instance
        sharedMemory = new QSharedMemory(getUserName() + "_Liquid");
        if (!sharedMemory->create(4, QSharedMemory::ReadOnly)) {
            delete sharedMemory;
            qDebug() << "Only one instance of Liquid is allowed";
            exit(-42);
        }

        mainWindow.show();
    } else  { // App name provided
        QString liquidAppName(argv[1]);
        // Replace directory separators (slashes) with underscores
        // to ensure no sub-directories would get created
        liquidAppName = liquidAppName.replace(QDir::separator(), "_");

init:
        // Attempt to load Liquid app's config file
        QSettings *tempAppSettings = new QSettings(QSettings::IniFormat,
                                                   QSettings::UserScope,
                                                   CONFIG_APPS_PATH,
                                                   liquidAppName,
                                                   nullptr);
        // Attempt to load app settings from a config file
        if (tempAppSettings->contains(SETTINGS_KEY_URL)) {
            // // Allow only one instance
            sharedMemory = new QSharedMemory(getUserName() + "_Liquid_app_" + liquidAppName);
            if (!sharedMemory->create(4, QSharedMemory::ReadOnly)) {
                delete sharedMemory;
                qDebug() << "Only one instance of Liquid app is allowed";
                exit(-42);
            }

            // Found existing liquid app settings file, show it
            liquidAppWindow.runLiquidApp(&liquidAppName);

            // TODO: check if this Liquid app is already running (allow only one instance)
        } else {
            // No such Liquid app found, open Liquid app creation dialog
            LiquidAppCreateEditDialog liquidAppCreateEditDialog(&mainWindow, liquidAppName);

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
