#pragma once

#include <QString>
#include <QStringList>

class QSettings;

// Launchers make Liquid apps show up as apps of their own (rather than as windows of Liquid): each one gets its own entry
// in the applications menu (where it can be searched for, and pinned to the dock/taskbar), optionally an icon on the desktop,
// and its windows get grouped under it, with its own icon.
//
// Launchers are just files (.desktop ones on GNU/Linux, BSD, etc), whether a Liquid app has one is up to whether its file exists:
// they may get edited or removed by the user, and only what Liquid needs to be in them gets kept up to date.
// Liquid apps can also be dragged out of Liquid, which makes launchers wherever they get dropped: those get known to Liquid
// once they get used (they tell the Liquid app where they are), to be kept up to date too.
class LiquidAppLauncher
{
public:
    enum Kind {
        ApplicationsMenu,
        Desktop,
    };

    // Whether launchers can be made on this system
    static bool isSupported(void);
    static bool isSupported(const Kind kind);

    static bool exists(const Kind kind, const QString& liquidAppName);
    static bool create(const Kind kind, const QString& liquidAppName, QSettings* liquidAppConfig);
    static void remove(const Kind kind, const QString& liquidAppName);
    // Including those made by dragging the Liquid app out of Liquid (that are known to it)
    static void removeAll(const QString& liquidAppName);

    // Keeps existing launchers in line with the Liquid app (its title, icon, how its windows get grouped, where Liquid itself is)
    static void update(const QString& liquidAppName, QSettings* liquidAppConfig = Q_NULLPTR);

    // A launcher to be dragged out of Liquid (it gets copied to wherever it gets dropped), an empty string if it can't be made
    static QString createForDragging(const QString& liquidAppName, QSettings* liquidAppConfig);

    // The Liquid app has been run by the given launcher (as told by the launcher itself), keep it up to date from now on
    static void adopt(const QString& liquidAppName, QSettings* liquidAppConfig, const QString& launcher);

    // Makes this program be seen by the system as the Liquid app itself, has to be done before any window gets created
    static void identifyProcess(const QString& liquidAppName);

    // What the system knows the Liquid app as (e.g. "liquid-YouTube"), it's also the name of its launcher in the applications menu
    static QString getId(const QString& liquidAppName);

private:
    static QString getFilePath(const Kind kind, const QString& liquidAppName);
    static QString getName(const QString& liquidAppName, QSettings* liquidAppConfig);
    static QString getContent(const QString& liquidAppName, QSettings* liquidAppConfig, const bool isRunnableFile);
    static bool writeFile(const QString& path, const QString& content, const bool isRunnableFile);
    static bool isOwnedBy(const QString& path, const QString& liquidAppName);
    static QStringList getAdoptedFilePaths(QSettings* liquidAppConfig);
    static QString getIconValue(const QString& liquidAppName);
    static QString getExecValue(const QString& liquidAppName);
    static bool isExecValueRunnable(const QString& execValue);
};
