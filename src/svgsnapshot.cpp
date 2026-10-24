#include <cmath>

#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRectF>
#include <QRegularExpression>
#include <QVector>
#include <QXmlStreamWriter>

#include "svgsnapshot.hpp"

namespace {

const QString SVG_NS = QStringLiteral("http://www.w3.org/2000/svg");
const QString XLINK_NS = QStringLiteral("http://www.w3.org/1999/xlink");

const qreal MAX_COORDINATE = 10000000; // Anything beyond that is nonsense
const int MAX_TEXT_LENGTH = 100000;
const int MAX_FONT_FAMILY_LENGTH = 1000;

enum ItemType {
    RectItem,
    BorderItem,
    TextItem,
    ImageItem,
};

struct Item {
    ItemType type = RectItem;
    int clip = -1;
    qreal opacity = 1;

    // Rectangles, borders, and images
    QRectF rect;
    QVector<qreal> radii; // Empty if no rounded corners

    // Fill color of rectangles and text, stroke color of borders
    QColor color;

    // Borders
    qreal strokeWidth = 0;
    QString dash;

    // Text
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;
    qreal fontSize = 0;
    QString text;
    QString fontFamily;
    QString fontWeight;
    QString fontStyle;
    QString decoration;

    // Images
    QByteArray imageData; // Validated PNG or JPEG data
    QString imageMimeType;
    QString preserveAspectRatio;
    int roundedClip = -1;
};

// Formats a number using at most two decimal places, without trailing zeros
QString num(const qreal value)
{
    QString string = QString::number(value, 'f', 2);

    if (string.contains('.')) {
        while (string.endsWith('0')) {
            string.chop(1);
        }

        if (string.endsWith('.')) {
            string.chop(1);
        }
    }

    return (string == "-0") ? QStringLiteral("0") : string;
}

bool readNumber(const QJsonValue& value, qreal* result)
{
    if (!value.isDouble()) {
        return false;
    }

    const qreal number = value.toDouble();

    if (!std::isfinite(number) || std::fabs(number) > MAX_COORDINATE) {
        return false;
    }

    *result = number;

    return true;
}

bool readRect(const QJsonValue& value, QRectF* result)
{
    const QJsonArray array = value.toArray();
    qreal x, y, width, height;

    if (!value.isArray() || array.size() != 4
        || !readNumber(array[0], &x) || !readNumber(array[1], &y)
        || !readNumber(array[2], &width) || !readNumber(array[3], &height)
        || width < 0 || height < 0) {
        return false;
    }

    *result = QRectF(x, y, width, height);

    return true;
}

// Colors come as [red, green, blue, alpha], with alpha being in the range of 0 to 1
bool readColor(const QJsonValue& value, QColor* result)
{
    const QJsonArray array = value.toArray();
    qreal components[4];

    if (!value.isArray() || array.size() != 4) {
        return false;
    }

    for (int i = 0; i < 4; i++) {
        if (!readNumber(array[i], &components[i])) {
            return false;
        }
    }

    QColor color = QColor::fromRgb(qBound(0, qRound(components[0]), 255),
                                   qBound(0, qRound(components[1]), 255),
                                   qBound(0, qRound(components[2]), 255));
    color.setAlphaF(qBound(0.0, components[3], 1.0));

    if (color.alpha() == 0) {
        return false;
    }

    *result = color;

    return true;
}

// Corner radii come either as null, or as [top-left, top-right, bottom-right, bottom-left]
QVector<qreal> readRadii(const QJsonValue& value)
{
    const QJsonArray array = value.toArray();
    QVector<qreal> radii;

    if (!value.isArray() || array.size() != 4) {
        return radii;
    }

    bool hasAny = false;

    for (int i = 0; i < 4; i++) {
        qreal radius;

        if (!readNumber(array[i], &radius) || radius < 0) {
            return QVector<qreal>();
        }

        radii.append(radius);
        hasAny = hasAny || radius > 0;
    }

    return (hasAny) ? radii : QVector<qreal>();
}

// Strips characters that aren't allowed in XML (as well as line breaks, all text items are single-line)
QString sanitizeText(const QString& text, const int maxLength)
{
    QString result;
    result.reserve(qMin(text.size(), maxLength));

    for (int i = 0; i < text.size() && result.size() < maxLength; i++) {
        const QChar character = text.at(i);
        const uint code = character.unicode();

        if (character.isHighSurrogate()) {
            if (i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
                result += character;
                result += text.at(++i);
            }

            continue; // Lone surrogates get dropped
        }

        if (character.isLowSurrogate() || code == 0xFFFE || code == 0xFFFF) {
            continue;
        }

        if (code == 0x9 || code == 0xA || code == 0xD) {
            result += QLatin1Char(' ');
        } else if (code >= 0x20) {
            result += character;
        }
    }

    return result;
}

// Returns the value if it fully matches the pattern, otherwise the fallback
QString allowOnly(const QString& value, const QRegularExpression& pattern, const QString& fallback)
{
    return (pattern.match(value).hasMatch()) ? value : fallback;
}

// Accepts only base64-encoded PNG and JPEG data URLs whose contents actually decode as an image
bool readImage(const QJsonValue& value, QByteArray* data, QString* mimeType)
{
    static const QRegularExpression dataUrlPattern(QStringLiteral("^data:image/(png|jpeg);base64,([A-Za-z0-9+/]+={0,2})$"));

    const QRegularExpressionMatch match = dataUrlPattern.match(value.toString());

    if (!match.hasMatch()) {
        return false;
    }

    const QString format = match.captured(1);
    const QByteArray decoded = QByteArray::fromBase64(match.captured(2).toLatin1());
    QImage image;

    if (!image.loadFromData(decoded, (format == "png") ? "PNG" : "JPEG")) {
        return false;
    }

    *data = decoded;
    *mimeType = "image/" + format;

    return true;
}

bool parseItem(const QJsonObject& object, const int clipCount, Item* item)
{
    static const QRegularExpression fontWeightPattern(QStringLiteral("^(normal|bold|bolder|lighter|\\d{1,4})$"));
    static const QRegularExpression fontStylePattern(QStringLiteral("^(normal|italic|oblique)$"));
    static const QRegularExpression dashPattern(QStringLiteral("^(solid|dashed|dotted)$"));
    static const QRegularExpression preserveAspectRatioPattern(QStringLiteral("^(none|xMidYMid meet|xMidYMid slice)$"));

    const QString type = object.value("type").toString();

    item->clip = object.value("clip").toInt(-1);
    if (item->clip < -1 || item->clip >= clipCount) {
        item->clip = -1;
    }

    if (!readNumber(object.value("opacity"), &item->opacity)) {
        item->opacity = 1;
    }
    item->opacity = qBound(0.0, item->opacity, 1.0);

    if (item->opacity <= 0) {
        return false;
    }

    if (type == "rect") {
        item->type = RectItem;
        item->radii = readRadii(object.value("radii"));

        return readRect(object.value("rect"), &item->rect)
               && readColor(object.value("fill"), &item->color);
    } else if (type == "border") {
        item->type = BorderItem;
        item->radii = readRadii(object.value("radii"));
        item->dash = allowOnly(object.value("dash").toString(), dashPattern, "solid");

        return readRect(object.value("rect"), &item->rect)
               && readColor(object.value("color"), &item->color)
               && readNumber(object.value("width"), &item->strokeWidth) && item->strokeWidth > 0;
    } else if (type == "text") {
        item->type = TextItem;
        item->text = sanitizeText(object.value("text").toString(), MAX_TEXT_LENGTH);
        item->fontFamily = sanitizeText(object.value("fontFamily").toString(), MAX_FONT_FAMILY_LENGTH);
        item->fontWeight = allowOnly(object.value("fontWeight").toString(), fontWeightPattern, "normal");
        item->fontStyle = allowOnly(object.value("fontStyle").toString(), fontStylePattern, "normal");

        QStringList decorations;
        for (const QString& decoration : object.value("decoration").toString().split(' ')) {
            if ((decoration == "underline" || decoration == "line-through" || decoration == "overline")
                && !decorations.contains(decoration)) {
                decorations.append(decoration);
            }
        }
        item->decoration = decorations.join(' ');

        return !item->text.trimmed().isEmpty()
               && readNumber(object.value("x"), &item->x)
               && readNumber(object.value("y"), &item->y)
               && readNumber(object.value("width"), &item->width) && item->width >= 0
               && readNumber(object.value("fontSize"), &item->fontSize) && item->fontSize > 0
               && readColor(object.value("fill"), &item->color);
    } else if (type == "image") {
        item->type = ImageItem;
        item->radii = readRadii(object.value("radii"));
        item->preserveAspectRatio = allowOnly(object.value("preserveAspectRatio").toString(), preserveAspectRatioPattern, "none");

        return readRect(object.value("rect"), &item->rect)
               && !item->rect.isEmpty()
               && readImage(object.value("href"), &item->imageData, &item->imageMimeType);
    }

    return false;
}

// Starts a <rect> (or a <path>, for unevenly rounded corners) element and writes its geometry
void writeShapeStart(QXmlStreamWriter& writer, const QRectF& rect, const QVector<qreal>& radii)
{
    const bool isUniform = radii.isEmpty()
                           || (qFuzzyCompare(radii[0], radii[1]) && qFuzzyCompare(radii[0], radii[2]) && qFuzzyCompare(radii[0], radii[3]));

    if (isUniform) {
        writer.writeStartElement(SVG_NS, "rect");
        writer.writeAttribute("x", num(rect.x()));
        writer.writeAttribute("y", num(rect.y()));
        writer.writeAttribute("width", num(rect.width()));
        writer.writeAttribute("height", num(rect.height()));

        if (!radii.isEmpty() && radii[0] > 0) {
            writer.writeAttribute("rx", num(radii[0]));
            writer.writeAttribute("ry", num(radii[0]));
        }
    } else {
        const qreal x = rect.x();
        const qreal y = rect.y();
        const qreal w = rect.width();
        const qreal h = rect.height();
        const qreal tl = radii[0];
        const qreal tr = radii[1];
        const qreal br = radii[2];
        const qreal bl = radii[3];

        const QString path = QString("M%1,%2 H%3 A%4,%4 0 0 1 %5,%6 V%7 A%8,%8 0 0 1 %9,%10 H%11 A%12,%12 0 0 1 %13,%14 V%15 A%16,%16 0 0 1 %1,%2 Z")
            .arg(num(x + tl), num(y), num(x + w - tr), num(tr), num(x + w), num(y + tr), num(y + h - br), num(br), num(x + w - br))
            .arg(num(y + h), num(x + bl), num(bl), num(x), num(y + h - bl), num(y + tl), num(tl));

        writer.writeStartElement(SVG_NS, "path");
        writer.writeAttribute("d", path);
    }
}

void writeColor(QXmlStreamWriter& writer, const QString& attribute, const QColor& color)
{
    writer.writeAttribute(attribute, color.name(QColor::HexRgb));

    if (color.alpha() < 255) {
        writer.writeAttribute(attribute + "-opacity", num(color.alphaF()));
    }
}

void writeCommonAttributes(QXmlStreamWriter& writer, const Item& item)
{
    if (item.clip >= 0) {
        writer.writeAttribute("clip-path", QString("url(#c%1)").arg(item.clip));
    }

    if (item.opacity < 1) {
        writer.writeAttribute("opacity", num(item.opacity));
    }
}

void writeItem(QXmlStreamWriter& writer, const Item& item)
{
    switch (item.type) {
        case RectItem:
            writeShapeStart(writer, item.rect, item.radii);
            writeColor(writer, "fill", item.color);
            writeCommonAttributes(writer, item);
            writer.writeEndElement();
            break;

        case BorderItem:
            writeShapeStart(writer, item.rect, item.radii);
            writer.writeAttribute("fill", "none");
            writeColor(writer, "stroke", item.color);
            writer.writeAttribute("stroke-width", num(item.strokeWidth));
            if (item.dash == "dashed") {
                writer.writeAttribute("stroke-dasharray", QString("%1 %1").arg(num(item.strokeWidth * 3)));
            } else if (item.dash == "dotted") {
                writer.writeAttribute("stroke-dasharray", QString("%1 %1").arg(num(item.strokeWidth)));
            }
            writeCommonAttributes(writer, item);
            writer.writeEndElement();
            break;

        case TextItem:
            writer.writeStartElement(SVG_NS, "text");
            writer.writeAttribute("x", num(item.x));
            writer.writeAttribute("y", num(item.y));
            writer.writeAttribute("font-family", item.fontFamily);
            writer.writeAttribute("font-size", num(item.fontSize));
            if (item.fontWeight != "normal" && item.fontWeight != "400") {
                writer.writeAttribute("font-weight", item.fontWeight);
            }
            if (item.fontStyle != "normal") {
                writer.writeAttribute("font-style", item.fontStyle);
            }
            writeColor(writer, "fill", item.color);
            if (item.width > 0) {
                // Makes the text occupy exactly as much room as it did on the page, even if rendered using a different font
                writer.writeAttribute("textLength", num(item.width));
                writer.writeAttribute("lengthAdjust", "spacingAndGlyphs");
            }
            if (!item.decoration.isEmpty()) {
                writer.writeAttribute("text-decoration", item.decoration);
            }
            writer.writeAttribute("xml:space", "preserve");
            writeCommonAttributes(writer, item);
            writer.writeCharacters(item.text);
            writer.writeEndElement();
            break;

        case ImageItem:
            // Clipping by the parent group, rounded corners by the image itself
            if (item.clip >= 0 || item.opacity < 1) {
                writer.writeStartElement(SVG_NS, "g");
                writeCommonAttributes(writer, item);
            }

            writer.writeStartElement(SVG_NS, "image");
            writer.writeAttribute("x", num(item.rect.x()));
            writer.writeAttribute("y", num(item.rect.y()));
            writer.writeAttribute("width", num(item.rect.width()));
            writer.writeAttribute("height", num(item.rect.height()));
            writer.writeAttribute("preserveAspectRatio", item.preserveAspectRatio);
            if (item.roundedClip >= 0) {
                writer.writeAttribute("clip-path", QString("url(#r%1)").arg(item.roundedClip));
            }
            writer.writeAttribute(XLINK_NS, "href", "data:" + item.imageMimeType + ";base64," + QString::fromLatin1(item.imageData.toBase64()));
            writer.writeEndElement();

            if (item.clip >= 0 || item.opacity < 1) {
                writer.writeEndElement();
            }
            break;
    }
}

} // namespace

