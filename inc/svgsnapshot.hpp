#pragma once

#include <QByteArray>
#include <QColor>
#include <QString>

// Turns the output of html2svg.js (a JSON list of primitive shapes) into an SVG document.
//
// The page that the shapes came from is untrusted, so nothing gets copied from it into the SVG as-is:
// only a fixed set of elements and attributes ever gets written, every value gets validated,
// and embedded images get decoded and re-encoded. The result has no scripts, no event handlers,
// no external references, and no embedded HTML, i.e. it's a harmless, frozen picture of the page.
class SvgSnapshot
{
public:
    // Returns an empty array (and sets the error, if provided) upon failure
    static QByteArray build(const QByteArray& json, const QColor& backdropColor, const qreal zoomFactor, QString* error = Q_NULLPTR);
};
