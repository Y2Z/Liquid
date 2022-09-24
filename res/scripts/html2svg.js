// html2svg.js, the engine behind Liquid's vector screenshot functionality

const snapshot = {
    width: %1,
    height: %2,
    backgroundColor: "%3",
    fullPage: %4,
};

// const irrelevantNodeNames = [
//         "STYLE",
//         "SCRIPT",
//     ];

const svgDoc = document.implementation.createDocument("http://www.w3.org/2000/svg", "svg", null);

const svgNode = svgDoc.rootElement;
svgNode.setAttribute("version", "1.1");
svgNode.setAttribute("width", snapshot.width);
svgNode.setAttribute("height", snapshot.height);

{
    const svgRectNode = document.createElementNS(null, "rect");
    svgRectNode.setAttribute("width", snapshot.width);
    svgRectNode.setAttribute("height", snapshot.height);
    svgRectNode.setAttribute("fill", snapshot.backgroundColor);
    svgNode.appendChild(svgRectNode);
}

function htmlElementPosition(htmlElement) {
    var rect = htmlElement.getBoundingClientRect(),
        scrollLeft = (snapshot.fullPage) ? 0 : (window.scrollX || document.documentElement.scrollLeft),
        scrollTop = (snapshot.fullPage) ? 0 : (window.scrollY || document.documentElement.scrollTop);

    return {
        left: rect.left + scrollLeft,
        top: rect.top + scrollTop,
        width: rect.width,
        height: rect.height,
    };
}

function processElement(htmlElement) {
    const style = window.getComputedStyle(htmlElement);

    if (style.visibility != "hidden" && style.display != "none" && style.opacity > 0) {
        const parameters = htmlElementPosition(htmlElement);

        if (parameters.width > 0 && parameters.height > 0) {
            const svgRectNode = document.createElementNS(null, "rect");
            svgRectNode.setAttribute("x", parameters.left);
            svgRectNode.setAttribute("y", parameters.top);
            svgRectNode.setAttribute("width", parameters.width);
            svgRectNode.setAttribute("height", parameters.height) ;
            if (style.backgroundColor != "rgba(0, 0, 0, 0)") {
                svgRectNode.setAttribute("fill", style.backgroundColor);
            } else {
                svgRectNode.setAttribute("fill", "none");
            }
            svgNode.appendChild(svgRectNode);
        }
    }
}

function processTextNode(htmlTextNode) {
    const text = htmlTextNode.nodeValue;

    if (text.trim().length > 0) {
        const htmlParentElement = htmlTextNode.parentElement;
        const style = window.getComputedStyle(htmlParentElement);
        const parameters = htmlElementPosition(htmlParentElement);

        const svgTextNode = document.createElementNS(null, "text");
        svgTextNode.setAttribute("x", parameters.left);
        svgTextNode.setAttribute("y", parameters.top);
        svgTextNode.setAttribute("color", style.color);

        var textNode = document.createTextNode(text);
        svgTextNode.appendChild(textNode);

        svgNode.appendChild(svgTextNode);
    }
}

function walkDom(htmlNode) {
    if (htmlNode.nodeType == Node.ELEMENT_NODE) {
        processElement(htmlNode);

        for (let childNode of htmlNode.childNodes) {
          walkDom(childNode);
        }
    } else if (htmlNode.nodeType == Node.TEXT_NODE) {
        processTextNode(htmlNode);
    }
}

//////////////////////////////////////////////////////////////////////////////

/*
const elementsOfInterest = [];

for (let x = (snapshot.fullPage) ? 0 : window.scrollX; x <= %1; x++) {
    for (let y = (snapshot.fullPage) ? 0 : window.scrollY; y <= %1; y++) {
        const elements = document.elementsFromPoint(x, y);
        if (elements.length > 0) {
            for (let z = 0; z < elements.length; z++) {
                if (elementsOfInterest.indexOf(elements[z]) == -1) {
                    elementsOfInterest.push(elements[z]);
                }
            }
        }
    }
}

for (let i = 0; i < elementsOfInterest.length; i++) {
    processElement(elementsOfInterest[0]);
}
*/

processElement(document.documentElement);
walkDom(document.body);

//////////////////////////////////////////////////////////////////////////////

return '<?xml version="1.0" encoding="UTF-8" standalone="no"?>' +  new XMLSerializer().serializeToString(svgDoc.documentElement);