QByteArray SvgSnapshot::build(const QByteArray& json, const QColor& backdropColor, const qreal zoomFactor, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject root = document.object();

    qreal width = 0;
    qreal height = 0;

    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || root.value("version").toInt() != 1
        || !readNumber(root.value("width"), &width) || width <= 0
        || !readNumber(root.value("height"), &height) || height <= 0) {
        if (error) {
            *error = "Invalid page data";
        }

        return QByteArray();
    }

    // Clips (invalid ones become empty, to keep indices intact)
    QVector<QRectF> clips;
    for (const QJsonValue& value : root.value("clips").toArray()) {
        QRectF rect;
        clips.append((readRect(value, &rect)) ? rect : QRectF());
    }

    // Items
    QVector<Item> items;
    int roundedClipCount = 0;
    for (const QJsonValue& value : root.value("items").toArray()) {
        Item item;

        if (parseItem(value.toObject(), clips.size(), &item)) {
            if (item.type == ImageItem && !item.radii.isEmpty()) {
                item.roundedClip = roundedClipCount++;
            }

            items.append(item);
        }
    }

    const qreal zoom = (std::isfinite(zoomFactor) && zoomFactor > 0) ? zoomFactor : 1;

    QByteArray output;
    QXmlStreamWriter writer(&output);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(0);

    writer.writeStartDocument();
    writer.writeDefaultNamespace(SVG_NS);
    writer.writeNamespace(XLINK_NS, "xlink");
    writer.writeStartElement(SVG_NS, "svg");
    writer.writeAttribute("version", "1.1");
    writer.writeAttribute("width", num(width * zoom));
    writer.writeAttribute("height", num(height * zoom));
    writer.writeAttribute("viewBox", QString("0 0 %1 %2").arg(num(width), num(height)));

    // Definitions
    if (!clips.isEmpty() || roundedClipCount > 0) {
        writer.writeStartElement(SVG_NS, "defs");

        for (int i = 0; i < clips.size(); i++) {
            writer.writeStartElement(SVG_NS, "clipPath");
            writer.writeAttribute("id", QString("c%1").arg(i));
            writeShapeStart(writer, clips[i], QVector<qreal>());
            writer.writeEndElement(); // rect
            writer.writeEndElement(); // clipPath
        }

        for (const Item& item : items) {
            if (item.roundedClip >= 0) {
                writer.writeStartElement(SVG_NS, "clipPath");
                writer.writeAttribute("id", QString("r%1").arg(item.roundedClip));
                writeShapeStart(writer, item.rect, item.radii);
                writer.writeEndElement(); // rect or path
                writer.writeEndElement(); // clipPath
            }
        }

        writer.writeEndElement(); // defs
    }

    // The color Liquid paints behind the web page (may be partially or fully transparent)
    if (backdropColor.isValid() && backdropColor.alpha() > 0) {
        writer.writeStartElement(SVG_NS, "rect");
        writer.writeAttribute("width", num(width));
        writer.writeAttribute("height", num(height));
        writeColor(writer, "fill", backdropColor);
        writer.writeEndElement();
    }

    for (const Item& item : items) {
        writeItem(writer, item);
    }

    writer.writeEndElement(); // svg
    writer.writeEndDocument();

    if (writer.hasError()) {
        if (error) {
            *error = "Unable to write SVG";
        }

        return QByteArray();
    }

    return output;
}
