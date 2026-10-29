#include "lqd.h"
#include "liquid.hpp"
#include "liquidappicon.hpp"
#include "liquidappfile.hpp"
#include "liquidapplauncher.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QList>
#include <QPair>
#include <QProcess>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QSettings>
#include <QUrl>
#include <QStandardPaths>
#include <QStringList>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#define LQD_DESKTOP_ENTRIES
#endif

#if defined(LQD_DESKTOP_ENTRIES)
// Value of type string (desktop entry specification, "Possible value types")
static QString escapeDesktopEntryString(const QString& value)
{
    QString escaped;

    for (const QChar c : value) {
        if (c == '\\') {
            escaped += "\\\\";
        } else if (c == '\n') {
            escaped += "\\n";
        } else if (c == '\t') {
            escaped += "\\t";
        } else if (c == '\r') {
            escaped += "\\r";
        } else {
            escaped += c;
        }
    }

    if (escaped.startsWith(' ')) {
        escaped.replace(0, 1, "\\s");
    }

    return escaped;
}

// Argument of Exec (desktop entry specification, "The Exec key"), before the string escaping applies to the whole value
static QString quoteExecArgument(const QString& argument)
{
    static const QString reservedCharacters = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");

    bool isQuotingNeeded = argument.isEmpty();
    for (const QChar c : argument) {
        if (reservedCharacters.contains(c)) {
            isQuotingNeeded = true;
            break;
        }
    }

    QString quoted;
    for (const QChar c : argument) {
        if (isQuotingNeeded && (c == '"' || c == '`' || c == '$' || c == '\\')) {
            quoted += '\\';
        }
        quoted += c;
    }
    // Field codes (e.g. %u) start with a percent sign, a literal one is doubled
    quoted.replace('%', "%%");

    return (isQuotingNeeded) ? '"' + quoted + '"' : quoted;
}

// First argument of an Exec value (the program), as written in the file
static QString getExecProgram(const QString& execValue)
{
    // Undo string escaping
    QString value;
    for (int i = 0; i < execValue.size(); i++) {
        if (execValue.at(i) == '\\' && i + 1 < execValue.size()) {
            const QChar next = execValue.at(++i);
            value += (next == 's') ? QChar(' ') : (next == 'n') ? QChar('\n') : (next == 't') ? QChar('\t') : (next == 'r') ? QChar('\r') : next;
        } else {
            value += execValue.at(i);
        }
    }
    value = value.trimmed();

    // Undo quoting
    QString program;
    if (value.startsWith('"')) {
        for (int i = 1; i < value.size() && value.at(i) != '"'; i++) {
            if (value.at(i) == '\\' && i + 1 < value.size()) {
                i++;
            }
            program += value.at(i);
        }
    } else {
        program = value.section(QRegularExpression("\\s"), 0, 0);
    }

    return program.replace("%%", "%");
}

// Sets keys of the [Desktop Entry] group, leaving everything else in the file as it is
static bool setDesktopEntryKeys(QStringList& lines, const QList<QPair<QString, QString>>& keys)
{
    bool isChanged = false;
    int groupStart = -1;
    int groupEnd = lines.size();

    for (int i = 0; i < lines.size(); i++) {
        const QString line = lines.at(i).trimmed();

        if (line.startsWith('[')) {
            if (groupStart >= 0) {
                groupEnd = i;
                break;
            }
            if (line == "[Desktop Entry]") {
                groupStart = i;
            }
        }
    }

    if (groupStart < 0) {
        return false;
    }

    // Don't leave blank lines between the group's keys and what gets added to it
    while (groupEnd - 1 > groupStart && lines.at(groupEnd - 1).trimmed().isEmpty()) {
        groupEnd--;
    }

    for (const QPair<QString, QString>& key : keys) {
        const QString newLine = key.first + '=' + key.second;
        bool isFound = false;

        for (int i = groupStart + 1; i < groupEnd; i++) {
            if (lines.at(i).section('=', 0, 0).trimmed() == key.first) {
                isFound = true;
                if (lines.at(i) != newLine) {
                    lines[i] = newLine;
                    isChanged = true;
                }
                break;
            }
        }

        if (!isFound) {
            lines.insert(groupEnd++, newLine);
            isChanged = true;
        }
    }

    return isChanged;
}
#endif

