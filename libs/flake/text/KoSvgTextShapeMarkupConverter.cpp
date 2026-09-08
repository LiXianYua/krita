/*
 * SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoSvgTextShapeMarkupConverter.h"

#include <PkMemoryStream.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkXmlNode.h>
#include <PkXmlStreamAttributes.h>
#include <PkXmlStreamReader.h>
#include <PkXmlStreamWriter.h>

#include <KoDocumentResourceManager.h>
#include <KoSvgTextShape.h>
#include <SvgParser.h>
#include <SvgSavingContext.h>
#include <SvgWriter.h>
#include <html/HtmlWriter.h>

#include "kis_assert.h"

struct KoSvgTextShapeMarkupConverter::Private
{
    explicit Private(KoSvgTextShape *_shape)
        : shape(_shape)
    {
    }

    KoSvgTextShape *shape;
    PkStringList errors;
    PkStringList warnings;

    void clearErrors()
    {
        errors.clear();
        warnings.clear();
    }
};

KoSvgTextShapeMarkupConverter::KoSvgTextShapeMarkupConverter(KoSvgTextShape *shape)
    : d(new Private(shape))
{
}

KoSvgTextShapeMarkupConverter::~KoSvgTextShapeMarkupConverter() = default;

bool KoSvgTextShapeMarkupConverter::convertToSvg(PkString *svgText,
                                                  PkString *stylesText)
{
    d->clearErrors();

    PkMemoryStream shapesBuffer;
    PkMemoryStream stylesBuffer;
    shapesBuffer.open(PkStream::WriteOnly);
    stylesBuffer.open(PkStream::WriteOnly);

    {
        SvgSavingContext savingContext(shapesBuffer, stylesBuffer);
        savingContext.setStrippedTextMode(true);
        SvgWriter writer({d->shape});
        writer.saveDetached(savingContext);
    }

    shapesBuffer.close();
    stylesBuffer.close();
    *svgText = PkString::fromUtf8(shapesBuffer.data());
    *stylesText = PkString::fromUtf8(stylesBuffer.data());
    return true;
}

bool KoSvgTextShapeMarkupConverter::convertFromSvg(const PkString &svgText,
                                                    const PkString &stylesText,
                                                    const PkRectF &boundsInPixels,
                                                    qreal pixelsPerInch)
{
    d->clearErrors();

    PkString errorMessage;
    int errorLine = 0;
    int errorColumn = 0;
    const PkString fullText =
        PkString("<svg>\n%1\n%2\n</svg>\n").arg(stylesText).arg(svgText);

    PkXmlDocument doc = SvgParser::createDocumentFromSvg(
        fullText, &errorMessage, &errorLine, &errorColumn);
    if (doc.isNull()) {
        d->errors << PkString("line %1, col %2: %3")
                         .arg(errorLine)
                         .arg(errorColumn)
                         .arg(errorMessage);
        return false;
    }

    KoDocumentResourceManager resourceManager;
    SvgParser parser(&resourceManager);
    parser.setResolution(boundsInPixels, pixelsPerInch);

    const PkXmlElement root = doc.documentElement();
    PkXmlNode node = root.firstChild();
    bool textNodeFound = false;

    for (; !node.isNull(); node = node.nextSibling()) {
        const PkXmlElement element = node.toElement();
        if (element.isNull()) {
            continue;
        }

        if (element.tagName() == "defs") {
            parser.parseDefsElement(element);
        } else if (element.tagName() == "text") {
            if (textNodeFound) {
                d->errors << PkString("More than one 'text' node found!");
                return false;
            }

            KoShape *shape = parser.parseTextElement(element, d->shape);
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape == d->shape, false);
            textNodeFound = true;
            break;
        } else {
            d->errors << PkString("Unknown node of type '%1' found!")
                             .arg(element.tagName());
            return false;
        }
    }

    if (!textNodeFound) {
        d->errors << PkString("No 'text' node found!");
        return false;
    }
    return true;
}

bool KoSvgTextShapeMarkupConverter::convertToHtml(PkString *htmlText)
{
    d->clearErrors();

    PkMemoryStream shapesBuffer;
    shapesBuffer.open(PkStream::WriteOnly);
    {
        HtmlWriter writer({d->shape});
        if (!writer.save(shapesBuffer)) {
            d->errors = writer.errors();
            d->warnings = writer.warnings();
            return false;
        }
    }
    shapesBuffer.close();
    *htmlText = PkString(shapesBuffer.data());
    return true;
}

namespace
{

bool isSpanLike(const PkString &name)
{
    static const PkStringList names = {
        "span", "font", "b", "strong", "em", "i", "pre", "u"};
    return names.contains(name);
}

bool isSvgStyle(const PkString &property)
{
    static const PkStringList properties = {
        "font-family", "font-size", "font-weight", "font-variant",
        "word-spacing", "text-decoration", "font-style",
        "font-size-adjust", "font-stretch", "direction", "letter-spacing"};
    return properties.contains(property);
}

} // namespace

bool KoSvgTextShapeMarkupConverter::convertFromHtml(const PkString &htmlText,
                                                     PkString *svgText,
                                                     PkString *styles)
{
    d->clearErrors();
    *svgText = PkString();
    *styles = PkString();

    PkXmlStreamReader htmlReader(htmlText);
    PkXmlStreamWriter svgWriter(svgText);
    svgWriter.setAutoFormatting(false);

    int lineCount = 0;
    bool firstElement = true;
    PkString bodyEm("1em");
    PkString previousStyleString;
    PkString currentElement;

    while (!htmlReader.atEnd()) {
        const PkXmlStreamReader::TokenType token = htmlReader.readNext();

        if (token == PkXmlStreamReader::StartElement) {
            currentElement = htmlReader.name();
            PkString elementName = currentElement;
            PkString appendStyle;
            PkString em;
            bool newLine = false;

            if (elementName == "br") {
                svgWriter.writeEndElement();
                elementName = "p";
                em = bodyEm;
                appendStyle = previousStyleString;
            }

            PkString outputName;
            if (elementName == "body") {
                outputName = "text";
            } else if (elementName == "p") {
                outputName = "tspan";
                newLine = true;
                ++lineCount;
                if (em.isEmpty()) {
                    em = bodyEm;
                }
            } else if (isSpanLike(elementName)) {
                outputName = "tspan";
                if (elementName == "b" || elementName == "strong") {
                    appendStyle = "font-weight:700;";
                } else if (elementName == "i" || elementName == "em") {
                    appendStyle = "font-style:italic;";
                } else if (elementName == "u") {
                    appendStyle = "text-decoration:underline";
                } else if (elementName == "pre") {
                    appendStyle = "white-space:pre";
                }
            }

            if (!outputName.isEmpty()) {
                svgWriter.writeStartElement(firstElement ? PkString("text") : outputName);
                firstElement = false;
            } else {
                // Attributes belong only to the start tag emitted for this
                // input node. A writer cannot attach them to a closed parent.
                continue;
            }

            const PkXmlStreamAttributes attributes = htmlReader.attributes();
            if (elementName == "font" && attributes.hasAttribute("color")) {
                svgWriter.writeAttribute("fill", attributes.value("color"));
            }

            PkString textAlign = attributes.value("align").trimmed();
            PkString filteredStyles;
            if (attributes.hasAttribute("style")) {
                const PkStringList declarations = attributes.value("style").split(';');
                for (const PkString &declaration : declarations) {
                    const int colon = declaration.indexOf(":");
                    if (colon < 0) {
                        continue;
                    }

                    const PkString property = declaration.left(colon).trimmed();
                    const PkString value = declaration.mid(colon + 1).trimmed();
                    if (isSvgStyle(property)) {
                        filteredStyles.append(declaration).append(";");
                    } else if (property == "color") {
                        filteredStyles.append(" fill:").append(value).append(";");
                    } else if (property == "text-align") {
                        textAlign = value;
                    } else if (property == "line-height") {
                        if (value.endsWith("%")) {
                            const double percentage =
                                value.left(value.length() - 1).toDouble();
                            em = PkString::number(percentage / 100.0) + "em";
                        } else if (value.endsWith("em") || value.endsWith("px")) {
                            em = value;
                        }
                        if (elementName == "body" && !em.isEmpty()) {
                            bodyEm = em;
                        }
                    }
                }
            }

            if (textAlign == "center") {
                filteredStyles.append(" text-anchor:middle;");
            } else if (textAlign == "right") {
                filteredStyles.append(" text-anchor:end;");
            } else if (textAlign == "left") {
                filteredStyles.append(" text-anchor:start;");
            }
            filteredStyles.append(appendStyle);

            if (!filteredStyles.isEmpty()) {
                svgWriter.writeAttribute("style", filteredStyles);
                previousStyleString = filteredStyles;
            }
            if (newLine && lineCount > 1) {
                svgWriter.writeAttribute("x", "0");
                svgWriter.writeAttribute("dy", em.isEmpty() ? bodyEm : em);
            }
        } else if (token == PkXmlStreamReader::EndElement) {
            const PkString elementName = htmlReader.name();
            if (elementName == "br") {
                continue;
            }
            if (elementName == "p" || elementName == "body" ||
                isSpanLike(elementName)) {
                svgWriter.writeEndElement();
            }
            currentElement = elementName;
        } else if (token == PkXmlStreamReader::Characters) {
            if (currentElement == "style") {
                *styles = htmlReader.text();
            } else {
                svgWriter.writeCharacters(htmlReader.text());
            }
        }
    }

    if (htmlReader.hasError()) {
        d->errors << htmlReader.errorString();
        *svgText = PkString();
        return false;
    }
    return true;
}

PkStringList KoSvgTextShapeMarkupConverter::errors() const
{
    return d->errors;
}

PkStringList KoSvgTextShapeMarkupConverter::warnings() const
{
    return d->warnings;
}
