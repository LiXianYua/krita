/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "HtmlWriter.h"

#include <PkStream.h>
#include <PkTextStream.h>

#include <KoShape.h>
#include <KoShapeLayer.h>
#include <KoShapeGroup.h>
#include <KoSvgTextShape.h>

#include <html/HtmlSavingContext.h>

HtmlWriter::HtmlWriter(const PkList<KoShape*> &toplevelShapes)
    : m_toplevelShapes(toplevelShapes)
{
}

HtmlWriter::~HtmlWriter()
{
}

bool HtmlWriter::save(PkStream &outputDevice)
{
    if (m_toplevelShapes.isEmpty()) {
        return false;
    }

    PkTextStream htmlStream(&outputDevice);
    htmlStream.setCodec("UTF-8");

    // header
    htmlStream << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" "
                                "\"http://www.w3.org/TR/REC-html40/strict.dtd\">"
                                "<html><head><meta name=\"Krita Svg Text\" />"
                                "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/>"
                                "</head>";
    htmlStream.flush();
    {
        HtmlSavingContext savingContext(outputDevice);
        saveShapes(m_toplevelShapes, savingContext);
    }
    htmlStream << "</html>";
    htmlStream.flush();
    return true;
}

PkStringList HtmlWriter::errors() const
{
    return m_errors;
}

PkStringList HtmlWriter::warnings() const
{
    return m_warnings;
}

void HtmlWriter::saveShapes(const PkList<KoShape *> shapes, HtmlSavingContext &savingContext)
{
    for (KoShape *shape : shapes) {
        KoShapeLayer *layer = dynamic_cast<KoShapeLayer*>(shape);
        if (layer) {
            m_errors << PkString("Saving KoShapeLayer to html is not implemented yet!");
        } else {
            KoShapeGroup *group = dynamic_cast<KoShapeGroup*>(shape);
            if (group) {
                m_errors << PkString("KoShapeGroup to html is not implemented yet!");
            }
            else {
                KoSvgTextShape *svgTextShape = dynamic_cast<KoSvgTextShape*>(shape);
                if (svgTextShape) {
                    if (!svgTextShape->saveHtml(savingContext)) {
                        m_errors << PkString("saving to html failed");
                    }
                }
                else {
                    m_errors << PkString("Cannot save %1 to html").arg(shape->name());
                }
            }
        }
    }
}
