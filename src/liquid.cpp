#include "lqd.h"
#include "liquid.hpp"

#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QTime>

QDir Liquid::getAppsDir(void)
{
    return QDir(getConfigDir().absolutePath() + QDir::separator() + LQD_APPS_DIR_NAME + QDir::separator());
}

QDir Liquid::getConfigDir(void)
{
    const QSettings* settings = new QSettings(QSettings::IniFormat,
                                              QSettings::UserScope,
                                              PROG_NAME,
                                              PROG_NAME,
                                              Q_NULLPTR);

    QFileInfo settingsFileInfo(settings->fileName());

    return QDir(settingsFileInfo.absolutePath() + QDir::separator());
}

void Liquid::runLiquidApp(const QString liquidAppName)
{
    const QString liquidAppFilePath(QCoreApplication::applicationFilePath());
    QProcess process;

    process.startDetached(liquidAppFilePath, QStringList() << QStringLiteral("%1").arg(liquidAppName));
}

void Liquid::sleep(const int ms)
{
    const QTime proceedAfter = QTime::currentTime().addMSecs(ms);

    while (QTime::currentTime() < proceedAfter) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, ms / 4);
    }
}