bool LiquidAppLauncher::isSupported(void)
{
    return isSupported(ApplicationsMenu) || isSupported(Desktop);
}

bool LiquidAppLauncher::isSupported(const Kind kind)
{
#if defined(LQD_DESKTOP_ENTRIES)
    return !getFilePath(kind, "_").isEmpty();
#else
    Q_UNUSED(kind);

    return false;
#endif
}

QString LiquidAppLauncher::getId(const QString& liquidAppName)
{
    // Only characters that can be used anywhere the ID ends up (file names, window classes, Wayland app IDs)
    QString id = liquidAppName;
    id.replace(QRegularExpression("[^A-Za-z0-9_-]"), "_");

    // Names that had to be altered get told apart by a hash of the original
    if (id != liquidAppName || id.isEmpty()) {
        id += '-' + QString::fromLatin1(QCryptographicHash::hash(liquidAppName.toUtf8(), QCryptographicHash::Sha1).toHex().left(8));
    }

    return PROG_NAME "-" + id;
}

QString LiquidAppLauncher::getFilePath(const Kind kind, const QString& liquidAppName)
{
#if defined(LQD_DESKTOP_ENTRIES)
    if (kind == ApplicationsMenu) {
        // ~/.local/share/applications/
        const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);

        return (dirPath.isEmpty()) ? QString() : QDir(dirPath).filePath(getId(liquidAppName) + ".desktop");
    } else {
        // Wherever the user's desktop is (as long as there's one: with no desktop folder set up, it's their home folder)
        QString dirPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (dirPath.isEmpty() || QDir(dirPath) == QDir::home() || !QDir(dirPath).exists()) {
            dirPath = QDir::home().filePath("Desktop");
            if (!QDir(dirPath).exists()) {
                return QString();
            }
        }

        // The desktop shows file names (in some places), it's the Liquid app's name, as it's always been
        QString fileName = liquidAppName;
        fileName.replace('/', '_');

        return QDir(dirPath).filePath(fileName + ".desktop");
    }
#else
    Q_UNUSED(kind);
    Q_UNUSED(liquidAppName);

    return QString();
#endif
}

bool LiquidAppLauncher::exists(const Kind kind, const QString& liquidAppName)
{
    const QString path = getFilePath(kind, liquidAppName);

    return !path.isEmpty() && QFileInfo(path).isFile();
}

QString LiquidAppLauncher::getIconValue(const QString& liquidAppName)
{
    const QString iconFilePath = LiquidAppIcon::getIconFilePath(liquidAppName);

    return (iconFilePath.isEmpty()) ? "internet-web-browser" : iconFilePath;
}

QString LiquidAppLauncher::getExecValue(const QString& liquidAppName)
{
#if defined(LQD_DESKTOP_ENTRIES)
    // Liquid itself: by its name if that's what it can be found by, otherwise where it is
    QString program = QCoreApplication::applicationFilePath();
    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        // Running from an AppImage (it gets mounted somewhere different every time)
        program = qEnvironmentVariable("APPIMAGE");
    } else if (QFileInfo(QStandardPaths::findExecutable(PROG_NAME)).canonicalFilePath() == QFileInfo(program).canonicalFilePath()) {
        program = PROG_NAME;
    }

    // %k is where the launcher is (for launchers made by dragging the Liquid app out of Liquid to become known to it)
    return escapeDesktopEntryString(quoteExecArgument(program) + ' ' + quoteExecArgument(liquidAppName) + " %k");
#else
    Q_UNUSED(liquidAppName);

    return QString();
