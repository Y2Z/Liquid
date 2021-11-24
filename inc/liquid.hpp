
#pragma once

#include <QDir>

class Liquid
{
public:
    static QDir getAppsDir(void);
    static QDir getConfigDir(void);
    static void runLiquidApp(const QString liquidAppName);
    static void sleep(const int ms);
};
