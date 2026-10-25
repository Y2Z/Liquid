#pragma once

#include <QByteArray>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QUrl>

#include "lqd.h"

class QNetworkAccessManager;
class QNetworkReply;
class QWebEnginePage;

// A Liquid app's icon: either an SVG image (kept as-is, so it stays sharp at any size), or a raster one (kept as PNG);
// stored inside .liquid files, with a copy next to them for .desktop files to point at
class LiquidAppIcon
{
public:
    LiquidAppIcon(void) {}

    // Accepts SVG and any raster format Qt can read (picking the largest image for files containing several, e.g. .ico);
    // SVG images that could make whatever displays them reach out for something (e.g. external images) get rasterized
    static LiquidAppIcon fromData(const QByteArray& data);
    static LiquidAppIcon fromImage(const QImage& image);
    static LiquidAppIcon fromIcon(const QIcon& icon); // Takes the largest image the icon contains

    // The Liquid app's own icon (null if it doesn't have one yet)
    static LiquidAppIcon load(QSettings* liquidAppConfig);

    // The Liquid app's own icon, or Liquid's icon if it doesn't have one yet
    static QIcon loadIcon(QSettings* liquidAppConfig);

    // Makes this the Liquid app's icon
    void save(QSettings* liquidAppConfig, const QString& liquidAppName) const;

    // Takes the icon away from the Liquid app (it'll get fetched from the website again)
    static void remove(QSettings* liquidAppConfig, const QString& liquidAppName);

    // Puts the icon file in place (for .desktop files) if it's missing, e.g. for icons from older versions of Liquid
    static void ensureIconFile(QSettings* liquidAppConfig, const QString& liquidAppName);

    // Where the copy of the icon for .desktop files lives (an empty string if there's none)
    static QString getIconFilePath(const QString& liquidAppName);
    static void removeIconFile(const QString& liquidAppName);

    bool isNull(void) const;
    bool isSvg(void) const;

    // How good of an icon this is (vector images beat raster ones of any size)
    int score(void) const;

    QImage toImage(const int size = LQD_ICON_MAX_SIZE) const;
    QIcon toIcon(void) const;

private:
    QByteArray svg;
    QImage image;
};

// Finds the best icon a website has to offer (looking at the page that's currently shown), downloads all candidates
// and picks the best one
class LiquidAppIconFetcher : public QObject
{
    Q_OBJECT

public:
    LiquidAppIconFetcher(QWebEnginePage* page, QObject* parent = Q_NULLPTR);

    void start(void);

signals:
    // The icon is null if no usable one has been found
    void finished(const LiquidAppIcon& icon);

private:
    void onPageInspected(const QString& json);
    void download(const QUrl& url, const bool isMaskable);
    void onDownloaded(QNetworkReply* reply, const bool isMaskable);
    void onManifestDownloaded(QNetworkReply* reply);
    void finishIfDone(void);

    QWebEnginePage* page;
    QNetworkAccessManager* networkAccessManager;
    QList<QUrl> requestedUrls;
    int pendingRequestCount = 0;
    LiquidAppIcon bestIcon;
    qreal bestScore = 0;
    bool isDone = false;
};
