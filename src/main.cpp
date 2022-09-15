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

MainWindow* mainWindow;
LiquidAppWindow* liquidAppWindow;

#if defined(Q_OS_UNIX)
static void onSignalHandler(int signum)
{
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
#endif

int main(int argc, char **argv)
{
    int ret = EXIT_SUCCESS;

#if defined(Q_OS_UNIX)
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

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Account for running on high-DPI displays
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);

    QApplication::setApplicationName(PROG_NAME);
    QApplication::setApplicationDisplayName("Liquid");
    QApplication::setApplicationVersion(VERSION);
    QApplication::setOrganizationDomain("y2z.github.io");
    QApplication::setOrganizationName("Y2Z");

    if (argc < 2) {
        // Show main program window
        mainWindow = new MainWindow;

        // Allow only one instance
        if (mainWindow->isAlreadyRunning()) {
            qDebug().noquote() << QString("Only one instance of Liquid is allowed");
            exit(EXIT_FAILURE);
        }

        mainWindow->run();
    } else  { // App name provided
        // CLI flags and options
        QCommandLineParser parser;

        parser.setApplicationDescription("Convert web resources into desktop applications");
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
            // Found existing liquid app settings file, show it
            liquidAppWindow = new LiquidAppWindow(&liquidAppName);

            // Allow only one instance
            if (liquidAppWindow->isAlreadyRunning()) {
                qDebug().noquote() << QString("Only one instance of Liquid app “%1” is allowed").arg(liquidAppName);
                exit(EXIT_FAILURE);
            }

            liquidAppWindow->run();
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
    return ret;
}
