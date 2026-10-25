#pragma once

#include <QSettings>

// The .liquid file format: a single, human-readable XML file per Liquid app, containing everything about it
// (settings, custom CSS and JS, state, cookies, icon), with multi-line code stored verbatim in CDATA sections.
//
// It's plugged into QSettings as a custom format, so the rest of the program keeps working with familiar
// QSettings keys (e.g. "URL", "Proxy/Host"), while this class maps them to and from XML.
class LiquidAppFile
{
public:
    // Registers the format with QSettings upon first use
    static QSettings::Format format(void);

    // Opens the .liquid file of the Liquid app by the given name (the caller takes ownership)
    static QSettings* open(const QString& liquidAppName);

private:
    static bool read(QIODevice& device, QSettings::SettingsMap& map);
    static bool write(QIODevice& device, const QSettings::SettingsMap& map);
};
