#include <csignal>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>

#include "lqd.h"
#include "liquid.hpp"
#include "liquidappconfigwindow.hpp"
#include "liquidappwindow.hpp"
#include "mainwindow.hpp"

QTextStream cout(stdout);

static QSharedMemory* sharedMemory = Q_NULLPTR;

LiquidAppWindow* liquidAppWindow;
MainWindow* mainWindow;

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
        sharedMemory = Q_NULLPTR;
    }

    if (liquidAppWindow) {
        liquidAppWindow->close();
        delete liquidAppWindow;
        liquidAppWindow = Q_NULLPTR;
    }

    if (mainWindow) {
        mainWindow->close();
        delete mainWindow;
        mainWindow = Q_NULLPTR;
    }

    qDebug() << "Terminated with signal" << signum;

    exit(128 + signum);
}

int main(int argc, char **argv)
{
    int ret = EXIT_SUCCESS;

#if defined(__GNUC__) && defined(Q_OS_LINUX)
    // Handle any further termination signals to ensure
    // that the QSharedMemory block is deleted
    // even if the process crashes
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
    // Make pixmaps assume high-DPI scaling by default
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Initialize application
    QApplication app(argc, argv);

    if (argc < 2) {
        // Allow only one instance
        sharedMemory = new QSharedMemory(getUserName() + "_Liquid");
        if (!sharedMemory->create(4, QSharedMemory::ReadOnly)) {
            delete sharedMemory;
            qDebug().noquote() << QString("Only one instance of Liquid is allowed");
            exit(EXIT_FAILURE);
        }

        // Show main program window
        mainWindow = new MainWindow;
    } else  { // App name provided
        // CLI flags and options
        QCommandLineParser parser;
        QCoreApplication::setApplicationName(PROG_NAME);
        QCoreApplication::setApplicationVersion(VERSION);

        // parser.setApplicationDescription("Test helper");
        parser.setApplicationDescription("Test helper");
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addPositionalArgument("app-name", QCoreApplication::translate("main", "Liquid App name"));

        // Set up CLI flags and options
        const QCommandLineOption listAppsFlag(QStringList() << "l" << "list-apps",
                QCoreApplication::translate("main", "List all available Liquid Apps"));
        parser.addOption(listAppsFlag);
        const QCommandLineOption editAppDialogFlag(QStringList() << "E" << "edit-app-dialog",
                QCoreApplication::translate("main", "Open edit Liquid App dialog"));
        parser.addOption(editAppDialogFlag);

        // Process the actual command line arguments given by the user
        parser.process(app);

        const QStringList args = parser.positionalArguments();
        QString liquidAppName = (args.size() > 0) ? args.at(0) : "";

        // Replace directory separators (slashes) with underscores
        // to ensure no sub-directories would get created
        liquidAppName = liquidAppName.replace(QDir::separator(), "_");

        // TODO: look for -c,-e, -d flags here

        // Process the -l/--list-apps flag
        if (parser.isSet(listAppsFlag)) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            cout << Liquid::getLiquidAppsList().join("\n") << Qt::endl;
#else
            cout << Liquid::getLiquidAppsList().join("\n") << endl;
#endif
            return ret;
        }

attempt_to_create_or_run_liquid_app:
        // Attempt to load Liquid app's config file
        QSettings* tempAppSettings = new QSettings(QSettings::IniFormat,
                                                   QSettings::UserScope,
                                                   QString(PROG_NAME) + QDir::separator() + LQD_APPS_DIR_NAME,
                                                   liquidAppName,
                                                   Q_NULLPTR);

        // Attempt to load app settings from a config file
        if (!parser.isSet(editAppDialogFlag) && tempAppSettings->contains(LQD_CFG_KEY_NAME_URL)) {
            // // Allow only one instance
            sharedMemory = new QSharedMemory(getUserName() + "_Liquid_app_" + liquidAppName);
            if (!sharedMemory->create(4, QSharedMemory::ReadOnly)) {
                delete sharedMemory;
                qDebug().noquote() << QString("Only one instance of Liquid app “%1” is allowed").arg(liquidAppName);
                exit(EXIT_FAILURE);
            }

            // Found existing liquid app settings file, show it
            liquidAppWindow = new LiquidAppWindow(&liquidAppName);
        } else {
            // No such Liquid app found, open Liquid app creation dialog
            LiquidAppConfigDialog LiquidAppConfigDialog(mainWindow, liquidAppName);
            LiquidAppConfigDialog.setPlanningToRun(true); // Make run after it's created

            // Reveal Liquid app creation dialog
            LiquidAppConfigDialog.show();
            switch (LiquidAppConfigDialog.exec()) {
                case QDialog::Rejected:
                    // Exit the program
                    goto done;
                break;

                case QDialog::Accepted:
                    // Ensure the new Liquid app's settings file is named based on the input field,
                    // and not on what was initially provided via CLI
                    liquidAppName = LiquidAppConfigDialog.getName();
                    // Replace directory separators (slashes) with underscores
                    // to ensure no sub-directories would get created
                    liquidAppName = liquidAppName.replace(QDir::separator(), "_");

                    if (LiquidAppConfigDialog.isPlanningToRun()) {
                        // Run the newly created Liquid App
                        goto attempt_to_create_or_run_liquid_app;
                    } else {
                        goto done;
                    }
                break;
            }
        }
    }

    ret = app.exec();

done:
    if (sharedMemory != Q_NULLPTR) {
        delete sharedMemory;
        sharedMemory = Q_NULLPTR;
    }

    return ret;
}
