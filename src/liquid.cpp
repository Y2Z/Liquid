#include "lqd.h"
#include "liquid.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QSettings>
#include <QTime>
#include <QWebEngineProfile>

void Liquid::applyQtStyleSheets(QWidget* window)
{
    QString styleSheet;

    // Load built-in stylesheet
    {
        QFile styleSheetFile(":/styles/" PROG_NAME ".qss");
        styleSheetFile.open(QFile::ReadOnly);
        styleSheet = QLatin1String(styleSheetFile.readAll());
        styleSheetFile.close();
    }

    // Load custom stylesheet
    {
        QFile customStyleSheetFile(Liquid::getConfigDir().absolutePath() + QDir::separator() + PROG_NAME ".qss");
        if (customStyleSheetFile.open(QFile::ReadOnly)) {
            styleSheet += QLatin1String(customStyleSheetFile.readAll());
            customStyleSheetFile.close();
        }
    }

    window->setStyleSheet(styleSheet);
}

void Liquid::createDesktopFile(const QString liquidAppName, const QString liquidAppStartingUrl)
{
#ifdef Q_OS_LINUX
    // Compose content
    QString context = "#!/usr/bin/env xdg-open\n\n";
    context += "[Desktop Entry]\n";
    context += "Type=Application\n";
    context += "Name=" + liquidAppName + "\n";
    context += "Icon=internet-web-browser\n";
    context += "Exec=liquid " + liquidAppName + "\n";
    context += "Comment=" + liquidAppStartingUrl + "\n";
    context += "Categories=Network;WebBrowser;\n";

    // Construct directory path
    // QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString desktopPath = QDir::homePath() + QDir::separator() + "Desktop";

    // Check if the desktop path exists
    QDir dir(desktopPath);
    if (dir.exists()) {
        // Create file
        QFile file(desktopPath + QDir::separator() + liquidAppName + ".desktop");
        file.open(QIODevice::WriteOnly);
        file.write(context.toStdString().c_str());
        file.setPermissions(QFileDevice::ReadUser
                           |QFileDevice::WriteUser
                           |QFileDevice::ExeUser
                           |QFileDevice::ReadGroup
                           |QFileDevice::ReadOther);
        file.flush();
        file.close();
    }
#endif
}

QByteArray Liquid::generateRandomByteArray(const int byteLength)
{
    std::vector<quint32> buf;

    srand(QTime::currentTime().msec());

    for (int i = 0; i < byteLength; ++i) {
        buf.push_back(rand());
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

QString Liquid::getReadableDateTimeString(void)
{
    return QDateTime::currentDateTimeUtc().toString(QLocale().dateTimeFormat());
}

void Liquid::removeDesktopFile(const QString liquidAppName)
{
#ifdef Q_OS_LINUX
    // Construct directory path
    // QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString desktopPath = QDir::homePath() + QDir::separator() + "Desktop";

    // Check if the desktop path exists
    QDir dir(desktopPath);
    if (dir.exists()) {
        // Unlink file
        QFile file(desktopPath + QDir::separator() + liquidAppName + ".desktop");
        file.remove();
    }
#endif
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
