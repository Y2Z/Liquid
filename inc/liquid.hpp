#pragma once

#include <functional>

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
    static QString getDesktopFilePath(const QString liquidAppName);
    static QString getDesktopFileIconValue(const QString liquidAppName);
    static QStringList getLiquidAppsList(void);
    static void raiseWindow(QWidget* window, const QString activationToken = QString());
    static void requestActivationToken(QWidget* window, const std::function<void(const QString&)> callback);
    static bool parseRaiseMessage(const QByteArray& message, QString* activationToken);
    static QByteArray makeRaiseMessage(const QString& activationToken);
    static QString getUniqueFilePath(const QString dirPath, const QString baseName, const QString extension);
    static void removeDesktopFile(const QString liquidAppName);
    static void runLiquidApp(const QString liquidAppName, const QString activationToken = QString());
    // For Liquid apps restarting themselves: they get run again once this program is done (and is no longer in the way)
    static void requestRestart(const QString liquidAppName);
    static QString getRequestedRestart(void);
    static void sleep(const int ms);
    static void updateDesktopFileIcon(const QString liquidAppName);
};
