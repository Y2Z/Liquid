#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSvgRenderer>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>

#include "liquid.hpp"
#include "liquidappicon.hpp"
#include "liquidapplauncher.hpp"
#include "lqd.h"

static const QString PNG_DATA_URL_PREFIX = QStringLiteral("data:image/png;base64,");
static const QString SVG_DATA_URL_PREFIX = QStringLiteral("data:image/svg+xml;base64,");

// Sizes vector icons get rendered at for showing them in the program (so that none ever needs to get scaled)
static const int SVG_ICON_SIZES[] = { 16, 22, 24, 32, 48, 64, 96, 128, 256 };

// Scripts can't tell what the page links to without JS (which may be disabled for the page itself),
// so this runs in Liquid's own JS world; it only reads the document, never changes it
static const char* const ICON_LINKS_SCRIPT =
    "(function() {"
        "const result = { icons: [], manifest: null };"
        "for (const link of document.querySelectorAll('link[rel][href]')) {"
            "const rel = link.getAttribute('rel').toLowerCase().split(/\\s+/);"
            "if (rel.includes('manifest')) {"
                "result.manifest = result.manifest || link.href;"
            "} else if (rel.includes('icon') || rel.includes('apple-touch-icon') || rel.includes('apple-touch-icon-precomposed')) {"
                "result.icons.push(link.href);"
            "}"
        "}"
        "return JSON.stringify(result);"
    "})()";

static bool looksLikeSvg(const QByteArray& data)
{
    // Compressed SVG (.svgz) included
    if (!data.startsWith("\x1f\x8b") && !data.left(4096).toLower().contains("<svg")) {
        return false;
    }

    return QSvgRenderer(data).isValid();
}

// Whether the SVG is a picture that's complete in itself, i.e. safe to keep as-is and hand over to other programs
// (e.g. whatever renders icons for the desktop): it must not run anything, nor refer to anything outside of itself
static bool isSelfContainedSvg(const QByteArray& data)
{
    if (data.size() > LQD_ICON_MAX_SVG_SIZE || data.startsWith("\x1f\x8b")) {
        return false;
    }

    const QString svg = QString::fromUtf8(data);

    static const QRegularExpression forbidden(QStringLiteral("<\\s*(script|foreignObject|iframe|!ENTITY)|@import|\\bon[a-z]+\\s*="),
                                              QRegularExpression::CaseInsensitiveOption);
    if (svg.contains(forbidden)) {
        return false;
    }

    // References may only point to the image's own parts, or to embedded images
    static const QRegularExpression href(QStringLiteral("\\bhref\\s*=\\s*([\"'])\\s*([^\"']*)\\1"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression url(QStringLiteral("\\burl\\s*\\(\\s*[\"']?\\s*([^\"')\\s]*)"), QRegularExpression::CaseInsensitiveOption);
    const auto isInternal = [](const QString& reference) {
        return reference.startsWith('#') || reference.startsWith(QLatin1String("data:image/"), Qt::CaseInsensitive);
    };

    for (const QRegularExpression& pattern : { href, url }) {
        QRegularExpressionMatchIterator matches = pattern.globalMatch(svg);

        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();

            if (!isInternal(match.captured(match.lastCapturedIndex()))) {
                return false;
            }
        }
    }

    return true;
}

static QImage renderSvg(const QByteArray& data, const int size)
{
    QSvgRenderer renderer(data);

    if (!renderer.isValid()) {
        return QImage();
    }

    QSize imageSize = renderer.defaultSize();
    if (imageSize.isEmpty()) {
        imageSize = QSize(size, size);
    }
    imageSize = imageSize.scaled(size, size, Qt::KeepAspectRatio);

    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    painter.end();

    return image.convertToFormat(QImage::Format_ARGB32);
}

// Picks the largest image for files containing several (e.g. .ico)
static QImage decodeRaster(const QByteArray& data)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);

    QImage largestImage;
    int i = 0;
    do {
        const QImage image = reader.read();

        if (!image.isNull() && qMin(image.width(), image.height()) > qMin(largestImage.width(), largestImage.height())) {
            largestImage = image;
        }
    } while (++i < 32 && reader.jumpToNextImage());

    return largestImage;
}

LiquidAppIcon LiquidAppIcon::fromData(const QByteArray& data)
{
    if (looksLikeSvg(data)) {
        if (isSelfContainedSvg(data)) {
            LiquidAppIcon icon;
            icon.svg = data;

            return icon;
        }

        return fromImage(renderSvg(data, LQD_ICON_MAX_SIZE));
    }

    return fromImage(decodeRaster(data));
}

