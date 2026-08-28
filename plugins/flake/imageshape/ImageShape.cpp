/*
 * SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ImageShape.h"
#include "ImageShapePngData.h"

#include <SvgLoadingContext.h>
#include <SvgSavingContext.h>
#include <SvgStyleWriter.h>
#include <SvgUtil.h>
#include <KoXmlWriter.h>
#include "kis_dom_utils.h"

ImageShape::ImageShape() = default;

ImageShape::ImageShape(const ImageShape &rhs)
    : KoShape(rhs)
    , m_state(rhs.m_state)
{
}

ImageShape::~ImageShape() = default;

KoShape *ImageShape::cloneShape() const
{
    return new ImageShape(*this);
}

void ImageShape::setSize(const PkSizeF &size)
{
    KoShape::setSize(size);
}

bool ImageShape::saveSvg(SvgSavingContext &context)
{
    const PkString uid = context.createUID("image");
    context.shapeWriter().startElement("image");
    context.shapeWriter().addAttribute("id", uid);
    SvgUtil::writeTransformAttributeLazy("transform", transformation(), context.shapeWriter());
    context.shapeWriter().addAttribute("width", PkString("%1px").arg(KisDomUtils::toString(size().width())));
    context.shapeWriter().addAttribute("height", PkString("%1px").arg(KisDomUtils::toString(size().height())));

    const ImageShapeState &state = m_state.read();
    const PkString aspectString = state.ratioParser ? state.ratioParser->toString() : PkString();
    if (!aspectString.isEmpty()) context.shapeWriter().addAttribute("preserveAspectRatio", aspectString);
    const PkString dataUri = ImageShapePngData::encodeDataUri(state.image);
    if (!dataUri.isEmpty()) context.shapeWriter().addAttribute("xlink:href", dataUri);
    SvgStyleWriter::saveMetadata(this, context);
    context.shapeWriter().endElement();
    return true;
}

bool ImageShape::loadSvg(const PkXmlElement &element, SvgLoadingContext &context)
{
    const qreal x = SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), element.attribute("x"));
    const qreal y = SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), element.attribute("y"));
    const qreal w = SvgUtil::parseUnitX(context.currentGC(), context.resolvedProperties(), element.attribute("width"));
    const qreal h = SvgUtil::parseUnitY(context.currentGC(), context.resolvedProperties(), element.attribute("height"));
    setSize(PkSizeF(w, h));
    setPosition(PkPointF(x, y));
    if (w == 0.0 || h == 0.0) setVisible(false);

    const PkString fileName = element.attribute("xlink:href");
    PkByteArray data;
    if (fileName.startsWith("data:")) {
        data = ImageShapePngData::decodeDataUriBase64(fileName);
    } else {
        data = context.fetchExternalFile(fileName);
    }

    ImageShapeState &state = m_state.write();
    if (!data.isEmpty()) state.image = ImageShapePngData::decodeImage(data);
    const PkString aspectString = element.attribute("preserveAspectRatio", "xMidYMid meet");
    state.ratioParser = std::make_unique<SvgUtil::PreserveAspectRatioParser>(aspectString);
    state.viewBoxTransform = PkTransform();
    if (!state.image.isNull()) {
        state.viewBoxTransform = PkTransform::fromScale(w / state.image.width(), h / state.image.height());
        SvgUtil::parseAspectRatio(*state.ratioParser,
                                  PkRectF(PkPointF(), PkSizeF(w, h)),
                                  PkRect(PkPoint(), state.image.size()),
                                  &state.viewBoxTransform);
    }
    return true;
}

void ImageShape::setImage(const PkImage &image)
{
    if (m_state.read().image != image) {
        m_state.write().image = image;
        shapeChanged(KoShape::ContentChanged);
    }
}

PkImage ImageShape::image() const
{
    return m_state.read().image;
}

void ImageShape::setViewBoxTransform(const PkTransform &transform)
{
    if (m_state.read().viewBoxTransform != transform) {
        m_state.write().viewBoxTransform = transform;
        shapeChanged(KoShape::GenericMatrixChange);
    }
}

PkTransform ImageShape::viewBoxTransform() const
{
    return m_state.read().viewBoxTransform;
}