#endif
}

bool LiquidAppLauncher::isExecValueRunnable(const QString& execValue)
{
#if defined(LQD_DESKTOP_ENTRIES)
    const QString program = getExecProgram(execValue);

    if (program.isEmpty()) {
        return false;
    }

    if (program.contains('/')) {
        const QFileInfo programFileInfo(program);

        return programFileInfo.isFile() && programFileInfo.isExecutable();
    }

    return !QStandardPaths::findExecutable(program).isEmpty();
#else
    Q_UNUSED(execValue);

    return false;
#endif
}

QString LiquidAppLauncher::getName(const QString& liquidAppName, QSettings* liquidAppConfig)
{
    // The title, if the Liquid app has one
    const QString title = (liquidAppConfig != Q_NULLPTR) ? liquidAppConfig->value(LQD_CFG_KEY_NAME_TITLE).toString().trimmed() : QString();

    return (title.isEmpty()) ? liquidAppName : title;
}

QString LiquidAppLauncher::getContent(const QString& liquidAppName, QSettings* liquidAppConfig, const bool isRunnableFile)
{
#if defined(LQD_DESKTOP_ENTRIES)
    QString content;
    // Desktop icons (and launchers in other folders) are run like programs
    if (isRunnableFile) {
        content += "#!/usr/bin/env xdg-open\n\n";
    }
    content += "[Desktop Entry]\n";
    content += "Type=Application\n";
    content += "Name=" + escapeDesktopEntryString(getName(liquidAppName, liquidAppConfig)) + "\n";
    content += "Comment=" + escapeDesktopEntryString(liquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString()) + "\n";
    content += "Icon=" + escapeDesktopEntryString(getIconValue(liquidAppName)) + "\n";
    content += "Exec=" + getExecValue(liquidAppName) + "\n";
    content += "Terminal=false\n";
    content += "Categories=Network;WebBrowser;\n";
    // Which windows are this Liquid app's (see identifyProcess())
    content += "StartupWMClass=" + getId(liquidAppName) + "\n";
    content += "StartupNotify=true\n";
    // Which Liquid app it's for (it's what tells launchers made by Liquid apart from any others)
    content += "X-Liquid-App=" + escapeDesktopEntryString(liquidAppName) + "\n";

    return content;
#else
    Q_UNUSED(liquidAppName);
    Q_UNUSED(liquidAppConfig);
    Q_UNUSED(isRunnableFile);

    return QString();
#endif
}

bool LiquidAppLauncher::writeFile(const QString& path, const QString& content, const bool isRunnableFile)
{
    QDir().mkpath(QFileInfo(path).path());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(content.toUtf8());
    file.close();

    if (isRunnableFile) {
        // Has to be executable to be run, and on GNOME also marked as trusted (that's what "Allow Launching" does)
        file.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser
                            | QFileDevice::ReadGroup | QFileDevice::ReadOther);
        QProcess::startDetached("gio", QStringList() << "set" << path << "metadata::trusted" << "true");
    }

    return true;
}

bool LiquidAppLauncher::create(const Kind kind, const QString& liquidAppName, QSettings* liquidAppConfig)
{
#if defined(LQD_DESKTOP_ENTRIES)
    const QString path = getFilePath(kind, liquidAppName);
    if (path.isEmpty()) {
        return false;
    }

    return writeFile(path, getContent(liquidAppName, liquidAppConfig, kind == Desktop), kind == Desktop);
#else
    Q_UNUSED(kind);
    Q_UNUSED(liquidAppName);
    Q_UNUSED(liquidAppConfig);

    return false;
#endif
}

