#include "lqd.h"
#include "liquid.hpp"
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include "liquidx11.h"
#endif

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTime>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWindow>
#include <QGuiApplication>

#if defined(LQD_WAYLAND_ACTIVATION) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <memory>
#include <QGuiApplication>
#include <QtGui/qpa/qplatformwindow_p.h>
// Qt 6.5 makes the interface available on all Unix-like systems, later versions only when built with Wayland support
#if !defined(QT_FEATURE_wayland) || QT_FEATURE_wayland == 1
#define LQD_CAN_REQUEST_ACTIVATION_TOKEN
#endif
#endif

#if defined(Q_OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#endif

void Liquid::applyQtStyleSheets(QWidget* widget)
{
    QString styleSheet;

    // Load built-in base stylesheet
    {
        QFile styleSheetFile(":/styles/base.qss");
        styleSheetFile.open(QFile::ReadOnly);
        styleSheet = QLatin1String(styleSheetFile.readAll());
        styleSheetFile.close();
    }
    // Load built-in color theme stylesheet
    {
        QFile colorThemeStyleSheetFile(QString(":/styles/%1.qss").arg((Liquid::detectDarkMode()) ? "dark" : "light"));
        colorThemeStyleSheetFile.open(QFile::ReadOnly);
        styleSheet += QLatin1String(colorThemeStyleSheetFile.readAll());
        colorThemeStyleSheetFile.close();
    }

    // Load user-defined stylesheet
    {
        QFile customStyleSheetFile(Liquid::getDataDir().filePath(PROG_NAME ".qss"));
        if (customStyleSheetFile.open(QFile::ReadOnly)) {
            styleSheet += QLatin1String(customStyleSheetFile.readAll());
            customStyleSheetFile.close();
        }
    }

    widget->setStyleSheet(styleSheet);
}

void Liquid::createDesktopFile(const QString liquidAppName, const QString liquidAppStartingUrl)
{
#if defined(Q_OS_LINUX)
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
#else
    Q_UNUSED(liquidAppName);
    Q_UNUSED(liquidAppStartingUrl);
#endif
}

bool Liquid::detectDarkMode(void)
{
    // Detecting it may involve spawning a process, do it only once per program run
    static int cachedResult = -1;

    if (cachedResult < 0) {
        cachedResult = detectDarkModeUncached() ? 1 : 0;
    }

    return cachedResult == 1;
}

bool Liquid::detectDarkModeUncached(void)
{
#if defined(Q_OS_LINUX)
    QProcess process;
    process.start("gsettings", QStringList() << "get" << "org.gnome.desktop.interface" << "gtk-theme");
    process.waitForFinished(1000);
    QString output = process.readAllStandardOutput();
    if (output.size() > 0) {
        return output.endsWith("-dark'\n", Qt::CaseInsensitive);
    }
#elif defined(Q_OS_MACOS)
    static const QString macOsVer = QSysInfo::productVersion();
    static const QStringList macOsVerParts = macOsVer.split('.');
    if (macOsVerParts.at(0).toInt() > 10 || (macOsVerParts.at(0).toInt() >= 10 && macOsVerParts.at(1).toInt() >= 14)) {
        bool macOsUserInterfaceIsUsingDarkMode = false;
        CFStringRef darkStr = CFSTR("Dark");
        CFStringRef macOsUserInterfaceStyleStr = CFSTR("AppleInterfaceStyle");
        CFStringRef macOsUserInterfaceStyle = (CFStringRef)CFPreferencesCopyAppValue(macOsUserInterfaceStyleStr, kCFPreferencesCurrentApplication);
        if (macOsUserInterfaceStyle != Q_NULLPTR) {
            macOsUserInterfaceIsUsingDarkMode = (CFStringCompare(macOsUserInterfaceStyle, darkStr, 0) == kCFCompareEqualTo);
            CFRelease(macOsUserInterfaceStyle);
            return macOsUserInterfaceIsUsingDarkMode;
        }
    }
#elif defined(Q_OS_WIN)
    return (QSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat)
            .value("AppsUseLightTheme", 1) == 0);
#endif

    return QApplication::palette().text().color().lightnessF() > QApplication::palette().window().color().lightnessF();
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
    const QString path = getDataDir().filePath(LQD_APPS_DIR_NAME);

    QDir().mkpath(path);

    return QDir(path);
}

QDir Liquid::getDataDir(void)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    // ~/Library/Application Support/Liquid/ on macOS, %LOCALAPPDATA%\Liquid\ on Windows
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + LQD_PROG_TITLE;
#else
    // ~/.liquid/ on GNU/Linux, BSD, etc (it's home to Liquid apps, not just to Liquid's own configuration)
    const QString path = QDir::homePath() + QDir::separator() + "." PROG_NAME;
#endif

    QDir().mkpath(path);

    return QDir(path);
}

QString Liquid::getLiquidAppFilePath(const QString liquidAppName)
{
    return getAppsDir().filePath(liquidAppName + "." LQD_APP_FILE_EXTENSION);
}

QString Liquid::getDefaultUserAgentString(void)
{
    return QWebEngineProfile().httpUserAgent();
}

