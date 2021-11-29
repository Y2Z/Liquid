#pragma once

#include <QDir>

class Liquid
{
public:
    static QByteArray generateRandomByteArray(const int byteLength);
    static QDir getAppsDir(void);
    static QDir getConfigDir(void);
    static void runLiquidApp(const QString liquidAppName);
    static void sleep(const int ms);
};
