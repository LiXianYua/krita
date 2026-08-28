/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ImageShape.h"
#include "ImageShapePngData.h"

#include <SvgLoadingContext.h>
#include <SvgSavingContext.h>
#include <SvgUtil.h>
#include <SvgStyleWriter.h>
#include <KoXmlWriter.h>
#include "kis_dom_utils.h"

#include <string>

struct ImageShape::Private
{
    Private() = default;
    Private(const Private &rhs)
        : image(rhs.image)
        , ratioParser(rhs.ratioParser
                          ? std::make_unique<SvgUtil::PreserveAspectRatioParser>(*rhs.ratioParser)
                          : nullptr)
        , viewBoxTransform(rhs.viewBoxTransform)
    {
    }

    PkImage image;
    std::unique_ptr<SvgUtil::PreserveAspectRatioParser> ratioParser;
    PkTransform viewBoxTransform;
};


ImageShape::ImageShape()
    : m_d(std::make_shared<Private>())
{
}

ImageShape::ImageShape(const ImageShape &rhs)
    : KoShape(rhs),
      m_d(rhs.m_d)
{
}

ImageShape::~ImageShape()
{
}

void ImageShape::detach()
{
    if (!m_d.unique()) {
        m_d = std::make_shared<Private>(*m_d);
    }
}

KoShape *ImageShape::cloneShape() const
{
    return new ImageShape(*this);
}

void ImageShape::paint(void *paintContext) const
{
    // S-09/M5 GAP: image drawing resumes when the Pk renderer is delivered.
    (void)paintContext;
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

    PkString aspectString = m_d->ratioParser? m_d->ratioParser->toString(): PkString();
    if (!aspectString.isEmpty()) {
        context.shapeWriter().addAttribute("preserveAspectRatio", aspectString);
    }

    const PkString dataUri = ImageShapePngData::encodeDataUri(m_d->image);
    if (!dataUri.isEmpty()) {
        context.shapeWriter().addAttribute("xlink:href", dataUri);
    }
    SvgStyleWriter::saveMetadata(this, context);

    context.shapeWriter().endElement(); // image

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

    if (w == 0.0 || h == 0.0) {
        setVisible(false);
    }

    const PkString fileName = element.attribute("xlink:href");

    PkByteArray data;

    if (fileName.startsWith("data:")) {
        const std::string marker(";base64,");
        const std::string encodedUri = fileName.PkToUtf8();
        const std::size_t payloadOffset = encodedUri.find(marker);
        if (payloadOffset != std::string::npos) {
            data = ImageShapePngData::decodeBase64(
                PkString(encodedUri.substr(payloadOffset + marker.size()).c_str()));
        }
    } else {
        data = context.fetchExternalFile(fileName);
    }

    detach();
    if (!data.isEmpty()) {
        m_d->image = ImageShapePngData::decodePng(data);
    }

    const PkString aspectString = element.attribute("preserveAspectRatio", "xMidYMid meet");
    m_d->ratioParser.reset(new SvgUtil::PreserveAspectRatioParser(aspectString));

    if (!m_d->image.isNull()) {

        m_d->viewBoxTransform =
             PkTransform::fromScale(w / m_d->image.width(), h / m_d->image.height());

        SvgUtil::parseAspectRatio(*m_d->ratioParser,
                                  PkRectF(PkPointF(), PkSizeF(w, h)),
                                  PkRect(PkPoint(), m_d->image.size()),
                                  &m_d->viewBoxTransform);
    }

    if (m_d->ratioParser->defer) {
        // TODO:
    }

    return true;
}

void ImageShape::setImage(const PkImage &img)
{
    if (m_d->image != img) {
        detach();
        m_d->image = img;
        shapeChanged(KoShape::ContentChanged);
    }
}

PkImage ImageShape::image() const
{
    return m_d->image;
}

void ImageShape::setViewBoxTransform(const PkTransform &tf)
{
    if (m_d->viewBoxTransform != tf) {
        detach();
        m_d->viewBoxTransform = tf;
        shapeChanged(KoShape::GenericMatrixChange);
    }
}

PkTransform ImageShape::viewBoxTransform() const
{
    return m_d->viewBoxTransform;
}
