#include <QApplication>
#include <QSettings>

#include "config.h"

#include "mainwindow.hpp"
#include "liquidappcreateeditdialog.hpp"
#include "liquidappwindow.hpp"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QAction quitAction;

    MainWindow mainWindow;
    LiquidAppWindow liquidAppWindow;

    // Set default Liquid window icon
    QIcon windowIcon(":/images/" PROG_NAME ".svg");
    app.setWindowIcon(windowIcon);

    // Process arguments
    if (argc < 2) {
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
            // Found existing liquid app settings file, show it
            liquidAppWindow.runLiquidApp(&liquidAppName);

            // TODO: check if this Liquid app is already running (allow only one instance)
        } else {
            // No such Liquid app found, open Liquid app creation dialog
            LiquidAppCreateEditDialog liquidAppCreateEditDialog(&mainWindow, liquidAppName);

            // Reveal Liquid app creation dialog
            liquidAppCreateEditDialog.show();

            switch (liquidAppCreateEditDialog.exec())
            {
                case QDialog::Rejected:
                    return 0;
                break;

                case QDialog::Accepted:
                    // Ensure the new Liquid app's settings file is named based on the input field,
                    // and not on what was initially provided via CLI
                    liquidAppName = liquidAppCreateEditDialog.getName();
                    // Replace directory separators (slashes) with underscores
                    // to ensure no sub-directories would get created
                    liquidAppName = liquidAppName.replace(QDir::separator(), "_");

                    goto init;
                break;
            }
        }
    }

    return app.exec();
}
