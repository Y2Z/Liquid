#pragma once

#include <QDir>
#include <QUrl>
#include <QWidget>

class Liquid
{
public:
    static void applyQtStyleSheets(QWidget* widget);
    static void createDesktopFile(const QString liquidAppName, const QString liquidAppStartingUrl);
    static bool detectDarkMode(void);
    static bool detectDarkModeUncached(void);
    static QByteArray generateRandomByteArray(const int byteLength);
    static QDir getAppsDir(void);
    static QDir getDataDir(void);
    static QString getLiquidAppFilePath(const QString liquidAppName);
    static QString getDefaultUserAgentString(void);
    static QStringList getLiquidAppsList(void);
    static void raiseWindow(QWidget* window);
    static QString getUniqueFilePath(const QString dirPath, const QString baseName, const QString extension);
    static void removeDesktopFile(const QString liquidAppName);
    static void runLiquidApp(const QString liquidAppName);
    static void sleep(const int ms);
};
