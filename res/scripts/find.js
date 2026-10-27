// Liquid's find-in-page engine.
//
// Runs in Liquid's own JS world: the page can neither see nor interfere with it, and it works even if JavaScript
// is disabled for the page itself.
//
// Matching is smart-case (case-insensitive unless the query has capital letters) and smart-accents (ignores diacritics
// unless the query has some); whitespace is collapsed (so line breaks in the source don't matter), matches may span
// across inline elements (e.g. "<b>Liq</b>uid"), but not across blocks (e.g. paragraphs), and open shadow roots
// get searched too, as do the current values of text inputs (except for passwords). Matches that aren't rendered
// (e.g. hidden, collapsed) don't count.
//
// Matches get highlighted, and connected to the search box (Liquid's, floating above the page) by rays, all of which
// invert whatever's underneath them, like an X-ray: they're drawn in a layer of their own on top of the page,
// isolated in a closed shadow root (the only thing that gets added to the document, and removed once done).

(function () {
    if (window.__liquidFind) {
        return;
    }

    // Elements whose text is never shown as such (text inputs get searched separately, through their values)
    const SKIPPED_ELEMENTS = new Set([
        'SCRIPT', 'STYLE', 'NOSCRIPT', 'TEMPLATE', 'HEAD', 'TITLE', 'IFRAME', 'OBJECT', 'EMBED',
        'CANVAS', 'VIDEO', 'AUDIO', 'SELECT', 'OPTION', 'TEXTAREA', 'INPUT',
    ]);

    // Elements text doesn't flow across (assumed from their tag rather than from their computed style, which is costly)
    const BLOCK_ELEMENTS = new Set([
        'ADDRESS', 'ARTICLE', 'ASIDE', 'BLOCKQUOTE', 'BODY', 'BUTTON', 'CAPTION', 'DD', 'DETAILS', 'DIALOG', 'DIV', 'DL',
        'DT', 'FIELDSET', 'FIGCAPTION', 'FIGURE', 'FOOTER', 'FORM', 'H1', 'H2', 'H3', 'H4', 'H5', 'H6', 'HEADER', 'HGROUP',
        'HR', 'HTML', 'LI', 'LEGEND', 'MAIN', 'MENU', 'NAV', 'OL', 'P', 'PRE', 'SECTION', 'SUMMARY', 'TABLE', 'TBODY',
        'TD', 'TFOOT', 'TH', 'THEAD', 'TR', 'UL',
    ]);

    // Inputs that take text (passwords are never searched)
    const TEXT_INPUT_TYPES = new Set(['', 'text', 'search', 'email', 'url', 'tel', 'number']);

    const SEPARATOR = '\u0000'; // Between blocks (queries never contain it, so matches never cross blocks)
    const MAX_MATCHES = 5000;
    const MAX_REPORTED = 500; // Matches drawn (the ones nearest to the middle of the viewport)
    const MIN_RESCAN_INTERVAL = 250; // Milliseconds, for pages that keep changing all the time
    const MAX_RESCAN_SHARE = 0.2; // Pages that take long to search get searched less often (at most 20% of the time)
    const SAFETY_RESCAN_INTERVAL = 3000; // Milliseconds, in case some change has gone unnoticed
    const CURRENT_MATCH_POSITION = 0.3; // Going to a match puts it there (vertically), above the search box
    const DOT_OFFSET = 7; // CSS pixels between the edge of the search box and the dots rays come out of
    const BACKDROP_BLUR = 12; // CSS pixels, how much the search box blurs what's behind it
    const DIACRITICS = /\p{M}/gu;
    const WHITESPACE = /\s/;
    const SVG = 'http://www.w3.org/2000/svg';

    const state = {
        key: null, // Query, to tell whether the matches have to be found again
        fold: null,
        needle: '',
        entries: [], // Ranges of text matches, and text inputs, in document order
        current: null, // The current match (an object that stays the same while the match is there)
        reported: [], // Matches drawn last time, in that order
        drawn: null, // What's been drawn last time (for redrawing, e.g. upon hovering a dot)
        isDirty: true,
        lastScanTime: 0,
        lastScanDuration: 0,
    };

    // Any change to the document may add or remove matches
    const observer = new MutationObserver(function () {
        state.isDirty = true;
    });
    observer.observe(document, { subtree: true, childList: true, characterData: true });

    ///////////////
    // Matching  //
    ///////////////

    function getFolder(query) {
        const isCaseSensitive = query !== query.toLowerCase();
        const isAccentSensitive = query.normalize('NFD') !== query.normalize('NFD').replace(DIACRITICS, '');

        return function (text) {
            // Fast path for ASCII, which is most of what there is
            if (text.length === 1 && text.charCodeAt(0) < 128) {
                return isCaseSensitive ? text : text.toLowerCase();
            }

            let folded = text;
            if (!isAccentSensitive) {
                folded = folded.normalize('NFD').replace(DIACRITICS, '');
            }
            if (!isCaseSensitive) {
                folded = folded.toLowerCase();
            }

            return folded;
        };
    }

    // Folds the text, collapsing whitespace, calling emit(character, start, end) for every resulting character,
    // with where it comes from in the original text; returns whether it ended with a space
    function foldText(text, fold, isAfterSpace, emit) {
        for (let i = 0; i < text.length; i++) {
            let character = text[i];
            const code = text.charCodeAt(i);

            // Keep surrogate pairs together
            if (code >= 0xD800 && code <= 0xDBFF && i + 1 < text.length) {
                character = text.substr(i, 2);
            }

            if (WHITESPACE.test(character)) {
                if (!isAfterSpace) {
                    emit(' ', i, i + 1);
                    isAfterSpace = true;
                }
            } else {
                const folded = fold(character);
                for (let j = 0; j < folded.length; j++) {
                    emit(folded[j], i, i + character.length);
                }
                isAfterSpace = false;
            }

            i += character.length - 1;
        }

        return isAfterSpace;
    }

    function getParent(element) {
        return element.parentElement || (element.getRootNode() instanceof ShadowRoot ? element.getRootNode().host : null);
    }

    function getBlock(element, cache) {
        let block = cache.get(element);

        if (block === undefined) {
            block = element;
            while (block && !BLOCK_ELEMENTS.has(block.tagName)) {
                block = getParent(block);
            }
            cache.set(element, block);
        }

        return block;
    }

    function isTextInput(element) {
        if (element.tagName === 'TEXTAREA') {
            return true;
        }

        return element.tagName === 'INPUT' && TEXT_INPUT_TYPES.has((element.getAttribute('type') || '').toLowerCase());
    }

    // Puts all of the document's text into one string (folded, whitespace collapsed), remembering where each of its
    // characters came from, and where text inputs are
    function index(fold) {
        const textNodes = [];
        const nodeIndexes = [];
        const startOffsets = [];
        const endOffsets = [];
        const parts = [];
        const inputs = []; // [element, position]
        const blockCache = new Map();
        let lastBlock = null;
        let isAfterSpace = true;

        function separate() {
            parts.push(SEPARATOR);
            nodeIndexes.push(-1);
            startOffsets.push(0);
            endOffsets.push(0);
            isAfterSpace = true;
        }

        // Depth-first, in document order, into open shadow roots too
        const stack = [document.documentElement];
        while (stack.length > 0) {
            const node = stack.pop();

            if (node.nodeType === Node.TEXT_NODE) {
                if (node.data.length === 0 || node.parentElement === null) {
                    continue;
                }

                const block = getBlock(node.parentElement, blockCache);
                if (block !== lastBlock && parts.length > 0) {
                    separate();
                }
                lastBlock = block;

                const nodeIndex = textNodes.length;
                textNodes.push(node);
                isAfterSpace = foldText(node.data, fold, isAfterSpace, function (character, start, end) {
                    parts.push(character);
                    nodeIndexes.push(nodeIndex);
                    startOffsets.push(start);
                    endOffsets.push(end);
                });
            } else if (node.nodeType === Node.ELEMENT_NODE || node.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
                if (node.nodeType === Node.ELEMENT_NODE) {
                    if (node === layer.host) {
                        continue;
                    }

                    if (SKIPPED_ELEMENTS.has(node.tagName)) {
                        if (isTextInput(node)) {
                            inputs.push([node, parts.length]);
                        }
                        continue;
                    }

                    // Line breaks end lines, text doesn't flow across them
                    if (node.tagName === 'BR') {
                        separate();
                        continue;
                    }
                }

                const children = node.childNodes;
                for (let i = children.length - 1; i >= 0; i--) {
                    stack.push(children[i]);
                }

                // Shadow trees get shown in place of their hosts' children (roughly)
                if (node.shadowRoot) {
                    stack.push(node.shadowRoot);
                }
            }
        }

        return { text: parts.join(''), textNodes, nodeIndexes, startOffsets, endOffsets, inputs };
    }

    function getNeedle(query, fold) {
        return Array.from(query).map(fold).join('').replace(/\s+/g, ' ');
    }

    // Text matches (as ranges) and text inputs (whose values get searched every time, as they change), in document order
    function find(query, fold) {
        const needle = getNeedle(query, fold);
        const entries = [];

        if (needle.trim().length === 0) {
            return entries;
        }

        const haystack = index(fold);
        let inputIndex = 0;
        let count = 0;

        let position = haystack.text.indexOf(needle);
        while (position >= 0 && count < MAX_MATCHES) {
            while (inputIndex < haystack.inputs.length && haystack.inputs[inputIndex][1] <= position) {
                entries.push({ input: haystack.inputs[inputIndex++][0] });
            }

            const last = position + needle.length - 1;
            try {
                const range = document.createRange();
                range.setStart(haystack.textNodes[haystack.nodeIndexes[position]], haystack.startOffsets[position]);
                range.setEnd(haystack.textNodes[haystack.nodeIndexes[last]], haystack.endOffsets[last]);
                entries.push({ range });
                count++;
            } catch (e) {
                // The document has changed in the meantime
            }

            position = haystack.text.indexOf(needle, position + needle.length);
        }

        while (inputIndex < haystack.inputs.length) {
            entries.push({ input: haystack.inputs[inputIndex++][0] });
        }

        return entries;
    }

    // Matches within the current value of a text input (the same object stands for the same match every time)
    const inputMatches = new WeakMap();

    function findInInput(input, needle, fold) {
        const value = input.value;
        const matches = [];

        if (!value || needle.trim().length === 0) {
            return matches;
        }

        const parts = [];
        const starts = [];
        const ends = [];
        foldText(value, fold, true, function (character, start, end) {
            parts.push(character);
            starts.push(start);
            ends.push(end);
        });
        const text = parts.join('');

        let known = inputMatches.get(input);
        if (!known) {
            known = new Map();
            inputMatches.set(input, known);
        }

        let position = text.indexOf(needle);
        while (position >= 0) {
            const start = starts[position];
            const end = ends[position + needle.length - 1];
            const key = start + ':' + end + ':' + value.length;

            if (!known.has(key)) {
                known.set(key, { input, start, end });
            }
            matches.push(known.get(key));

            position = text.indexOf(needle, position + needle.length);
        }

        return matches;
    }

    // The text match among those just found that's the same as the given one (matches in text inputs stay the same)
    function remap(match, byStartNode) {
        if (!match || !match.range) {
            return match;
        }

        const range = match.range;
        for (const entry of byStartNode.get(range.startContainer) || []) {
            if (entry.range.startOffset === range.startOffset &&
                entry.range.endContainer === range.endContainer && entry.range.endOffset === range.endOffset) {
                return entry;
            }
        }

        return null;
    }

    /////////////////////////////
    // Where matches are shown //
    /////////////////////////////

    // Where the element's content can be seen (clipped by scrolled containers, but not by the viewport: what's outside
    // of it gets scrolled to), and whether it can be seen at all
    const UNCLIPPED = { hidden: false, left: -Infinity, top: -Infinity, right: Infinity, bottom: Infinity };

    function getClipRect(element, cache) {
        let clip = cache.get(element);

        if (clip === undefined) {
            const parent = getParent(element);
            clip = parent ? getClipRect(parent, cache) : UNCLIPPED;

            const style = getComputedStyle(element);

            // Fixed elements are positioned relative to the viewport, whatever they're in
            if (style.position === 'fixed') {
                clip = { hidden: clip.hidden, left: -Infinity, top: -Infinity, right: Infinity, bottom: Infinity };
            }

            // Visibility gets inherited, but can be overridden by descendants
            if (style.visibility !== 'visible' || clip.hidden) {
                clip = { hidden: style.visibility !== 'visible', left: clip.left, top: clip.top, right: clip.right, bottom: clip.bottom };
            }

            if (style.overflowX !== 'visible' || style.overflowY !== 'visible') {
                const rect = element.getBoundingClientRect();
                clip = {
                    hidden: clip.hidden,
                    left: style.overflowX !== 'visible' ? Math.max(clip.left, rect.left) : clip.left,
                    top: style.overflowY !== 'visible' ? Math.max(clip.top, rect.top) : clip.top,
                    right: style.overflowX !== 'visible' ? Math.min(clip.right, rect.right) : clip.right,
                    bottom: style.overflowY !== 'visible' ? Math.min(clip.bottom, rect.bottom) : clip.bottom,
                };
            }

            cache.set(element, clip);
        }

        return clip;
    }

    // Text inputs don't expose where their text is, so it's measured (using their font), and wrapped like they wrap it
    const measuringContext = document.createElement('canvas').getContext('2d');

    function wrapLines(value, maxWidth, measure) {
        const lines = [];
        let offset = 0;

        for (const hardLine of value.split('\n')) {
            let lineStart = offset;
            let x = 0;
            let position = offset;

            for (const token of hardLine.match(/\s+|\S+/g) || []) {
                const isSpace = WHITESPACE.test(token[0]);
                const tokenWidth = measure(token);

                if (!isSpace && x > 0 && x + tokenWidth > maxWidth) {
                    lines.push([lineStart, position]);
                    lineStart = position;
                    x = 0;
                }

                // Words too long for a line get broken anywhere
                if (!isSpace && tokenWidth > maxWidth) {
                    for (let i = 0; i < token.length; i++) {
                        const characterWidth = measure(token[i]);
                        if (x > 0 && x + characterWidth > maxWidth) {
                            lines.push([lineStart, position + i]);
                            lineStart = position + i;
                            x = 0;
                        }
                        x += characterWidth;
                    }
                } else {
                    x += tokenWidth;
                }

                position += token.length;
            }

            lines.push([lineStart, offset + hardLine.length]);
            offset += hardLine.length + 1;
        }

        return lines;
    }

    function getInputMatchRects(match) {
        const input = match.input;
        const rect = input.getBoundingClientRect();
        if (rect.width === 0 || rect.height === 0) {
            return [];
        }

        const style = getComputedStyle(input);
        const px = function (value) { return parseFloat(value) || 0; };
        measuringContext.font = [style.fontStyle, style.fontWeight, style.fontSize, style.fontFamily].join(' ');
        const measure = function (text) { return measuringContext.measureText(text).width; };

        const left = rect.left + px(style.borderLeftWidth) + px(style.paddingLeft);
        const top = rect.top + px(style.borderTopWidth) + px(style.paddingTop);
        const contentWidth = input.clientWidth - px(style.paddingLeft) - px(style.paddingRight);
        const contentHeight = input.clientHeight - px(style.paddingTop) - px(style.paddingBottom);
        const lineHeight = style.lineHeight === 'normal' ? px(style.fontSize) * 1.2 : px(style.lineHeight);
        const value = input.value;
        const rects = [];

        if (input.tagName === 'INPUT') {
            let x = left - input.scrollLeft;
            const textWidth = measure(value);
            if (textWidth < contentWidth) {
                if (style.textAlign === 'center') {
                    x += (contentWidth - textWidth) / 2;
                } else if (style.textAlign === 'right' || (style.textAlign === 'end' && style.direction === 'ltr')) {
                    x += contentWidth - textWidth;
                }
            }

            rects.push([x + measure(value.slice(0, match.start)), top + (contentHeight - lineHeight) / 2,
                        measure(value.slice(match.start, match.end)), lineHeight]);
        } else {
            const lines = wrapLines(value, contentWidth, measure);
            for (let i = 0; i < lines.length; i++) {
                const start = Math.max(match.start, lines[i][0]);
                const end = Math.min(match.end, lines[i][1]);

                if (start < end) {
                    rects.push([left - input.scrollLeft + measure(value.slice(lines[i][0], start)),
                                top - input.scrollTop + i * lineHeight,
                                measure(value.slice(start, end)), lineHeight]);
                }
            }
        }

        // Text scrolled out of view within the input itself is clipped by it
        const contentRight = left + contentWidth;
        const contentBottom = top + contentHeight;
        return rects.map(function (r) {
            const l = Math.max(r[0], left);
            const t = Math.max(r[1], top);
            const ri = Math.min(r[0] + r[2], contentRight);
            const b = Math.min(r[1] + r[3], contentBottom);
            return { left: l, top: t, right: Math.max(l, ri), bottom: Math.max(t, b), width: Math.max(0, ri - l), height: Math.max(0, b - t) };
        });
    }

    function getElement(match) {
        return match.range ? match.range.startContainer.parentElement : match.input;
    }

    function getClientRects(match) {
        return match.range ? match.range.getClientRects() : getInputMatchRects(match);
    }

    function getBounds(match) {
        let bounds = null;

        for (const rect of getClientRects(match)) {
            if (rect.width > 0 && rect.height > 0) {
                bounds = bounds ? {
                    left: Math.min(bounds.left, rect.left),
                    top: Math.min(bounds.top, rect.top),
                    right: Math.max(bounds.right, rect.right),
                    bottom: Math.max(bounds.bottom, rect.bottom),
                } : { left: rect.left, top: rect.top, right: rect.right, bottom: rect.bottom };
            }
        }

        return bounds;
    }

    function intersects(a, b) {
        return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
    }

    function getRem() {
        return parseFloat(getComputedStyle(document.documentElement).fontSize) || 16;
    }

    // Scrolls the match into view (within whatever scrolls it, and the page itself, in both directions): either to where
    // the current match goes, or just enough for it to be in view, with some room around it; never behind the search box
    function scrollIntoView(match, isGoingToCurrentPosition, searchBox) {
        const element = getElement(match);
        if (!element) {
            return;
        }

        // Where it is before anything gets scrolled (bringing it into view within its containers below may well scroll
        // the page too, just enough for it to be at its very edge)
        const before = getBounds(match);
        const wasOutOfViewVertically = !before || before.bottom <= 0 || before.top >= window.innerHeight;
        const wasOutOfViewHorizontally = !before || before.right <= 0 || before.left >= window.innerWidth;

        element.scrollIntoView({ block: 'nearest', inline: 'nearest' });

        // Within a text input that scrolls (e.g. a long text area)
        if (match.input && match.input.tagName === 'TEXTAREA') {
            const inputRect = match.input.getBoundingClientRect();
            const bounds = getBounds(match);
            const lines = (match.input.value.slice(0, match.start).match(/\n/g) || []).length;
            if (bounds === null || bounds.top < inputRect.top || bounds.bottom > inputRect.bottom) {
                const lineHeight = parseFloat(getComputedStyle(match.input).lineHeight) || 20;
                match.input.scrollTop = Math.max(0, lines * lineHeight - match.input.clientHeight / 2);
            }
        }

        let rect = getBounds(match);
        if (rect === null) {
            return;
        }

        const margin = 2 * getRem();

        function getScroll(from, to, size, position, wasOutOfView) {
            // Matches that were out of view go to where the current match goes
            if (isGoingToCurrentPosition && wasOutOfView) {
                return (from + to) / 2 - size * position;
            }

            // Otherwise, just enough for them to be in view, with some room around them
            if (from < margin) {
                return from - margin;
            }
            if (to > size - margin) {
                return Math.min(to - (size - margin), from - margin); // The start of long matches stays in view
            }
            return 0;
        }

        // Matches go above the search box (which is in the middle)
        window.scrollBy(getScroll(rect.left, rect.right, window.innerWidth, 0.5, wasOutOfViewHorizontally),
                        getScroll(rect.top, rect.bottom, window.innerHeight, CURRENT_MATCH_POSITION, wasOutOfViewVertically));

        // Never behind the search box: right above it, or right below it if there's no room above
        rect = getBounds(match);
        if (searchBox && rect && intersects(rect, { left: searchBox.left - 8, top: searchBox.top - 8, right: searchBox.right + 8, bottom: searchBox.bottom + 8 })) {
            let top = searchBox.top - margin - (rect.bottom - rect.top);
            if (top < margin) {
                top = searchBox.bottom + margin;
            }
            window.scrollBy(0, rect.top - top);
        }
    }

    ///////////////////////
    // Drawing (X-ray)  //
    ///////////////////////

    // All white, blended with what's underneath using "difference": everything drawn inverts the page beneath it;
    // the search box also blurs what's behind it (in an element of its own, under the layer: anything in the layer
    // would only get blended with what's in it, rather than with the page)
    const layer = { host: null, backdrop: null };

    function getLayer() {
        if (layer.host && layer.host.isConnected && layer.backdrop && layer.backdrop.isConnected) {
            return layer;
        }
        removeLayer();

        const backdrop = document.createElement('liquid-find-backdrop');
        backdrop.setAttribute('style', [
            'all: initial', 'display: block', 'position: fixed', 'z-index: 2147483646', 'pointer-events: none',
            'backdrop-filter: blur(' + BACKDROP_BLUR + 'px)', '-webkit-backdrop-filter: blur(' + BACKDROP_BLUR + 'px)',
        ].map(function (rule) { return rule + ' !important;'; }).join(''));
        layer.backdrop = backdrop;
        document.documentElement.appendChild(backdrop);

        const host = document.createElement('liquid-find');
        host.setAttribute('style', [
            'all: initial', 'display: block', 'position: fixed', 'inset: 0', 'z-index: 2147483647', 'pointer-events: none',
            'mix-blend-mode: difference', 'contain: strict', 'overflow: hidden',
        ].map(function (rule) { return rule + ' !important;'; }).join(''));

        const root = host.attachShadow({ mode: 'closed' });
        const svg = document.createElementNS(SVG, 'svg');
        svg.setAttribute('width', '100%');
        svg.setAttribute('height', '100%');
        svg.setAttribute('style', 'position: absolute; inset: 0; display: block;');
        root.appendChild(svg);

        function add(tag, attributes) {
            const element = document.createElementNS(SVG, tag);
            for (const name in attributes) {
                element.setAttribute(name, attributes[name]);
            }
            svg.appendChild(element);
            return element;
        }

        // Back to front
        layer.searchBox = add('rect', { fill: '#fff' });
        layer.rays = add('path', { fill: 'none', stroke: '#fff', 'stroke-width': 1.5, 'stroke-linecap': 'round' });
        layer.highlights = add('path', { fill: '#fff' });
        layer.dots = add('path', { fill: '#fff' });
        layer.currentRay = add('path', { fill: 'none', stroke: '#fff', 'stroke-width': 2.5, 'stroke-linecap': 'round' });
        layer.currentHighlight = add('path', { fill: '#fff', stroke: '#fff', 'stroke-width': 2 });
        layer.currentRing = add('path', { fill: 'none', stroke: '#fff', 'stroke-width': 2 });
        layer.currentDot = add('path', { fill: '#fff' });
        // Hovered dots, their rays and matches stand out in color (inverted too, but tinted)
        layer.hoveredRay = add('path', { fill: 'none', stroke: '#0af', 'stroke-width': 3, 'stroke-linecap': 'round' });
        layer.hoveredHighlight = add('path', { fill: '#0af' });
        layer.hoveredDot = add('path', { fill: '#0af' });

        layer.host = host;
        document.documentElement.appendChild(host);

        return layer;
    }

    function removeLayer() {
        for (const name of ['host', 'backdrop']) {
            if (layer[name]) {
                layer[name].remove();
                layer[name] = null;
            }
        }
    }

    function rectPath(rect, grow) {
        const x = rect.left - grow;
        const y = rect.top - grow;
        return 'M' + x + ' ' + y + 'h' + (rect.right - rect.left + 2 * grow) + 'v' + (rect.bottom - rect.top + 2 * grow) +
               'h' + (-(rect.right - rect.left + 2 * grow)) + 'Z';
    }

    function circlePath(x, y, radius) {
        return 'M' + (x - radius) + ' ' + y + 'a' + radius + ' ' + radius + ' 0 1 0 ' + (2 * radius) + ' 0' +
               'a' + radius + ' ' + radius + ' 0 1 0 ' + (-2 * radius) + ' 0';
    }

    // Where the line from the middle of the search box (a pill: a rectangle with round ends) towards the point
    // crosses its edge: rays come out of the search box in the direction of their matches
    function getPointOnSearchBoxTowards(searchBox, x, y) {
        const radius = (searchBox.bottom - searchBox.top) / 2;
        const centerX = (searchBox.left + searchBox.right) / 2;
        const centerY = (searchBox.top + searchBox.bottom) / 2;
        const length = Math.hypot(x - centerX, y - centerY);

        if (length < 0.001) {
            return [centerX, centerY, 0, 0];
        }

        const dx = (x - centerX) / length;
        const dy = (y - centerY) / length;
        const distanceFromSpine = function (px, py) {
            const sx = Math.max(searchBox.left + radius, Math.min(px, searchBox.right - radius));
            return Math.hypot(px - sx, py - centerY);
        };

        let inside = 0;
        let outside = searchBox.right - searchBox.left + radius;
        for (let i = 0; i < 24; i++) {
            const middle = (inside + outside) / 2;
            if (distanceFromSpine(centerX + dx * middle, centerY + dy * middle) < radius) {
                inside = middle;
            } else {
                outside = middle;
            }
        }

        return [centerX + dx * outside, centerY + dy * outside, dx, dy];
    }

    function draw(hoveredIndex) {
        const drawn = state.drawn;
        if (!drawn) {
            return;
        }

        const target = getLayer();
        const searchBox = drawn.searchBox;
        const paths = {
            rays: [], highlights: [], dots: [],
            currentRay: [], currentHighlight: [], currentRing: [], currentDot: [],
            hoveredRay: [], hoveredHighlight: [], hoveredDot: [],
        };

        if (searchBox) {
            const height = searchBox.bottom - searchBox.top;
            target.searchBox.setAttribute('x', searchBox.left);
            target.searchBox.setAttribute('y', searchBox.top);
            target.searchBox.setAttribute('width', searchBox.right - searchBox.left);
            target.searchBox.setAttribute('height', height);
            target.searchBox.setAttribute('rx', height / 2);

            const style = target.backdrop.style;
            style.setProperty('left', searchBox.left + 'px', 'important');
            style.setProperty('top', searchBox.top + 'px', 'important');
            style.setProperty('width', (searchBox.right - searchBox.left) + 'px', 'important');
            style.setProperty('height', height + 'px', 'important');
            style.setProperty('border-radius', (height / 2) + 'px', 'important');
        }

        for (let i = 0; i < drawn.matches.length; i++) {
            const match = drawn.matches[i];
            const kind = (i === hoveredIndex) ? 'hovered' : (match.isCurrent) ? 'current' : '';

            for (const rect of match.visibleRects) {
                paths[kind ? kind + 'Highlight' : 'highlights'].push(rectPath(rect, kind ? 2 : 1));
                if (kind === 'current') {
                    paths.currentRing.push(rectPath(rect, 5));
                }
            }

            if (match.dot) {
                const radius = (kind === 'hovered') ? 6 : (kind === 'current') ? 5 : 3.5;
                paths[kind ? kind + 'Ray' : 'rays'].push('M' + match.dot[0] + ' ' + match.dot[1] + 'L' + match.rayEnd[0] + ' ' + match.rayEnd[1]);
                paths[kind ? kind + 'Dot' : 'dots'].push(circlePath(match.dot[0], match.dot[1], radius));
            }
        }

        for (const name in paths) {
            target[name].setAttribute('d', paths[name].join(''));
        }
    }

    // Whether what the page shows at that point is light (the search box, drawn inverted there, is then dark):
    // judged by the first background color that isn't see-through, from there up to the root (images aren't considered)
    function isLightAt(x, y) {
        let element = document.elementFromPoint(x, y); // Liquid's own layer doesn't take the mouse, so it's never what's there

        while (element) {
            const color = getComputedStyle(element).backgroundColor.match(/[\d.]+/g);
            if (color && color.length >= 3 && (color.length < 4 || parseFloat(color[3]) > 0.5)) {
                return (0.2126 * color[0] + 0.7152 * color[1] + 0.0722 * color[2]) / 255 > 0.5;
            }
            element = getParent(element);
        }

        return true; // Pages are white unless they say otherwise
    }

    // A match as it is now: [match, visible rects, bounds], or null if it's not rendered
    function measure(match, clipCache) {
        const element = getElement(match);
        if (!element || !element.isConnected) {
            return null;
        }

        const clip = getClipRect(element, clipCache);
        if (clip.hidden) {
            return null;
        }

        const visibleRects = [];
        let bounds = null;
        for (const rect of getClientRects(match)) {
            if (rect.width === 0 || rect.height === 0) {
                continue;
            }

            bounds = bounds ? {
                left: Math.min(bounds.left, rect.left),
                top: Math.min(bounds.top, rect.top),
                right: Math.max(bounds.right, rect.right),
                bottom: Math.max(bounds.bottom, rect.bottom),
            } : { left: rect.left, top: rect.top, right: rect.right, bottom: rect.bottom };

            // Clipped out of view within a scrolled container
            const visible = {
                left: Math.max(rect.left, clip.left),
                top: Math.max(rect.top, clip.top),
                right: Math.min(rect.right, clip.right),
                bottom: Math.min(rect.bottom, clip.bottom),
            };
            if (visible.right > visible.left && visible.bottom > visible.top) {
                visibleRects.push(visible);
            }
        }

        return (bounds !== null) ? [match, visibleRects, bounds] : null;
    }

    // What gets drawn: rays go from dots just off the edge of the search box (in the direction of their matches),
    // to the point of their matches closest to the middle of the search box; returns where the dots are
    function layOut(searchBox, items, current) {
        const drawn = { searchBox, matches: [] };
        const dots = [];

        for (const item of items) {
            const bounds = item[2];
            const match = { isCurrent: item[0] === current, visibleRects: item[1], dot: null, rayEnd: null };

            if (searchBox) {
                const centerX = (searchBox.left + searchBox.right) / 2;
                const centerY = (searchBox.top + searchBox.bottom) / 2;
                const endX = Math.max(bounds.left, Math.min(centerX, bounds.right));
                const endY = Math.max(bounds.top, Math.min(centerY, bounds.bottom));
                const isUnderSearchBox = endX >= searchBox.left && endX <= searchBox.right && endY >= searchBox.top && endY <= searchBox.bottom;

                if (!isUnderSearchBox) {
                    const edge = getPointOnSearchBoxTowards(searchBox, endX, endY);
                    match.dot = [edge[0] + edge[2] * DOT_OFFSET, edge[1] + edge[3] * DOT_OFFSET];
                    match.rayEnd = [endX, endY];
                }
            }

            drawn.matches.push(match);
            dots.push(match.dot);
        }

        state.drawn = drawn;

        return dots;
    }

    // Liquid's search box stays in the middle of the viewport (as it gets resized, before Liquid says where it is now)
    function getSearchBox() {
        const base = state.searchBoxBase;
        if (!base) {
            return null;
        }

        const dx = (window.innerWidth - base.viewportWidth) / 2;
        const dy = (window.innerHeight - base.viewportHeight) / 2;
        return { left: base.left + dx, top: base.top + dy, right: base.right + dx, bottom: base.bottom + dy };
    }

    // Upon scrolling (the page, or anything within it) and resizing, what's drawn follows right away, every frame,
    // without looking for matches again; only works if JavaScript is enabled for the page (otherwise, events don't
    // reach this world, and Liquid's own updates are all there is)
    let isRedrawRequested = false;

    function redraw() {
        isRedrawRequested = false;

        if (!state.drawn || !layer.host) {
            return;
        }

        const clipCache = new Map();
        const items = [];
        for (const match of state.reported) {
            const item = match ? measure(match, clipCache) : null;
            if (item !== null) {
                items.push(item);
            }
        }

        // Reported matches that are gone would make dots (whose order Liquid knows) point at the wrong ones
        if (items.length !== state.reported.length) {
            return;
        }

        layOut(getSearchBox(), items, state.reportedCurrent);
        draw(state.hoveredIndex);
    }

    function requestRedraw() {
        if (!isRedrawRequested && state.drawn) {
            isRedrawRequested = true;
            requestAnimationFrame(redraw);
        }
    }

    window.addEventListener('scroll', requestRedraw, { capture: true, passive: true });
    window.addEventListener('resize', requestRedraw, { passive: true });

    ////////////
    // Update //
    ////////////

    // move: 0 (stay), 1 (next), -1 (previous); goTo: index of a match drawn last time (whose dot has been clicked), or -1;
    // searchBox: [x, y, width, height] of Liquid's search box, in CSS pixels relative to the viewport
    function update(query, move, goTo, searchBoxRect, hoveredIndex) {
        const now = Date.now();
        const isNewQuery = query !== state.key;
        state.searchBoxBase = searchBoxRect ? {
            left: searchBoxRect[0], top: searchBoxRect[1],
            right: searchBoxRect[0] + searchBoxRect[2], bottom: searchBoxRect[1] + searchBoxRect[3],
            viewportWidth: window.innerWidth, viewportHeight: window.innerHeight,
        } : null;
        const searchBox = getSearchBox();

        const rescanInterval = Math.max(MIN_RESCAN_INTERVAL, state.lastScanDuration / MAX_RESCAN_SHARE);
        if (isNewQuery ||
            (state.isDirty && now - state.lastScanTime >= rescanInterval) ||
            now - state.lastScanTime >= Math.max(SAFETY_RESCAN_INTERVAL, rescanInterval)) {
            state.fold = getFolder(query);
            state.needle = getNeedle(query, state.fold);
            state.entries = find(query, state.fold);

            // The same matches, found again, are new objects: the current one (and those drawn last time,
            // whose dots may be clicked) have to remain the same
            if (!isNewQuery) {
                const byStartNode = new Map();
                for (const entry of state.entries) {
                    if (entry.range) {
                        const list = byStartNode.get(entry.range.startContainer) || [];
                        list.push(entry);
                        byStartNode.set(entry.range.startContainer, list);
                    }
                }
                const remapOne = function (match) { return remap(match, byStartNode); };
                state.current = remapOne(state.current);
                state.reported = state.reported.map(remapOne);
            }
            state.key = query;
            state.isDirty = false;
            state.lastScanTime = now;
            state.lastScanDuration = Date.now() - now;
        }

        // Matches as they are now: text inputs' values change all the time, and everything moves around
        // (the page scrolls, things change, etc.); matches that get rendered at all count, the parts of them that can be
        // seen get highlighted, and wherever they are (even if scrolled out of view), rays point at them
        const clipCache = new Map();
        const shown = []; // [match, visible rects, bounds]
        for (const entry of state.entries) {
            const matches = entry.range ? [entry] : findInInput(entry.input, state.needle, state.fold);

            for (const match of matches) {
                const item = measure(match, clipCache);
                if (item !== null) {
                    shown.push(item);
                }
            }
        }

        // In view, and not hidden behind the search box
        function isInView(item) {
            return item[1].some(function (rect) {
                const isInViewport = rect.right > 0 && rect.left < window.innerWidth && rect.bottom > 0 && rect.top < window.innerHeight;
                const isUnderSearchBox = searchBox !== null && rect.left >= searchBox.left && rect.right <= searchBox.right &&
                                         rect.top >= searchBox.top && rect.bottom <= searchBox.bottom;
                return isInViewport && !isUnderSearchBox;
            });
        }

        // Which one's current
        let currentIndex = shown.findIndex(function (item) { return item[0] === state.current; });
        let isGoingTo = false;
        if (goTo >= 0 && goTo < state.reported.length) {
            const index = shown.findIndex(function (item) { return item[0] === state.reported[goTo]; });
            if (index >= 0) {
                currentIndex = index;
                isGoingTo = true;
            }
        } else if (isNewQuery || currentIndex < 0) {
            // The first one in view (or below it)
            currentIndex = shown.findIndex(function (item) { return isInView(item) || item[2].top >= window.innerHeight; });
            if (currentIndex < 0) {
                currentIndex = 0;
            }
        }
        if (shown.length > 0 && move !== 0) {
            currentIndex = (currentIndex + move + shown.length) % shown.length;
        }
        const current = shown.length > 0 ? shown[currentIndex] : null;
        state.current = current ? current[0] : null;

        // Go to the current match: whenever its dot gets clicked (just enough for it to be in view),
        // or when it's out of view once found, or upon going to the next or previous one
        if (current && (isGoingTo || ((move !== 0 || isNewQuery) && !isInView(current)))) {
            scrollIntoView(current[0], !isGoingTo, searchBox);
            return update(query, 0, -1, searchBoxRect, hoveredIndex);
        }

        // Pages may have lots of matches: the nearest ones (and the current one) get drawn, the rest gets counted
        let reported = shown.map(function (item, index) { return index; });
        if (reported.length > MAX_REPORTED) {
            const centerX = window.innerWidth / 2;
            const centerY = window.innerHeight / 2;
            const distance = function (index) {
                const bounds = shown[index][2];
                const dx = Math.max(bounds.left - centerX, 0, centerX - bounds.right);
                const dy = Math.max(bounds.top - centerY, 0, centerY - bounds.bottom);
                return dx * dx + dy * dy;
            };
            reported.sort(function (a, b) { return distance(a) - distance(b); });
            reported = reported.slice(0, MAX_REPORTED);
            if (current && !reported.includes(currentIndex)) {
                reported[reported.length - 1] = currentIndex;
            }
            reported.sort(function (a, b) { return a - b; });
        }
        state.reported = reported.map(function (index) { return shown[index][0]; });

        state.reportedCurrent = current ? current[0] : null;
        state.hoveredIndex = hoveredIndex;
        const dots = layOut(searchBox, reported.map(function (index) { return shown[index]; }), state.reportedCurrent);
        draw(hoveredIndex);

        // For the text of the search box to be told apart from the inverted page underneath
        const isSearchBoxDark = searchBox ? isLightAt((searchBox.left + searchBox.right) / 2, (searchBox.top + searchBox.bottom) / 2) : true;

        return JSON.stringify({ total: shown.length, current: current ? currentIndex + 1 : 0, dots, isSearchBoxDark });
    }

    window.__liquidFind = {
        update,
        // Hovering a dot only changes how things are drawn
        hover: function (index) {
            state.hoveredIndex = index;
            draw(index);
        },
        // Snapshots are of the page, not of the search
        setHidden: function (isHidden) {
            for (const element of [layer.host, layer.backdrop]) {
                if (element) {
                    element.style.setProperty('visibility', isHidden ? 'hidden' : 'visible', 'important');
                }
            }
        },
        close: function () {
            removeLayer();
            state.drawn = null;
            state.current = null;
            state.key = null;
        },
    };
})();