QString LiquidAppLauncher::createForDragging(const QString& liquidAppName, QSettings* liquidAppConfig)
{
#if defined(LQD_DESKTOP_ENTRIES)
    // Named after the Liquid app's title (that's what file managers show, for other launchers to be told apart from it)
    QString fileName = getName(liquidAppName, liquidAppConfig);
    fileName.replace('/', '_');
    if (fileName.startsWith('.')) {
        fileName.replace(0, 1, '_');
    }

    // One folder per Liquid app, for launchers of Liquid apps with the same title not to get in each other's way
    const QDir dir(Liquid::getDataDir().filePath(QString(LQD_LAUNCHERS_DIR_NAME) + QDir::separator() + getId(liquidAppName)));
    QDir(dir).removeRecursively();
    const QString path = dir.filePath(fileName + ".desktop");

    return (writeFile(path, getContent(liquidAppName, liquidAppConfig, true), true)) ? path : QString();
#else
    Q_UNUSED(liquidAppName);
    Q_UNUSED(liquidAppConfig);

    return QString();
#endif
}

bool LiquidAppLauncher::isOwnedBy(const QString& path, const QString& liquidAppName)
{
#if defined(LQD_DESKTOP_ENTRIES)
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Only a launcher made by Liquid for this Liquid app (not one that's been replaced with something else since)
    const QStringList lines = QString::fromUtf8(file.read(64 * 1024)).split('\n');

    return lines.contains("X-Liquid-App=" + escapeDesktopEntryString(liquidAppName))
           || lines.contains("StartupWMClass=" + getId(liquidAppName));
#else
    Q_UNUSED(path);
    Q_UNUSED(liquidAppName);

    return false;
#endif
}

QStringList LiquidAppLauncher::getAdoptedFilePaths(QSettings* liquidAppConfig)
{
    return liquidAppConfig->value(LQD_CFG_KEY_NAME_LAUNCHERS).toString().split('\n', Qt::SkipEmptyParts);
}

void LiquidAppLauncher::adopt(const QString& liquidAppName, QSettings* liquidAppConfig, const QString& launcher)
{
#if defined(LQD_DESKTOP_ENTRIES)
    // Launchers are told where they are either as paths or as URLs
    const QUrl url(launcher);
    QString path = (url.isLocalFile()) ? url.toLocalFile() : launcher;
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()) {
        return;
    }
    path = QFileInfo(path).absoluteFilePath();

    // Those in the applications menu and on the desktop are already known, and those in Liquid's own folder are yet to be dropped
    if (path == getFilePath(ApplicationsMenu, liquidAppName) || path == getFilePath(Desktop, liquidAppName)
        || path.startsWith(Liquid::getDataDir().filePath(LQD_LAUNCHERS_DIR_NAME) + QDir::separator())
        || !isOwnedBy(path, liquidAppName)) {
        return;
    }

    QStringList paths = getAdoptedFilePaths(liquidAppConfig);
    if (!paths.contains(path)) {
        paths.append(path);
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_LAUNCHERS, paths.join('\n'));
        liquidAppConfig->sync();

        // It may have been dropped a while ago (and the Liquid app's title or icon changed since)
        update(liquidAppName, liquidAppConfig);
    }
#else
    Q_UNUSED(liquidAppName);
    Q_UNUSED(liquidAppConfig);
    Q_UNUSED(launcher);
#endif
}

void LiquidAppLauncher::remove(const Kind kind, const QString& liquidAppName)
{
    const QString path = getFilePath(kind, liquidAppName);

    if (!path.isEmpty()) {
        QFile::remove(path);
    }
}

void LiquidAppLauncher::removeAll(const QString& liquidAppName)
{
    remove(ApplicationsMenu, liquidAppName);
    remove(Desktop, liquidAppName);

#if defined(LQD_DESKTOP_ENTRIES)
    // They'd only be left pointing at nothing
    QScopedPointer<QSettings> liquidAppConfig(LiquidAppFile::open(liquidAppName));
    for (const QString& path : getAdoptedFilePaths(liquidAppConfig.data())) {
        if (isOwnedBy(path, liquidAppName)) {
            QFile::remove(path);
        }
    }

    QDir(Liquid::getDataDir().filePath(QString(LQD_LAUNCHERS_DIR_NAME) + QDir::separator() + getId(liquidAppName))).removeRecursively();
#endif
}

