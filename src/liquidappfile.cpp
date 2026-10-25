#include <QIODevice>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "liquid.hpp"
#include "liquidappfile.hpp"
#include "lqd.h"

namespace {

const int FORMAT_VERSION = 1;
const QString ROOT_ELEMENT = QStringLiteral("liquid");
// Data URL prefixes of the image types icons can be stored as
const QStringList ICON_TYPES = { QStringLiteral("image/png"), QStringLiteral("image/svg+xml") };
const QString COOKIES_KEY_PREFIX = QStringLiteral(LQD_CFG_GROUP_NAME_COOKIES "/");

// Settings stored as text of their own elements
struct TextMapping {
    const char* key;
    const char* element;
};

const TextMapping TEXT_MAPPINGS[] = {
    { LQD_CFG_KEY_NAME_URL,        "url" },
    { LQD_CFG_KEY_NAME_TITLE,      "title" },
    { LQD_CFG_KEY_NAME_NOTES,      "notes" },
    { LQD_CFG_KEY_NAME_USER_AGENT, "user-agent" },
};

// Settings stored as attributes, grouped into elements (written in the order of their first appearance)
struct AttributeMapping {
    const char* key;
    const char* element;
    const char* attribute;
};

const AttributeMapping ATTRIBUTE_MAPPINGS[] = {
    { LQD_CFG_KEY_NAME_ENABLE_JS,               "javascript", "enabled" },
    { LQD_CFG_KEY_NAME_ALLOW_COOKIES,           "cookies",    "enabled" },
    { LQD_CFG_KEY_NAME_ALLOW_3RD_PARTY_COOKIES, "cookies",    "third-party" },
    { LQD_CFG_KEY_NAME_WIN_X,                   "window",     "x" },
    { LQD_CFG_KEY_NAME_WIN_Y,                   "window",     "y" },
    { LQD_CFG_KEY_NAME_WIN_WIDTH,               "window",     "width" },
    { LQD_CFG_KEY_NAME_WIN_HEIGHT,              "window",     "height" },
    { LQD_CFG_KEY_NAME_WIN_MAXIMIZED,           "window",     "maximized" },
    { LQD_CFG_KEY_NAME_WIN_FULLSCREEN,          "window",     "fullscreen" },
    { LQD_CFG_KEY_NAME_LOCK_WIN_GEOM,           "window",     "locked" },
    { LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME,     "window",     "frameless" },
    { LQD_CFG_KEY_NAME_ZOOM_LVL,                "viewport",   "zoom" },
    { LQD_CFG_KEY_NAME_HIDE_SCROLLBARS,         "viewport",   "hide-scrollbars" },
    { LQD_CFG_KEY_NAME_USE_CUSTOM_BG,           "viewport",   "custom-background" },
    { LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR,         "viewport",   "background-color" },
    { LQD_CFG_KEY_NAME_MUTE_AUDIO,              "audio",      "muted" },
    { LQD_CFG_KEY_NAME_USE_PROXY,               "proxy",      "enabled" },
    { LQD_CFG_KEY_NAME_PROXY_USE_SOCKS,         "proxy",      "socks" },
    { LQD_CFG_KEY_NAME_PROXY_HOST,              "proxy",      "host" },
    { LQD_CFG_KEY_NAME_PROXY_PORT,              "proxy",      "port" },
    { LQD_CFG_KEY_NAME_PROXY_USE_AUTH,          "proxy",      "authenticate" },
    { LQD_CFG_KEY_NAME_PROXY_USER_NAME,         "proxy",      "username" },
    { LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD,     "proxy",      "password" },
};

// Multi-line code, stored verbatim (in CDATA sections)
struct CodeMapping {
    const char* key;
    const char* element;
};

const CodeMapping CODE_MAPPINGS[] = {
    { LQD_CFG_KEY_NAME_ADDITIONAL_CSS, "style" },
    { LQD_CFG_KEY_NAME_ADDITIONAL_JS,  "script" },
};

// Strips characters that aren't allowed in XML documents
QString sanitize(const QString& text)
{
    QString result;
    result.reserve(text.size());

    for (int i = 0; i < text.size(); i++) {
        const QChar character = text.at(i);
        const uint code = character.unicode();

        if (character.isHighSurrogate()) {
            if (i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
                result += character;
                result += text.at(++i);
            }
        } else if (character.isLowSurrogate() || code == 0xFFFE || code == 0xFFFF) {
            continue;
        } else if (code >= 0x20 || code == 0x9 || code == 0xA || code == 0xD) {
            result += character;
        }
    }

    return result;
}

// Removes the setting from the map, returning its value as text
bool take(QSettings::SettingsMap& map, const QString& key, QString* value)
{
    const QSettings::SettingsMap::iterator it = map.find(key);

    if (it == map.end()) {
        return false;
    }

    *value = sanitize(it.value().toString());
    map.erase(it);

    return true;
}

} // namespace