LiquidAppIcon LiquidAppIcon::fromImage(const QImage& image)
{
    LiquidAppIcon icon;

    if (!image.isNull()) {
        icon.image = image.convertToFormat(QImage::Format_ARGB32);

        // Large raster icons get scaled down before they get stored
        if (icon.image.width() > LQD_ICON_MAX_SIZE || icon.image.height() > LQD_ICON_MAX_SIZE) {
            icon.image = icon.image.scaled(LQD_ICON_MAX_SIZE, LQD_ICON_MAX_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    return icon;
}

LiquidAppIcon LiquidAppIcon::fromIcon(const QIcon& icon)
{
    QSize largestSize;

    for (const QSize& size : icon.availableSizes()) {
        if (size.width() * size.height() > largestSize.width() * largestSize.height()) {
            largestSize = size;
        }
    }

    if (largestSize.isEmpty()) {
        return LiquidAppIcon();
    }

    return fromImage(icon.pixmap(largestSize).toImage());
}

LiquidAppIcon LiquidAppIcon::load(QSettings* liquidAppConfig)
{
    const QString value = liquidAppConfig->value(LQD_CFG_KEY_NAME_ICON).toString();
    LiquidAppIcon icon;

    if (value.startsWith(SVG_DATA_URL_PREFIX)) {
        icon.svg = QByteArray::fromBase64(value.mid(SVG_DATA_URL_PREFIX.size()).toLatin1());
    } else if (value.startsWith(PNG_DATA_URL_PREFIX)) {
        icon.image.loadFromData(QByteArray::fromBase64(value.mid(PNG_DATA_URL_PREFIX.size()).toLatin1()), "PNG");
    }

    return icon;
}

QIcon LiquidAppIcon::loadIcon(QSettings* liquidAppConfig)
{
    const LiquidAppIcon icon = load(liquidAppConfig);

    return (icon.isNull()) ? QIcon(":/images/" PROG_NAME ".svg") : icon.toIcon();
}

void LiquidAppIcon::save(QSettings* liquidAppConfig, const QString& liquidAppName) const
{
    if (isNull()) {
        return;
    }

    QByteArray fileData;
    if (isSvg()) {
        fileData = svg;
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_ICON, SVG_DATA_URL_PREFIX + QString::fromLatin1(svg.toBase64()));
    } else {
        QBuffer buffer(&fileData);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        liquidAppConfig->setValue(LQD_CFG_KEY_NAME_ICON, PNG_DATA_URL_PREFIX + QString::fromLatin1(fileData.toBase64()));
    }
    liquidAppConfig->sync();

#if defined(Q_OS_LINUX)
    // .desktop files can only point to icons, not contain them
    removeIconFile(liquidAppName);

    const QDir iconsDir(Liquid::getDataDir().filePath(LQD_ICONS_DIR_NAME));
    QDir().mkpath(iconsDir.path());
    QSaveFile iconFile(iconsDir.filePath(liquidAppName + ((isSvg()) ? ".svg" : ".png")));
    if (iconFile.open(QIODevice::WriteOnly)) {
        iconFile.write(fileData);
        iconFile.commit();
    }

    LiquidAppLauncher::update(liquidAppName);
#else
    Q_UNUSED(liquidAppName);
#endif
}

void LiquidAppIcon::remove(QSettings* liquidAppConfig, const QString& liquidAppName)
{
    liquidAppConfig->remove(LQD_CFG_KEY_NAME_ICON);
    liquidAppConfig->sync();

    removeIconFile(liquidAppName);
    LiquidAppLauncher::update(liquidAppName);
}

void LiquidAppIcon::ensureIconFile(QSettings* liquidAppConfig, const QString& liquidAppName)
{
#if defined(Q_OS_LINUX)
    if (getIconFilePath(liquidAppName).isEmpty()) {
        load(liquidAppConfig).save(liquidAppConfig, liquidAppName);
    }
#else
    Q_UNUSED(liquidAppConfig);
    Q_UNUSED(liquidAppName);
#endif
}

QString LiquidAppIcon::getIconFilePath(const QString& liquidAppName)
{
    const QDir iconsDir(Liquid::getDataDir().filePath(LQD_ICONS_DIR_NAME));

    for (const char* const extension : { ".svg", ".png" }) {
        const QString path = iconsDir.filePath(liquidAppName + extension);

        if (QFile::exists(path)) {
            return path;
        }
    }

    return QString();
}

void LiquidAppIcon::removeIconFile(const QString& liquidAppName)
{
    const QDir iconsDir(Liquid::getDataDir().filePath(LQD_ICONS_DIR_NAME));

    QFile::remove(iconsDir.filePath(liquidAppName + ".svg"));
    QFile::remove(iconsDir.filePath(liquidAppName + ".png"));
}

bool LiquidAppIcon::isNull(void) const
{
    return svg.isEmpty() && image.isNull();
}

bool LiquidAppIcon::isSvg(void) const
{
    return !svg.isEmpty();
}

int LiquidAppIcon::score(void) const
{
    if (isSvg()) {
        return LQD_ICON_MAX_SIZE * 2;
    }

    return qMin(image.width(), image.height());
}

QImage LiquidAppIcon::toImage(const int size) const
{
    if (isSvg()) {
        return renderSvg(svg, size);
    }

    if (image.width() > size || image.height() > size) {
        return image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

QIcon LiquidAppIcon::toIcon(void) const
{
    if (isNull()) {
        return QIcon();
    }

    if (!isSvg()) {
        return QIcon(QPixmap::fromImage(image));
    }

    QIcon icon;
    for (const int size : SVG_ICON_SIZES) {
        icon.addPixmap(QPixmap::fromImage(renderSvg(svg, size)));
    }

    return icon;
}

LiquidAppIconFetcher::LiquidAppIconFetcher(QWebEnginePage* page, QObject* parent) : QObject(parent), page(page)
{
    networkAccessManager = new QNetworkAccessManager(this);
    networkAccessManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

void LiquidAppIconFetcher::start(void)
{
    QPointer<LiquidAppIconFetcher> self(this);

    page->runJavaScript(ICON_LINKS_SCRIPT, QWebEngineScript::ApplicationWorld, [self](const QVariant& result) {
        if (self) {
            self->onPageInspected(result.toString());
        }
    });
}

void LiquidAppIconFetcher::onPageInspected(const QString& json)
{
    const QJsonObject result = QJsonDocument::fromJson(json.toUtf8()).object();
    const QUrl pageUrl = page->url();

    // Whatever the page links to
    for (const QJsonValue& icon : result.value("icons").toArray()) {
        download(QUrl(icon.toString()), false);
    }

    // Icons for "installing" the website, these tend to be the largest ones
    const QString manifest = result.value("manifest").toString();
    if (!manifest.isEmpty()) {
        const QUrl manifestUrl(manifest);

        if (manifestUrl.scheme() == "http" || manifestUrl.scheme() == "https") {
            QNetworkRequest request(manifestUrl);
            request.setHeader(QNetworkRequest::UserAgentHeader, page->profile()->httpUserAgent());
            request.setTransferTimeout(LQD_ICON_DOWNLOAD_TIMEOUT);

            QNetworkReply* reply = networkAccessManager->get(request);
            pendingRequestCount++;
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                onManifestDownloaded(reply);
            });
        }
    }

    // Places where websites tend to keep icons without linking to them
    if (pageUrl.scheme() == "http" || pageUrl.scheme() == "https") {
        download(pageUrl.resolved(QUrl("/apple-touch-icon.png")), false);
        download(pageUrl.resolved(QUrl("/favicon.ico")), false);
    }

    finishIfDone();
}

void LiquidAppIconFetcher::download(const QUrl& url, const bool isMaskable)
{
    const QString scheme = url.scheme();

    if (!url.isValid() || (scheme != "http" && scheme != "https" && scheme != "data")) {
        return;
    }

    if (requestedUrls.contains(url) || requestedUrls.size() >= LQD_ICON_MAX_CANDIDATES) {
        return;
    }
    requestedUrls << url;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, page->profile()->httpUserAgent());
    request.setTransferTimeout(LQD_ICON_DOWNLOAD_TIMEOUT);

    QNetworkReply* reply = networkAccessManager->get(request);
    pendingRequestCount++;

    // Icons are small, anything big is something else
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](const qint64 bytesReceived, const qint64 bytesTotal) {
        if (bytesReceived > LQD_ICON_MAX_FILE_SIZE || bytesTotal > LQD_ICON_MAX_FILE_SIZE) {
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, isMaskable]() {
        onDownloaded(reply, isMaskable);
    });
}

void LiquidAppIconFetcher::onDownloaded(QNetworkReply* reply, const bool isMaskable)
{
    reply->deleteLater();
    pendingRequestCount--;

    if (reply->error() == QNetworkReply::NoError) {
        const LiquidAppIcon icon = LiquidAppIcon::fromData(reply->readAll());

        if (!icon.isNull()) {
            // Maskable icons are meant to get cropped (they have extra space around them),
            // so others of about the same size are preferred
            const qreal score = icon.score() * ((isMaskable) ? 0.75 : 1.0);

            if (score > bestScore) {
                bestScore = score;
                bestIcon = icon;
            }
        }
    }

    finishIfDone();
}

void LiquidAppIconFetcher::onManifestDownloaded(QNetworkReply* reply)
{
    reply->deleteLater();
    pendingRequestCount--;

    if (reply->error() == QNetworkReply::NoError && reply->size() <= LQD_ICON_MAX_FILE_SIZE) {
        const QUrl manifestUrl = reply->url();
        const QJsonObject manifest = QJsonDocument::fromJson(reply->readAll()).object();

        for (const QJsonValue& value : manifest.value("icons").toArray()) {
            const QJsonObject icon = value.toObject();
            const QStringList purposes = icon.value("purpose").toString("any").toLower().split(' ', Qt::SkipEmptyParts);

            // Single-color silhouettes aren't meant to be shown as-is
            if (!purposes.contains("any") && !purposes.contains("maskable")) {
                continue;
            }

            download(manifestUrl.resolved(QUrl(icon.value("src").toString())), !purposes.contains("any"));
        }
    }

    finishIfDone();
}

void LiquidAppIconFetcher::finishIfDone(void)
{
    if (pendingRequestCount > 0 || isDone) {
        return;
    }

    isDone = true;
    emit finished(bestIcon);
}
