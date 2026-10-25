#include <csignal>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>

#include "lqd.h"
#include "liquid.hpp"
#include "liquidappfile.hpp"
#include "liquidappconfigwindow.hpp"
#include "liquidappwindow.hpp"
#include "mainwindow.hpp"
#include "singleinstance.hpp"

QTextStream cout(stdout);

static SingleInstance* singleInstance = Q_NULLPTR;

LiquidAppWindow* liquidAppWindow;
MainWindow* mainWindow;

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

int main(int argc, char **argv)
{
    int ret = EXIT_SUCCESS;

#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    // Handle termination signals to make sure that
    // Liquid app's settings get saved upon exit
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
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // Make pixmaps assume high-DPI scaling by default (always the case in Qt 6)
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES, true);

    QApplication app(argc, argv);

    if (argc < 2) {
        // Allow only one instance, bring the already running one to front instead of starting another
        singleInstance = new SingleInstance(LQD_INSTANCE_ID_MAIN);
        if (!singleInstance->claim()) {
            qDebug().noquote() << QString("Liquid is already running");
            // Pass on the activation token this process may have been launched with (e.g. by the desktop environment)
            SingleInstance::sendMessage(LQD_INSTANCE_ID_MAIN, Liquid::makeRaiseMessage(qEnvironmentVariable("XDG_ACTIVATION_TOKEN")));
            goto done;
        }

        // Show main program window
        mainWindow = new MainWindow;

        QObject::connect(singleInstance, &SingleInstance::messageReceived, mainWindow, [](const QByteArray& message) {
            static const QByteArray startedPrefix = QByteArray(LQD_INSTANCE_MSG_STARTED) + " ";
            static const QByteArray iconChangedPrefix = QByteArray(LQD_INSTANCE_MSG_ICON_CHANGED) + " ";

            QString activationToken;

            if (Liquid::parseRaiseMessage(message, &activationToken)) {
                Liquid::raiseWindow(mainWindow, activationToken);
            } else if (message.startsWith(iconChangedPrefix)) {
                // A Liquid app has got its icon from its website
                mainWindow->updateLiquidAppIcon(QString::fromUtf8(message.mid(iconChangedPrefix.size())));
            } else if (message.startsWith(startedPrefix)) {
                // A Liquid app has started, show it as running
                mainWindow->watchLiquidApp(QString::fromUtf8(message.mid(startedPrefix.size())));
            }
        });
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
        QSettings* tempAppSettings = LiquidAppFile::open(liquidAppName);

        // Attempt to load app settings from a config file
        if (!parser.isSet(editAppDialogFlag) && tempAppSettings->contains(LQD_CFG_KEY_NAME_URL)) {
            // Allow only one instance per Liquid app, bring the already running one to front instead of starting another
            const QString instanceId = LQD_INSTANCE_ID_APP_PREFIX + liquidAppName;
            singleInstance = new SingleInstance(instanceId);
            if (!singleInstance->claim()) {
                qDebug().noquote() << QString("Liquid app “%1” is already running").arg(liquidAppName);
                SingleInstance::sendMessage(instanceId, Liquid::makeRaiseMessage(qEnvironmentVariable("XDG_ACTIVATION_TOKEN")));
                goto done;
            }

            // Found existing liquid app settings file, show it
            liquidAppWindow = new LiquidAppWindow(&liquidAppName);

            QObject::connect(singleInstance, &SingleInstance::messageReceived, liquidAppWindow, [](const QByteArray& message) {
                QString activationToken;

                if (Liquid::parseRaiseMessage(message, &activationToken)) {
                    Liquid::raiseWindow(liquidAppWindow, activationToken);
                } else if (message == LQD_INSTANCE_MSG_QUIT) {
                    liquidAppWindow->close();
                } else if (message == LQD_INSTANCE_MSG_RELOAD_SETTINGS) {
                    liquidAppWindow->reloadLiquidAppConfig();
                } else if (message.startsWith(LQD_INSTANCE_MSG_PREVIEW_CSS " ")) {
                    // Temporary, while the settings are being edited (reloading settings brings back the saved one)
                    const QByteArray css = QByteArray::fromBase64(message.mid(qstrlen(LQD_INSTANCE_MSG_PREVIEW_CSS " ")));
                    liquidAppWindow->setAdditionalCss(QString::fromUtf8(css));
                } else if (message.startsWith(LQD_INSTANCE_MSG_PREVIEW_BG " ")) {
                    // Temporary, while the settings are being edited (reloading settings brings back the saved one)
                    const QString color = QString::fromUtf8(message.mid(qstrlen(LQD_INSTANCE_MSG_PREVIEW_BG " "))).trimmed();
                    liquidAppWindow->setBackground((color == "none") ? QColor() : QColor(color));
                } else if (message.startsWith(LQD_INSTANCE_MSG_PREVIEW_FRAME " ")) {
                    // Temporary, while the settings are being edited (reloading settings brings back the saved one)
                    liquidAppWindow->setFrameless(message.mid(qstrlen(LQD_INSTANCE_MSG_PREVIEW_FRAME " ")).trimmed() == "off");
                } else if (message.startsWith(LQD_INSTANCE_MSG_PREVIEW_SCROLL " ")) {
                    // Temporary, while the settings are being edited (reloading settings brings back the saved one)
                    liquidAppWindow->setScrollBarsHidden(message.mid(qstrlen(LQD_INSTANCE_MSG_PREVIEW_SCROLL " ")).trimmed() == "off");
                }
            });

            // Let the main window (if it's open) know that this Liquid app is now running
            SingleInstance::sendMessage(LQD_INSTANCE_ID_MAIN, QByteArray(LQD_INSTANCE_MSG_STARTED) + " " + liquidAppName.toUtf8());
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
    if (singleInstance != Q_NULLPTR) {
        // Stops listening (and removes the socket file, where applicable)
        delete singleInstance;
        singleInstance = Q_NULLPTR;
    }

    return ret;
}