QSettings::Format LiquidAppFile::format(void)
{
    static const QSettings::Format registeredFormat =
        QSettings::registerFormat(LQD_APP_FILE_EXTENSION, &LiquidAppFile::read, &LiquidAppFile::write, Qt::CaseSensitive);

    return registeredFormat;
}

QSettings* LiquidAppFile::open(const QString& liquidAppName)
{
    return new QSettings(Liquid::getLiquidAppFilePath(liquidAppName), format());
}

bool LiquidAppFile::read(QIODevice& device, QSettings::SettingsMap& map)
{
    QXmlStreamReader xml(&device);

    if (!xml.readNextStartElement() || xml.name() != ROOT_ELEMENT) {
        return false;
    }

    // Anything this version of Liquid doesn't recognize (e.g. made by a newer one) gets skipped
    while (xml.readNextStartElement()) {
        const QString element = xml.name().toString();
        bool isHandled = false;

        for (const TextMapping& mapping : TEXT_MAPPINGS) {
            if (element == mapping.element) {
                map.insert(mapping.key, xml.readElementText());
                isHandled = true;
                break;
            }
        }

        for (const CodeMapping& mapping : CODE_MAPPINGS) {
            if (!isHandled && element == mapping.element) {
                map.insert(mapping.key, xml.readElementText());
                isHandled = true;
            }
        }

        if (isHandled) {
            continue;
        }

        if (element == "domains") {
            QStringList domains;

            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("domain")) {
                    const QString domain = xml.readElementText().trimmed();

                    if (!domain.isEmpty()) {
                        domains.append(domain);
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }

            if (!domains.isEmpty()) {
                map.insert(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS, domains.join(' '));
            }
        } else if (element == "cookie-jar") {
            while (xml.readNextStartElement()) {
                const QString id = xml.attributes().value("id").toString();

                if (xml.name() == QLatin1String("cookie") && !id.isEmpty()) {
                    map.insert(COOKIES_KEY_PREFIX + id, xml.readElementText());
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else if (element == "settings") {
            while (xml.readNextStartElement()) {
                const QString key = xml.attributes().value("key").toString();

                if (xml.name() == QLatin1String("setting") && !key.isEmpty()) {
                    map.insert(key, xml.readElementText());
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else if (element == "icon") {
            static const QRegularExpression whitespace(QStringLiteral("\\s"));

            QString type = xml.attributes().value("type").toString();
            if (!ICON_TYPES.contains(type)) {
                type = ICON_TYPES.first();
            }

            map.insert(LQD_CFG_KEY_NAME_ICON, "data:" + type + ";base64," + xml.readElementText().remove(whitespace));
        } else {
            const QXmlStreamAttributes attributes = xml.attributes();

            for (const AttributeMapping& mapping : ATTRIBUTE_MAPPINGS) {
                if (element == mapping.element && attributes.hasAttribute(mapping.attribute)) {
                    map.insert(mapping.key, attributes.value(mapping.attribute).toString());
                }
            }

            xml.skipCurrentElement();
        }
    }

    return !xml.hasError();
}

bool LiquidAppFile::write(QIODevice& device, const QSettings::SettingsMap& settingsMap)
{
    // Settings get removed from this copy as they're written, whatever remains goes into <settings> in the end
    QSettings::SettingsMap map = settingsMap;
    QString value;

    QXmlStreamWriter xml(&device);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);

    xml.writeStartDocument();
    xml.writeStartElement(ROOT_ELEMENT);
    xml.writeAttribute("version", QString::number(FORMAT_VERSION));

    for (const TextMapping& mapping : TEXT_MAPPINGS) {
        if (take(map, mapping.key, &value)) {
            xml.writeTextElement(mapping.element, value);
        }
    }

    if (take(map, LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS, &value)) {
        xml.writeStartElement("domains");

        for (const QString& domain : value.split(' ', Qt::SkipEmptyParts)) {
            xml.writeTextElement("domain", domain);
        }

        xml.writeEndElement();
    }

    // Elements made of attributes
    QStringList writtenElements;
    for (const AttributeMapping& elementMapping : ATTRIBUTE_MAPPINGS) {
        if (writtenElements.contains(elementMapping.element)) {
            continue;
        }
        writtenElements.append(elementMapping.element);

        QXmlStreamAttributes attributes;
        for (const AttributeMapping& mapping : ATTRIBUTE_MAPPINGS) {
            if (QLatin1String(mapping.element) == QLatin1String(elementMapping.element) && take(map, mapping.key, &value)) {
                attributes.append(mapping.attribute, value);
            }
        }

        if (!attributes.isEmpty()) {
            xml.writeEmptyElement(elementMapping.element);
            xml.writeAttributes(attributes);
        }
    }

    for (const CodeMapping& mapping : CODE_MAPPINGS) {
        if (take(map, mapping.key, &value)) {
            xml.writeStartElement(mapping.element);
            xml.writeCDATA(value); // Sections containing "]]>" get split automatically
            xml.writeEndElement();
        }
    }

    // Cookies and the icon go last, since they're long and not meant to be read by humans
    QStringList cookieKeys;
    for (const QString& key : map.keys()) {
        if (key.startsWith(COOKIES_KEY_PREFIX)) {
            cookieKeys.append(key);
        }
    }

    QString icon;
    QString iconType;
    take(map, LQD_CFG_KEY_NAME_ICON, &icon);
    for (const QString& type : ICON_TYPES) {
        if (icon.startsWith("data:" + type + ";base64,")) {
            iconType = type;
        }
    }
    const bool hasIcon = !iconType.isEmpty();
    if (!hasIcon && !icon.isEmpty()) {
        map.insert(LQD_CFG_KEY_NAME_ICON, icon); // Not an image data URL of a known type, keep it as a generic setting
    }

    // Anything else
    bool hasOtherSettings = false;
    for (QSettings::SettingsMap::const_iterator it = map.constBegin(); it != map.constEnd(); ++it) {
        if (cookieKeys.contains(it.key())) {
            continue;
        }

        if (!hasOtherSettings) {
            xml.writeStartElement("settings");
            hasOtherSettings = true;
        }

        xml.writeStartElement("setting");
        xml.writeAttribute("key", sanitize(it.key()));
        xml.writeCharacters(sanitize(it.value().toString()));
        xml.writeEndElement();
    }
    if (hasOtherSettings) {
        xml.writeEndElement();
    }

    if (!cookieKeys.isEmpty()) {
        xml.writeStartElement("cookie-jar");

        for (const QString& key : cookieKeys) {
            xml.writeStartElement("cookie");
            xml.writeAttribute("id", sanitize(key.mid(COOKIES_KEY_PREFIX.size())));
            xml.writeCharacters(sanitize(map.value(key).toString()));
            xml.writeEndElement();
        }

        xml.writeEndElement();
    }

    if (hasIcon) {
        xml.writeStartElement("icon");
        xml.writeAttribute("type", iconType);
        xml.writeCharacters(icon.mid(QString("data:" + iconType + ";base64,").size()));
        xml.writeEndElement();
    }

    xml.writeEndElement(); // liquid
    xml.writeEndDocument();

    return !xml.hasError();
}
