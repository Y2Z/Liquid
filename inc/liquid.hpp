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
    static QByteArray generateRandomByteArray(const int byteLength);
    static QDir getAppsDir(void);
    static QDir getConfigDir(void);
    static QString getDefaultUserAgentString(void);
    static QStringList getLiquidAppsList(void);
    static QString getReadableDateTimeString(void);
    static void removeDesktopFile(const QString liquidAppName);
    static void runLiquidApp(const QString liquidAppName);
    static void sleep(const int ms);
};
