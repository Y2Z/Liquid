// html2svg.js, the engine behind Liquid's vector snapshots.
//
// Runs inside of the page (in an isolated JavaScript world, which works even when JavaScript is disabled
// for the page, and can't be tampered with by the page's own scripts), walks the rendered page, and produces
// a flat list of primitive shapes (rectangles, borders, lines of text, raster images) in document coordinates.
//
// This script does NOT produce any SVG markup itself: Liquid turns this list into SVG on its own,
// only ever writing out elements and attributes it knows to be harmless.
//
// Since some parts of the work are asynchronous (loading and rasterizing images), the result gets placed into
// window.lqdHtml2Svg (as a JSON string), for Liquid to pick it up once it's marked as done.

(() => {
    const state = window.lqdHtml2Svg = { done: false, result: null, error: null };

    const MAX_ITEMS = 500000;
    const MAX_IMAGE_SCALE = 2; // Raster images get embedded at up to 2x their displayed size
    const SVG_NS = "http://www.w3.org/2000/svg";

    const scrollX = window.scrollX;
    const scrollY = window.scrollY;
    const devicePixelRatio = window.devicePixelRatio || 1;

    const items = [];
    const clips = [];
    const clipIndexByKey = new Map();
    const pendingTasks = [];
    let skippedImageCount = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Colors
    ////////////////////////////////////////////////////////////////////////////

    const colorCanvas = document.createElement("canvas");
    colorCanvas.width = colorCanvas.height = 1;
    const colorContext = colorCanvas.getContext("2d", { willReadFrequently: true });
    const colorCache = new Map();

    // Returns [r, g, b, a] (0-255, 0-255, 0-255, 0-1), or null if fully transparent
    function parseColor(cssColor) {
        if (!cssColor || cssColor === "transparent") {
            return null;
        }

        let color = colorCache.get(cssColor);

        if (color === undefined) {
            color = null;

            const match = /^rgba?\(\s*([\d.]+)[\s,]+([\d.]+)[\s,]+([\d.]+)(?:[\s,/]+([\d.]+)(%?))?\s*\)$/.exec(cssColor);

            if (match) {
                let alpha = (match[4] === undefined) ? 1 : parseFloat(match[4]);

                if (match[5] === "%") {
                    alpha /= 100;
                }

                color = [Math.round(+match[1]), Math.round(+match[2]), Math.round(+match[3]), alpha];
            } else {
                // Any other color syntax (e.g. oklch() or color()): let the browser paint it, then read the pixel back
                colorContext.clearRect(0, 0, 1, 1);
                colorContext.fillStyle = "rgba(0, 0, 0, 0)";
                colorContext.fillStyle = cssColor;
                colorContext.fillRect(0, 0, 1, 1);

                const pixel = colorContext.getImageData(0, 0, 1, 1).data;
                color = [pixel[0], pixel[1], pixel[2], pixel[3] / 255];
            }

            if (color && color[3] <= 0) {
                color = null;
            }

            colorCache.set(cssColor, color);
        }

        return color;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Geometry
    ////////////////////////////////////////////////////////////////////////////

    function round(n) {
        return Math.round(n * 100) / 100;
    }

    // Converts a client rect into document coordinates
    function toDocumentRect(rect) {
        return { x: rect.left + scrollX, y: rect.top + scrollY, w: rect.width, h: rect.height };
    }

    function intersectRects(a, b) {
        const x = Math.max(a.x, b.x);
        const y = Math.max(a.y, b.y);
        const right = Math.min(a.x + a.w, b.x + b.w);
        const bottom = Math.min(a.y + a.h, b.y + b.h);

        return { x: x, y: y, w: Math.max(0, right - x), h: Math.max(0, bottom - y) };
    }

    function rectsOverlap(a, b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    }

    function getClipRect(clipIndex) {
        return (clipIndex < 0) ? null : clips[clipIndex].rect;
    }

    function addClip(parentClipIndex, rect) {
        const parentRect = getClipRect(parentClipIndex);
        const clipRect = (parentRect) ? intersectRects(parentRect, rect) : rect;
        const key = [round(clipRect.x), round(clipRect.y), round(clipRect.w), round(clipRect.h)].join(",");

        let index = clipIndexByKey.get(key);

        if (index === undefined) {
            index = clips.length;
            clips.push({ rect: clipRect, data: [round(clipRect.x), round(clipRect.y), round(clipRect.w), round(clipRect.h)] });
            clipIndexByKey.set(key, index);
        }

        return index;
    }

    // Tells whether a rectangle would be visible at all, given the clip it's subject to
    function isWorthPainting(rect, context) {
        if (rect.w <= 0 || rect.h <= 0) {
            return false;
        }

        const clipRect = getClipRect(context.clip);

        return !clipRect || rectsOverlap(rect, clipRect);
    }

    function getBorderWidths(style) {
        return {
            top: parseFloat(style.borderTopWidth) || 0,
            right: parseFloat(style.borderRightWidth) || 0,
            bottom: parseFloat(style.borderBottomWidth) || 0,
            left: parseFloat(style.borderLeftWidth) || 0,
        };
    }

    function getPaddings(style) {
        return {
            top: parseFloat(style.paddingTop) || 0,
            right: parseFloat(style.paddingRight) || 0,
            bottom: parseFloat(style.paddingBottom) || 0,
            left: parseFloat(style.paddingLeft) || 0,
        };
    }

    function shrinkRect(rect, edges) {
        return {
            x: rect.x + edges.left,
            y: rect.y + edges.top,
            w: Math.max(0, rect.w - edges.left - edges.right),
            h: Math.max(0, rect.h - edges.top - edges.bottom),
        };
    }

    // Returns corner radii as [top-left, top-right, bottom-right, bottom-left] (circular only), or null if none
    function getRadii(style, rect) {
        function parseRadius(value) {
            const first = String(value).trim().split(/\s+/)[0];

            if (first.endsWith("%")) {
                return parseFloat(first) / 100 * Math.min(rect.w, rect.h);
            }

            return parseFloat(first) || 0;
        }

        const radii = [
            parseRadius(style.borderTopLeftRadius),
            parseRadius(style.borderTopRightRadius),
            parseRadius(style.borderBottomRightRadius),
            parseRadius(style.borderBottomLeftRadius),
        ];

        if (radii.every((r) => r <= 0)) {
            return null;
        }

        // Scale radii down if they overlap (the same way browsers do)
        const factor = Math.min(
            1,
            rect.w / Math.max(radii[0] + radii[1], 1e-6),
            rect.w / Math.max(radii[3] + radii[2], 1e-6),
            rect.h / Math.max(radii[0] + radii[3], 1e-6),
            rect.h / Math.max(radii[1] + radii[2], 1e-6),
        );

        return radii.map((r) => round(Math.max(0, r * factor)));
    }

    function insetRadii(radii, inset) {
        return (radii) ? radii.map((r) => round(Math.max(0, r - inset))) : null;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Items
    ////////////////////////////////////////////////////////////////////////////

    function pushItem(item, context) {
        if (items.length >= MAX_ITEMS) {
            throw new Error("Page is too complex");
        }

        item.clip = context.clip;
        item.opacity = round(context.opacity);
        items.push(item);

        return items.length - 1;
    }

    function rectData(rect) {
        return [round(rect.x), round(rect.y), round(rect.w), round(rect.h)];
    }

    function paintBackgroundColor(style, rect, context) {
        const color = parseColor(style.backgroundColor);

        if (color && isWorthPainting(rect, context)) {
            pushItem({ type: "rect", rect: rectData(rect), fill: color, radii: getRadii(style, rect) }, context);
        }
    }

    function paintBorders(style, rect, context) {
        const widths = getBorderWidths(style);
        const sides = ["top", "right", "bottom", "left"];
        const styles = {
            top: style.borderTopStyle,
            right: style.borderRightStyle,
            bottom: style.borderBottomStyle,
            left: style.borderLeftStyle,
        };
        const colors = {
            top: style.borderTopColor,
            right: style.borderRightColor,
            bottom: style.borderBottomColor,
            left: style.borderLeftColor,
        };

        const visibleSides = sides.filter((side) =>
            widths[side] > 0 && styles[side] !== "none" && styles[side] !== "hidden" && parseColor(colors[side])
        );

        if (visibleSides.length === 0 || !isWorthPainting(rect, context)) {
            return;
        }

        const isUniform = visibleSides.length === 4 && sides.every((side) =>
            widths[side] === widths.top && styles[side] === styles.top && colors[side] === colors.top
        );

        if (isUniform) {
            // One stroked shape, which also takes care of rounded corners
            const width = widths.top;
            const inset = width / 2;
            const strokeRect = { x: rect.x + inset, y: rect.y + inset, w: rect.w - width, h: rect.h - width };

            if (strokeRect.w > 0 && strokeRect.h > 0) {
                pushItem({
                    type: "border",
                    rect: rectData(strokeRect),
                    width: round(width),
                    color: parseColor(colors.top),
                    dash: (styles.top === "dashed" || styles.top === "dotted") ? styles.top : "solid",
                    radii: insetRadii(getRadii(style, rect), inset),
                }, context);
            }
        } else {
            // Each side as its own filled strip (corners are square)
            for (const side of visibleSides) {
                const width = widths[side];
                let strip;

                switch (side) {
                    case "top":    strip = { x: rect.x, y: rect.y, w: rect.w, h: width }; break;
                    case "right":  strip = { x: rect.x + rect.w - width, y: rect.y, w: width, h: rect.h }; break;
                    case "bottom": strip = { x: rect.x, y: rect.y + rect.h - width, w: rect.w, h: width }; break;
                    case "left":   strip = { x: rect.x, y: rect.y, w: width, h: rect.h }; break;
                }

                pushItem({ type: "rect", rect: rectData(strip), fill: parseColor(colors[side]), radii: null }, context);
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Text
    ////////////////////////////////////////////////////////////////////////////

    const metricsCanvas = document.createElement("canvas");
    const metricsContext = metricsCanvas.getContext("2d");
    const fontMetricsCache = new Map();

    function getCanvasFont(style) {
        return [style.fontStyle, style.fontWeight, style.fontSize, style.fontFamily].join(" ");
    }

    // Returns fraction of the text box's height that lies above the baseline
    function getAscentRatio(style) {
        const font = getCanvasFont(style);
        let ratio = fontMetricsCache.get(font);

        if (ratio === undefined) {
            metricsContext.font = font;
            const metrics = metricsContext.measureText("Hg");
            const ascent = metrics.fontBoundingBoxAscent;
            const descent = metrics.fontBoundingBoxDescent;

            ratio = (ascent > 0 && descent >= 0) ? ascent / (ascent + descent) : 0.8;
            fontMetricsCache.set(font, ratio);
        }

        return ratio;
    }

    function measureTextWidth(text, style) {
        metricsContext.font = getCanvasFont(style);

        return metricsContext.measureText(text).width;
    }

    function applyTextTransform(text, style) {
        switch (style.textTransform) {
            case "uppercase":
                return text.toUpperCase();

            case "lowercase":
                return text.toLowerCase();

            case "capitalize":
                return text.replace(/(^|[\s\-])(\S)/g, (match, prefix, letter) => prefix + letter.toUpperCase());

            default:
                return text;
        }
    }

    function getTextDecoration(style) {
        const line = style.textDecorationLine || style.textDecoration || "";
        const decorations = [];

        for (const decoration of ["underline", "line-through", "overline"]) {
            if (line.includes(decoration)) {
                decorations.push(decoration);
            }
        }

        return decorations.join(" ");
    }

    function pushTextLine(text, left, right, top, bottom, style, fill, context) {
        if (text.length === 0) {
            return;
        }

        const rect = { x: left + scrollX, y: top + scrollY, w: right - left, h: bottom - top };

        if (!isWorthPainting({ x: rect.x, y: rect.y, w: Math.max(rect.w, 1), h: Math.max(rect.h, 1) }, context)) {
            return;
        }

        pushItem({
            type: "text",
            x: round(rect.x),
            y: round(rect.y + rect.h * getAscentRatio(style)), // SVG text is positioned by its baseline
            width: round(rect.w),
            text: applyTextTransform(text, style),
            fontFamily: style.fontFamily,
            fontSize: round(parseFloat(style.fontSize) || 16),
            fontWeight: style.fontWeight,
            fontStyle: style.fontStyle,
            decoration: context.decoration,
            fill: fill,
        }, context);
    }

    function processTextNode(textNode, style, context) {
        const text = textNode.nodeValue;

        if (!text || style.visibility !== "visible") {
            return;
        }

        const fill = parseColor(style.webkitTextFillColor || style.color);

        if (!fill) {
            return;
        }

        const whiteSpace = style.whiteSpace;
        const collapsesSpaces = (whiteSpace === "normal" || whiteSpace === "nowrap" || whiteSpace === "pre-line");

        if (collapsesSpaces && !/\S/.test(text)) {
            return;
        }

        const range = document.createRange();

        // Fast path: text that fits on one line
        range.selectNodeContents(textNode);
        const nodeRects = range.getClientRects();

        if (nodeRects.length === 0) {
            return;
        }

        if (nodeRects.length === 1 && !text.includes("\n")) {
            const rect = nodeRects[0];

            if (rect.width > 0 && rect.height > 0) {
                if (collapsesSpaces) {
                    // Collapsed leading/trailing spaces may or may not take room,
                    // so measure from the first to the last visible character instead
                    range.setStart(textNode, text.search(/\S/));
                    range.setEnd(textNode, text.trimEnd().length);
                    const trimmedRect = range.getBoundingClientRect();

                    pushTextLine(text.replace(/\s+/g, " ").trim(), trimmedRect.left, trimmedRect.right,
                                 rect.top, rect.bottom, style, fill, context);
                } else {
                    pushTextLine(text.replace(/\t/g, "    "), rect.left, rect.right, rect.top, rect.bottom, style, fill, context);
                }
            }

            return;
        }

        // Text spanning multiple lines: measure it word by word (character by character for words that wrap),
        // and group pieces that ended up on the same line together
        const pieces = [];
        const wordPattern = /\S+/g;
        let match;

        while ((match = wordPattern.exec(text)) !== null) {
            const start = match.index;
            const end = start + match[0].length;

            range.setStart(textNode, start);
            range.setEnd(textNode, end);
            const rects = range.getClientRects();

            if (rects.length === 1) {
                pieces.push({ start: start, end: end, rect: rects[0] });
            } else if (rects.length > 1) {
                for (let i = start; i < end; i++) {
                    range.setStart(textNode, i);
                    range.setEnd(textNode, i + 1);
                    const charRect = range.getBoundingClientRect();

                    if (charRect.width > 0 || charRect.height > 0) {
                        pieces.push({ start: i, end: i + 1, rect: charRect });
                    }
                }
            }
        }

        let line = null;

        function flushLine() {
            if (line) {
                let content;

                if (collapsesSpaces) {
                    // Pieces are either whole words or single characters (of words broken across lines),
                    // only put a space in between those that had whitespace in between them to begin with
                    content = line.pieces.map((piece, index) => {
                        const gap = (index > 0) ? text.slice(line.pieces[index - 1].end, piece.start) : "";

                        return ((/\s/.test(gap)) ? " " : "") + text.slice(piece.start, piece.end);
                    }).join("");
                } else {
                    content = text.slice(line.pieces[0].start, line.pieces[line.pieces.length - 1].end).replace(/\t/g, "    ");
                }

                pushTextLine(content, line.left, line.right, line.top, line.bottom, style, fill, context);
                line = null;
            }
        }

        for (const piece of pieces) {
            const rect = piece.rect;
            const startsNewLine = !line ||
                rect.top >= line.bottom - (line.bottom - line.top) * 0.25 ||
                rect.left < line.right - 1 ||
                text.slice(line.pieces[line.pieces.length - 1].end, piece.start).includes("\n");

            if (startsNewLine) {
                flushLine();
                line = { pieces: [], left: rect.left, right: rect.right, top: rect.top, bottom: rect.bottom };
            }

            line.pieces.push(piece);
            line.right = Math.max(line.right, rect.right);
            line.top = Math.min(line.top, rect.top);
            line.bottom = Math.max(line.bottom, rect.bottom);
        }

        flushLine();
    }

    // Text inside of form controls doesn't exist as text nodes, it has to be painted separately
    function paintFormControlText(element, style, rect, context) {
        const tagName = element.localName;
        let lines = null;
        let fill = parseColor(style.color);
        let isCentered = false;

        if (tagName === "input") {
            const type = (element.type || "text").toLowerCase();

            if (["button", "submit", "reset"].includes(type)) {
                lines = [element.value || ""];
                isCentered = true;
            } else if (["text", "search", "email", "url", "tel", "number", "password"].includes(type)) {
                if (element.value) {
                    lines = [(type === "password") ? "•".repeat(element.value.length) : element.value];
                } else if (element.placeholder) {
                    lines = [element.placeholder];
                    fill = parseColor(getComputedStyle(element, "::placeholder").color) || fill;
                }
            }
        } else if (tagName === "textarea") {
            if (element.value) {
                lines = element.value.split("\n");
            } else if (element.placeholder) {
                lines = element.placeholder.split("\n");
                fill = parseColor(getComputedStyle(element, "::placeholder").color) || fill;
            }
        } else if (tagName === "select") {
            const option = element.options[element.selectedIndex];

            if (option) {
                lines = [option.text];
            }
        }

        if (!lines || !fill) {
            return;
        }

        const contentRect = shrinkRect(shrinkRect(rect, getBorderWidths(style)), getPaddings(style));

        if (contentRect.w <= 0 || contentRect.h <= 0) {
            return;
        }

        const clippedContext = { clip: addClip(context.clip, contentRect), opacity: context.opacity, decoration: "" };
        const fontSize = parseFloat(style.fontSize) || 16;
        const lineHeight = parseFloat(style.lineHeight) || fontSize * 1.2;
        const ascentRatio = getAscentRatio(style);
        // Single-line controls center their text vertically, multi-line ones start at the top
        const firstLineTop = (tagName === "textarea") ? contentRect.y : contentRect.y + (contentRect.h - lineHeight) / 2;

        lines.forEach((lineText, index) => {
            if (lineText.length === 0) {
                return;
            }

            const width = measureTextWidth(lineText, style);
            const left = (isCentered) ? contentRect.x + (contentRect.w - width) / 2 : contentRect.x;
            const textTop = firstLineTop + index * lineHeight + (lineHeight - fontSize * 1.2) / 2;

            pushItem({
                type: "text",
                x: round(left),
                y: round(textTop + fontSize * 1.2 * ascentRatio),
                width: round(width),
                text: lineText,
                fontFamily: style.fontFamily,
                fontSize: round(fontSize),
                fontWeight: style.fontWeight,
                fontStyle: style.fontStyle,
                decoration: "",
                fill: fill,
            }, clippedContext);
        });
    }

    ////////////////////////////////////////////////////////////////////////////
    // Images
    ////////////////////////////////////////////////////////////////////////////

    function getPreserveAspectRatio(style) {
        switch (style.objectFit) {
            case "contain":
            case "scale-down":
                return "xMidYMid meet";

            case "cover":
            case "none":
                return "xMidYMid slice";

            default: // "fill"
                return "none";
        }
    }

    // Draws something drawable onto a canvas and returns it as a data URL (or null if that's not allowed)
    function rasterize(source, sourceWidth, sourceHeight, displayWidth, displayHeight, preferJpeg) {
        const maxSize = MAX_IMAGE_SCALE * Math.max(devicePixelRatio, 1) * Math.max(displayWidth, displayHeight);
        const scale = Math.min(1, maxSize / Math.max(sourceWidth, sourceHeight, 1));
        const canvas = document.createElement("canvas");
        canvas.width = Math.max(1, Math.round(sourceWidth * scale));
        canvas.height = Math.max(1, Math.round(sourceHeight * scale));

        try {
            canvas.getContext("2d").drawImage(source, 0, 0, canvas.width, canvas.height);

            // Throws if the canvas got "tainted" by an image from a different origin
            return (preferJpeg) ? canvas.toDataURL("image/jpeg", 0.92) : canvas.toDataURL("image/png");
        } catch (error) {
            return null;
        }
    }

    function pushImage(href, rect, preserveAspectRatio, radii, context) {
        if (href && /^data:image\/(png|jpeg);base64,/.test(href)) {
            pushItem({ type: "image", rect: rectData(rect), href: href, preserveAspectRatio: preserveAspectRatio, radii: radii }, context);
        } else {
            skippedImageCount++;
        }
    }

    function paintImage(element, style, rect, context) {
        const contentRect = shrinkRect(shrinkRect(rect, getBorderWidths(style)), getPaddings(style));

        if (!isWorthPainting(contentRect, context)) {
            return;
        }

        const radii = getRadii(style, rect);

        if (element.localName === "img") {
            if (!element.complete || element.naturalWidth === 0) {
                return;
            }

            const source = element.currentSrc || element.src || "";
            const preferJpeg = /^data:image\/jpe?g|\.jpe?g($|[?#])/i.test(source);

            pushImage(rasterize(element, element.naturalWidth, element.naturalHeight, contentRect.w, contentRect.h, preferJpeg),
                      contentRect, getPreserveAspectRatio(style), radii, context);
        } else if (element.localName === "canvas") {
            pushImage(rasterize(element, element.width, element.height, contentRect.w, contentRect.h, false),
                      contentRect, "none", radii, context);
        } else if (element.localName === "video") {
            if (element.readyState >= 2 && element.videoWidth > 0) {
                const fit = getPreserveAspectRatio(style);

                pushImage(rasterize(element, element.videoWidth, element.videoHeight, contentRect.w, contentRect.h, true),
                          contentRect, (fit === "none") ? "xMidYMid meet" : fit, radii, context);
            }
        }
    }

    // Inline SVG gets rasterized (embedding page-provided SVG markup as-is would defeat the purpose of the
    // resulting file being harmless). Page styles don't apply to a standalone copy, so computed ones get inlined.
    const SVG_STYLE_PROPERTIES = [
        "fill", "fill-opacity", "fill-rule", "stroke", "stroke-width", "stroke-opacity", "stroke-linecap",
        "stroke-linejoin", "stroke-dasharray", "opacity", "color", "visibility", "display",
        "font-family", "font-size", "font-weight", "font-style", "text-anchor", "dominant-baseline",
        "stop-color", "stop-opacity",
    ];

    function paintInlineSvg(svgElement, rect, context) {
        if (!isWorthPainting(rect, context)) {
            return;
        }

        const clone = svgElement.cloneNode(true);
        const originals = [svgElement, ...svgElement.querySelectorAll("*")];
        const copies = [clone, ...clone.querySelectorAll("*")];

        originals.forEach((original, index) => {
            const computed = getComputedStyle(original);

            copies[index].setAttribute("style", SVG_STYLE_PROPERTIES
                .map((property) => property + ":" + computed.getPropertyValue(property))
                .join(";"));
        });

        // Resolve references to symbols defined elsewhere in the document (commonly used for icon sprites)
        for (const use of clone.querySelectorAll("use")) {
            const reference = use.getAttribute("href") || use.getAttributeNS("http://www.w3.org/1999/xlink", "href") || "";

            if (!reference.startsWith("#")) {
                continue;
            }

            const id = reference.slice(1);

            if ([...clone.querySelectorAll("[id]")].some((node) => node.id === id)) {
                continue; // Defined within this very SVG, nothing to resolve
            }

            const target = document.getElementById(id);

            if (target) {
                const replacement = document.createElementNS(SVG_NS, "svg");

                for (const attribute of ["x", "y", "width", "height", "style"]) {
                    if (use.hasAttribute(attribute)) {
                        replacement.setAttribute(attribute, use.getAttribute(attribute));
                    }
                }

                if (target.localName === "symbol") {
                    if (target.hasAttribute("viewBox")) {
                        replacement.setAttribute("viewBox", target.getAttribute("viewBox"));
                    }

                    for (const child of target.childNodes) {
                        replacement.appendChild(child.cloneNode(true));
                    }
                } else {
                    replacement.appendChild(target.cloneNode(true));
                }

                use.replaceWith(replacement);
            }
        }

        // Scripts and event handlers wouldn't run in an image anyway, but there's no reason to keep them around
        clone.querySelectorAll("script, foreignObject").forEach((node) => node.remove());

        clone.setAttribute("xmlns", SVG_NS);
        clone.setAttribute("width", rect.w);
        clone.setAttribute("height", rect.h);

        const markup = new XMLSerializer().serializeToString(clone);
        const url = URL.createObjectURL(new Blob([markup], { type: "image/svg+xml" }));
        const index = pushItem({ type: "image", rect: rectData(rect), href: null, preserveAspectRatio: "none", radii: null }, context);

        pendingTasks.push(loadImage(url).then((image) => {
            if (image) {
                const scale = MAX_IMAGE_SCALE * Math.max(devicePixelRatio, 1);
                items[index].href = rasterize(image, rect.w * scale, rect.h * scale, rect.w, rect.h, false);
            }

            URL.revokeObjectURL(url);
        }));
    }

    ////////////////////////////////////////////////////////////////////////////
    // Background images
    ////////////////////////////////////////////////////////////////////////////

    const MAX_BACKGROUND_CANVAS_AREA = 16 * 1024 * 1024;
    const imageLoadCache = new Map();

    // Splits a comma-separated CSS list, ignoring commas inside of parentheses
    function splitCssList(value) {
        const parts = [];
        let depth = 0;
        let current = "";

        for (const character of String(value)) {
            if (character === "(") {
                depth++;
            } else if (character === ")") {
                depth--;
            }

            if (character === "," && depth === 0) {
                parts.push(current.trim());
                current = "";
            } else {
                current += character;
            }
        }

        parts.push(current.trim());

        return parts;
    }

    function loadImage(url) {
        let promise = imageLoadCache.get(url);

        if (!promise) {
            // Event handlers and timers don't run when JavaScript is disabled for the page, promises do
            const image = new Image();
            image.src = url;
            promise = image.decode().then(() => image, () => null);
            imageLoadCache.set(url, promise);
        }

        return promise;
    }

    // Resolves a length such as "10px", "50%", or "calc(100% - 10px)" against the given size
    function resolveLength(value, size) {
        value = String(value).trim();

        let match = /^(-?[\d.]+)px$/.exec(value);
        if (match) {
            return parseFloat(match[1]);
        }

        match = /^(-?[\d.]+)%$/.exec(value);
        if (match) {
            return parseFloat(match[1]) / 100 * size;
        }

        match = /^calc\((-?[\d.]+)%\s*([+-])\s*(-?[\d.]+)px\)$/.exec(value);
        if (match) {
            const offset = parseFloat(match[3]) * ((match[2] === "-") ? -1 : 1);
            return parseFloat(match[1]) / 100 * size + offset;
        }

        return 0;
    }

    function computeBackgroundLayerSize(sizeValue, image, area) {
        const naturalWidth = image.naturalWidth || area.w;
        const naturalHeight = image.naturalHeight || area.h;

        if (sizeValue === "cover" || sizeValue === "contain") {
            const scales = [area.w / naturalWidth, area.h / naturalHeight];
            const scale = (sizeValue === "cover") ? Math.max(...scales) : Math.min(...scales);

            return { w: naturalWidth * scale, h: naturalHeight * scale };
        }

        const tokens = String(sizeValue).trim().split(/\s+/);
        const widthToken = tokens[0] || "auto";
        const heightToken = tokens[1] || "auto";
        let width = (widthToken === "auto") ? null : resolveLength(widthToken, area.w);
        let height = (heightToken === "auto") ? null : resolveLength(heightToken, area.h);

        if (width === null && height === null) {
            width = naturalWidth;
            height = naturalHeight;
        } else if (width === null) {
            width = height * naturalWidth / naturalHeight;
        } else if (height === null) {
            height = width * naturalHeight / naturalWidth;
        }

        return { w: width, h: height };
    }

    // CSS background images (often used for icons and sprites) get rasterized together into a single image,
    // which takes care of sizing, positioning, repeating, and clipping them all at once
    function paintBackgroundImages(style, rect, context) {
        const layers = splitCssList(style.backgroundImage);
        const urls = layers.map((layer) => {
            const match = /^url\(\s*(["']?)(.*)\1\s*\)$/.exec(layer);
            return (match) ? match[2] : null;
        });

        if (!urls.some((url) => url) || !isWorthPainting(rect, context)) {
            return;
        }

        const sizes = splitCssList(style.backgroundSize);
        const positionsX = splitCssList(style.backgroundPositionX);
        const positionsY = splitCssList(style.backgroundPositionY);
        const repeats = splitCssList(style.backgroundRepeat);
        const origins = splitCssList(style.backgroundOrigin);
        const clipAreas = splitCssList(style.backgroundClip);
        const borders = getBorderWidths(style);
        const paddings = getPaddings(style);
        const borderBox = { x: 0, y: 0, w: rect.w, h: rect.h };
        const paddingBox = shrinkRect(borderBox, borders);
        const contentBox = shrinkRect(paddingBox, paddings);
        const boxes = { "border-box": borderBox, "padding-box": paddingBox, "content-box": contentBox };

        const index = pushItem({ type: "image", rect: rectData(rect), href: null, preserveAspectRatio: "none", radii: getRadii(style, rect) }, context);

        pendingTasks.push(Promise.all(urls.map((url) => (url) ? loadImage(url) : null)).then((images) => {
            let scale = MAX_IMAGE_SCALE * Math.max(devicePixelRatio, 1);

            if (rect.w * rect.h * scale * scale > MAX_BACKGROUND_CANVAS_AREA) {
                scale = Math.sqrt(MAX_BACKGROUND_CANVAS_AREA / (rect.w * rect.h));
            }

            const canvas = document.createElement("canvas");
            canvas.width = Math.max(1, Math.round(rect.w * scale));
            canvas.height = Math.max(1, Math.round(rect.h * scale));
            const context2d = canvas.getContext("2d");
            context2d.scale(scale, scale);

            let hasPainted = false;

            // The first layer is the topmost one, so paint them in reverse order
            for (let i = layers.length - 1; i >= 0; i--) {
                const image = images[i];

                if (!image) {
                    continue;
                }

                const area = boxes[origins[i % origins.length]] || paddingBox;
                const clipArea = boxes[clipAreas[i % clipAreas.length]] || borderBox;
                const size = computeBackgroundLayerSize(sizes[i % sizes.length], image, area);

                if (!(size.w > 0 && size.h > 0)) {
                    continue;
                }

                const x = area.x + resolveLength(positionsX[i % positionsX.length], area.w - size.w);
                const y = area.y + resolveLength(positionsY[i % positionsY.length], area.h - size.h);
                const repeatTokens = repeats[i % repeats.length].split(/\s+/);
                const repeatX = (repeatTokens[0] === "repeat-x") || (repeatTokens[0] !== "repeat-y" && repeatTokens[0] !== "no-repeat");
                const repeatY = (repeatTokens[0] === "repeat-y") ||
                                (repeatTokens.length > 1 ? repeatTokens[1] !== "no-repeat" : (repeatTokens[0] !== "repeat-x" && repeatTokens[0] !== "no-repeat"));

                context2d.save();
                context2d.beginPath();
                context2d.rect(clipArea.x, clipArea.y, clipArea.w, clipArea.h);
                context2d.clip();

                if (!repeatX && !repeatY) {
                    context2d.drawImage(image, x, y, size.w, size.h);
                } else {
                    const pattern = context2d.createPattern(image, (repeatX && repeatY) ? "repeat" : (repeatX) ? "repeat-x" : "repeat-y");
                    const naturalWidth = image.naturalWidth || size.w;
                    const naturalHeight = image.naturalHeight || size.h;

                    pattern.setTransform(new DOMMatrix().translate(x, y).scale(size.w / naturalWidth, size.h / naturalHeight));
                    context2d.fillStyle = pattern;
                    context2d.fillRect(clipArea.x, clipArea.y, clipArea.w, clipArea.h);
                }

                context2d.restore();
                hasPainted = true;
            }

            if (hasPainted) {
                try {
                    items[index].href = canvas.toDataURL("image/png");
                } catch (error) {
                    // Image(s) from a different origin
                }
            }
        }));
    }

    ////////////////////////////////////////////////////////////////////////////
    // Walking the page
    ////////////////////////////////////////////////////////////////////////////

    const IGNORED_ELEMENTS = new Set(["head", "script", "style", "noscript", "template", "iframe", "object", "embed", "frame", "frameset"]);

    const documentElement = document.documentElement;
    const body = document.body;

    // The root element's background (or the body's, if the root has none) paints the entire canvas
    let canvasBackgroundElement = null;

    if (parseColor(getComputedStyle(documentElement).backgroundColor)) {
        canvasBackgroundElement = documentElement;
    } else if (body && parseColor(getComputedStyle(body).backgroundColor)) {
        canvasBackgroundElement = body;
    }

    function getChildNodes(node) {
        if (node.shadowRoot) {
            return node.shadowRoot.childNodes;
        }

        if (node.localName === "slot" && typeof node.assignedNodes === "function") {
            const assigned = node.assignedNodes({ flatten: true });

            if (assigned.length > 0) {
                return assigned;
            }
        }

        return node.childNodes;
    }

    function walkElement(element, context) {
        if (IGNORED_ELEMENTS.has(element.localName)) {
            return;
        }

        const style = getComputedStyle(element);

        if (style.display === "none") {
            return;
        }

        const opacity = context.opacity * (parseFloat(style.opacity) || 0);

        if (opacity <= 0) {
            return;
        }

        // Fixed elements escape all scrollable containers (and stay where they currently are on the screen)
        let clip = (style.position === "fixed") ? -1 : context.clip;
        const ownContext = { clip: clip, opacity: opacity };
        const isVisible = style.visibility === "visible";
        const hasBox = style.display !== "contents";
        const rect = (hasBox) ? toDocumentRect(element.getBoundingClientRect()) : null;

        // Text decorations (e.g. underline) propagate to descendants' text,
        // except for atomic inline-level, floating, and absolutely positioned ones
        const isolatesDecorations = style.display.startsWith("inline-") || style.float !== "none" ||
                                    style.position === "absolute" || style.position === "fixed";
        const ownDecoration = getTextDecoration(style);
        const inheritedDecoration = (isolatesDecorations) ? "" : context.decoration;
        const decoration = [...new Set((inheritedDecoration + " " + ownDecoration).split(" ").filter((d) => d))].join(" ");
        ownContext.decoration = decoration;

        if (hasBox && isVisible) {
            if (element !== canvasBackgroundElement) {
                paintBackgroundColor(style, rect, ownContext);
            }

            if (style.backgroundImage !== "none") {
                paintBackgroundImages(style, rect, ownContext);
            }

            if (element !== canvasBackgroundElement) {
                paintBorders(style, rect, ownContext);
            }
        }

        // Replaced elements
        if (element.namespaceURI === SVG_NS) {
            if (hasBox && isVisible && element.localName === "svg") {
                paintInlineSvg(element, rect, ownContext);
            }

            return; // Contents of SVG elements are taken care of by rasterizing them
        }

        if (hasBox && isVisible) {
            switch (element.localName) {
                case "img":
                case "canvas":
                case "video":
                    paintImage(element, style, rect, ownContext);
                    return;

                case "input":
                case "textarea":
                case "select":
                    paintFormControlText(element, style, rect, ownContext);
                    return;
            }
        }

        // Elements that don't let their contents overflow clip their descendants to their padding box
        if (hasBox && element !== documentElement && element !== body &&
            (style.overflowX !== "visible" || style.overflowY !== "visible")) {
            clip = addClip(clip, shrinkRect(rect, getBorderWidths(style)));

            if (clips[clip].rect.w <= 0 || clips[clip].rect.h <= 0) {
                return;
            }
        }

        const childContext = { clip: clip, opacity: opacity, decoration: decoration };

        for (const child of getChildNodes(element)) {
            if (child.nodeType === Node.ELEMENT_NODE) {
                walkElement(child, childContext);
            } else if (child.nodeType === Node.TEXT_NODE) {
                processTextNode(child, style, childContext);
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////

    (async () => {
        const width = Math.max(documentElement.scrollWidth, documentElement.clientWidth, body ? body.scrollWidth : 0);
        const height = Math.max(documentElement.scrollHeight, documentElement.clientHeight, body ? body.scrollHeight : 0);

        // The canvas background
        if (canvasBackgroundElement) {
            pushItem({
                type: "rect",
                rect: [0, 0, round(width), round(height)],
                fill: parseColor(getComputedStyle(canvasBackgroundElement).backgroundColor),
                radii: null,
            }, { clip: -1, opacity: 1 });
        }

        walkElement(documentElement, { clip: -1, opacity: 1, decoration: "" });

        await Promise.all(pendingTasks);

        const paintedItems = items.filter((item) => item.type !== "image" || item.href);

        return {
            version: 1,
            width: round(width),
            height: round(height),
            clips: clips.map((clip) => clip.data),
            items: paintedItems,
            skippedImages: skippedImageCount + (items.length - paintedItems.length),
        };
    })().then((result) => {
        state.result = JSON.stringify(result);
        state.done = true;
    }).catch((error) => {
        state.error = String((error && error.message) || error);
        state.done = true;
    });
})();