void LiquidAppLauncher::update(const QString& liquidAppName, QSettings* liquidAppConfig)
{
#if defined(LQD_DESKTOP_ENTRIES)
    QScopedPointer<QSettings> ownLiquidAppConfig;
    if (liquidAppConfig == Q_NULLPTR) {
        ownLiquidAppConfig.reset(LiquidAppFile::open(liquidAppName));
        liquidAppConfig = ownLiquidAppConfig.data();
    }

    QStringList paths;
    for (const Kind kind : { ApplicationsMenu, Desktop }) {
        if (exists(kind, liquidAppName)) {
            paths << getFilePath(kind, liquidAppName);
        }
    }

    // Those made by dragging the Liquid app out of Liquid, forgetting the ones that are gone (or are no longer this Liquid app's)
    const QStringList adoptedPaths = getAdoptedFilePaths(liquidAppConfig);
    QStringList remainingAdoptedPaths;
    for (const QString& path : adoptedPaths) {
        if (!paths.contains(path) && QFileInfo(path).isFile() && isOwnedBy(path, liquidAppName)) {
            remainingAdoptedPaths << path;
        }
    }
    if (remainingAdoptedPaths != adoptedPaths) {
        if (remainingAdoptedPaths.isEmpty()) {
            liquidAppConfig->remove(LQD_CFG_KEY_NAME_LAUNCHERS);
        } else {
            liquidAppConfig->setValue(LQD_CFG_KEY_NAME_LAUNCHERS, remainingAdoptedPaths.join('\n'));
        }
        liquidAppConfig->sync();
    }
    paths << remainingAdoptedPaths;

    for (const QString& path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
        file.close();

        QList<QPair<QString, QString>> keys;
        keys << qMakePair(QString("Name"), escapeDesktopEntryString(getName(liquidAppName, liquidAppConfig)));
        keys << qMakePair(QString("Icon"), escapeDesktopEntryString(getIconValue(liquidAppName)));
        keys << qMakePair(QString("StartupWMClass"), getId(liquidAppName));
        keys << qMakePair(QString("StartupNotify"), QString("true"));
        keys << qMakePair(QString("X-Liquid-App"), escapeDesktopEntryString(liquidAppName));

        // Only if Liquid can't be run the way it's written there (e.g. it's been moved), or it's how older versions
        // of Liquid wrote it, otherwise it may have been edited by the user
        const QString execValue = getExecValue(liquidAppName);
        for (const QString& line : lines) {
            if (line.startsWith("Exec=")) {
                const QString currentExecValue = line.mid(qstrlen("Exec="));
                if (!isExecValueRunnable(currentExecValue)
                    || currentExecValue == PROG_NAME " " + liquidAppName // Didn't work for names with spaces in them
                    || currentExecValue + " %k" == execValue // Didn't tell the Liquid app where its launcher is
                ) {
                    keys << qMakePair(QString("Exec"), execValue);
                }
                break;
            }
        }

        // Writing in place keeps the file's permissions (desktop icons have to remain executable)
        if (setDesktopEntryKeys(lines, keys) && file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(lines.join('\n').toUtf8());
            file.close();
        }
    }
#else
    Q_UNUSED(liquidAppName);
    Q_UNUSED(liquidAppConfig);
#endif
}

void LiquidAppLauncher::identifyProcess(const QString& liquidAppName)
{
#if defined(LQD_DESKTOP_ENTRIES)
    const QString id = getId(liquidAppName);

    // Window class on X11 (matched against StartupWMClass), it gets set once the first window is created
    QCoreApplication::setApplicationName(id);
    // App ID on Wayland (matched against the name of the launcher in the applications menu)
    QGuiApplication::setDesktopFileName(id);
#else
    Q_UNUSED(liquidAppName);
#endif
}