QStringList Liquid::getLiquidAppsList(void)
{
    const QFileInfoList liquidAppsFileList = getAppsDir().entryInfoList(QStringList() << "*." LQD_APP_FILE_EXTENSION,
                                                                        QDir::Files | QDir::NoDotAndDotDot,
                                                                        QDir::Name | QDir::IgnoreCase);
    QStringList liquidAppsNames;
    foreach (QFileInfo liquidAppFileInfo, liquidAppsFileList) {
        liquidAppsNames << liquidAppFileInfo.completeBaseName();
    }
    return liquidAppsNames;
}

QString Liquid::getUniqueFilePath(const QString dirPath, const QString baseName, const QString extension)
{
    // Produces "<baseName> (N).<extension>", where N is the lowest number not yet taken
    // (deliberately avoids timestamps, both for privacy and for filesystem compatibility)
    const QDir dir(dirPath);
    QString filePath;

    for (int n = 1; ; n++) {
        filePath = dir.filePath(QString("%1 (%2).%3").arg(baseName).arg(n).arg(extension));

        if (!QFileInfo::exists(filePath)) {
            break;
        }
    }

    return filePath;
}

void Liquid::raiseWindow(QWidget* window, const QString activationToken)
{
    if (window == Q_NULLPTR) {
        return;
    }

    // On Wayland, windows may only be brought to front using a token obtained by whoever the user is interacting with
    // (otherwise the compositor shows an "is ready" notification instead); Qt picks it up from this environment variable
    if (!activationToken.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
    }

    // Un-minimize (without affecting full-screen or maximized state), then bring to front
    window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
    window->show();
    window->raise();
    window->activateWindow();

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // X11 window managers (e.g. GNOME's) ignore activation requests Qt makes on behalf of windows that weren't interacted
    // with recently (showing an "is ready" notification instead), so ask them the same way taskbars do
    // (Qt 5 always ends up using X11 on GNOME, even in Wayland sessions)
    if (QGuiApplication::platformName() == "xcb") {
        liquidX11ActivateWindow(window->effectiveWinId());
    }
#endif
}

void Liquid::removeDesktopFile(const QString liquidAppName)
{
#if defined(Q_OS_LINUX)
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
#else
    Q_UNUSED(liquidAppName);
#endif
}

void Liquid::runLiquidApp(const QString liquidAppName, const QString activationToken)
{
    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments(QStringList() << liquidAppName);

    // Lets the new Liquid app's window show up in front on Wayland (see raiseWindow())
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove("XDG_ACTIVATION_TOKEN");
    if (!activationToken.isEmpty()) {
        environment.insert("XDG_ACTIVATION_TOKEN", activationToken);
    }
    process.setProcessEnvironment(environment);

    process.startDetached();
}

void Liquid::requestActivationToken(QWidget* window, const std::function<void(const QString&)> callback)
{
#if defined(LQD_CAN_REQUEST_ACTIVATION_TOKEN)
    // Ask the compositor for a token on behalf of the window the user is interacting with (requires private Qt API)
    using QNativeInterface::Private::QWaylandWindow;

    QWindow* windowHandle = (window != Q_NULLPTR) ? window->window()->windowHandle() : Q_NULLPTR;
    QWaylandWindow* waylandWindow = (windowHandle != Q_NULLPTR) ? windowHandle->nativeInterface<QWaylandWindow>() : Q_NULLPTR;
    auto* waylandApplication = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();

    if (waylandWindow != Q_NULLPTR && waylandApplication != Q_NULLPTR) {
        // The callback gets called exactly once: either with the token, or without one if the compositor doesn't respond
        const auto isDone = std::make_shared<bool>(false);
        const auto connection = std::make_shared<QMetaObject::Connection>();

        *connection = QObject::connect(waylandWindow, &QWaylandWindow::xdgActivationTokenCreated, window,
                                       [isDone, connection, callback](const QString& token) {
            QObject::disconnect(*connection);

            if (!*isDone) {
                *isDone = true;
                callback(token);
            }
        });

        QTimer::singleShot(LQD_ACTIVATION_TOKEN_TIMEOUT, window, [isDone, connection, callback]() {
            QObject::disconnect(*connection);

            if (!*isDone) {
                *isDone = true;
                callback(QString());
            }
        });

        waylandWindow->requestXdgActivationToken(waylandApplication->lastInputSerial());
        return;
    }
#else
    Q_UNUSED(window);
#endif

    callback(QString());
}

bool Liquid::parseRaiseMessage(const QByteArray& message, QString* activationToken)
{
    // Either just "raise", or "raise <activation token>"
    static const QByteArray prefix = QByteArray(LQD_INSTANCE_MSG_RAISE) + " ";

    if (message == LQD_INSTANCE_MSG_RAISE) {
        activationToken->clear();
        return true;
    }

    if (message.startsWith(prefix)) {
        *activationToken = QString::fromUtf8(message.mid(prefix.size())).trimmed();
        return true;
    }

    return false;
}

QByteArray Liquid::makeRaiseMessage(const QString& activationToken)
{
    QByteArray message = LQD_INSTANCE_MSG_RAISE;

    if (!activationToken.isEmpty()) {
        message += " " + activationToken.toUtf8();
    }

    return message;
}

void Liquid::sleep(const int ms)
{
    // Keep processing events while waiting, but without spinning the CPU:
    // block until an event arrives, with the timer guaranteeing one when it's time to stop waiting
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(ms);

    QElapsedTimer elapsed;
    elapsed.start();

    while (elapsed.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents);
    }
}
