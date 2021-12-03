#include "lqd.h"
#include "liquid.hpp"

#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QTime>
#include <QWebEngineProfile>

QByteArray Liquid::generateRandomByteArray(const int byteLength)
{
    std::vector<quint32> buf;

    qsrand(QTime::currentTime().msec());

    for (int i = 0; i < byteLength; ++i) {
        buf.push_back(qrand());
    }

    return QByteArray(reinterpret_cast<const char*>(buf.data()), byteLength);
}

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

QString Liquid::getDefaultUserAgentString(void)
{
    return QWebEngineProfile().httpUserAgent();
}

QStringList Liquid::getLiquidAppsList(void)
{
    const QFileInfoList liquidAppsFileList = getAppsDir().entryInfoList(QStringList() << "*.ini",
                                                                        QDir::Files | QDir::NoDotAndDotDot,
                                                                        QDir::Name | QDir::IgnoreCase);
    QStringList liquidAppsNames;
    foreach (QFileInfo liquidAppFileInfo, liquidAppsFileList) {
        liquidAppsNames << liquidAppFileInfo.completeBaseName();
    }
    return liquidAppsNames;
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
