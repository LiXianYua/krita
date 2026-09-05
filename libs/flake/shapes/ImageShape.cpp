/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "ImageShape.h"
#include "kis_debug.h"

#include <QPainter>
#include <SvgLoadingContext.h>
#include <SvgSavingContext.h>
#include <SvgUtil.h>
#include <SvgStyleWriter.h>
#include <PkMemoryStream.h>
#include <KisMimeDatabase.h>
#include <KoXmlWriter.h>
#include "kis_dom_utils.h"
#include <QRegularExpression>
#include "KisQPainterStateSaver.h"


struct Q_DECL_HIDDEN ImageShape::Private : public QSharedData
{
    Private() {}
    Private(const Private &rhs)
        : QSharedData(),
          image(rhs.image),
          ratioParser(rhs.ratioParser ? new SvgUtil::PreserveAspectRatioParser(*rhs.ratioParser) : 0),
          viewBoxTransform(rhs.viewBoxTransform)
    {
    }

    PkImage image;
    PkScopedPointer<SvgUtil::PreserveAspectRatioParser> ratioParser;
    PkTransform viewBoxTransform;
};


ImageShape::ImageShape()
    : m_d(new Private)
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

KoShape *ImageShape::cloneShape() const
{
    return new ImageShape(*this);
}

void ImageShape::paint(QPainter &painter) const
{
    KisQPainterStateSaver saver(&painter);

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setClipRect(toQRectF(PkRectF(PkPointF(), size())), Qt::IntersectClip);
    painter.setTransform(toQTransform(m_d->viewBoxTransform), true);
    painter.drawImage(QPoint(), toQImage(m_d->image));
}

void ImageShape::setSize(const PkSizeF &size)
{
    KoShape::setSize(size);
}

bool ImageShape::saveSvg(SvgSavingContext &context)
{
    const PkString uid = context.createUID("image");

    context.shapeWriter().startElement("image");
    context.shapeWriter().addAttribute("id", uid.toUtf8().constData());
    SvgUtil::writeTransformAttributeLazy("transform", transformation(), context.shapeWriter());
    context.shapeWriter().addAttribute("width", PkString("%1px").arg(KisDomUtils::toString(size().width())).toUtf8().constData());
    context.shapeWriter().addAttribute("height", PkString("%1px").arg(KisDomUtils::toString(size().height())).toUtf8().constData());

    PkString aspectString = m_d->ratioParser ? m_d->ratioParser->toString() : PkString();
    if (!aspectString.isEmpty()) {
        context.shapeWriter().addAttribute("preserveAspectRatio", aspectString.toUtf8().constData());
    }

    // 过渡期：PNG 编码/base64 走 Qt（QImage::save）
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (toQImage(m_d->image).save(&buffer, "PNG")) {
        const PkString mimeType = toPkString(KisMimeDatabase::mimeTypeForSuffix("*.png"));
        context.shapeWriter().addAttribute("xlink:href", ("data:" + mimeType + ";base64," + PkString(png.toBase64().constData())).toUtf8().constData());
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

        QRegularExpression re("data:(.+?);base64,(.+)");
        QRegularExpressionMatch match = re.match(toQString(fileName));

        data = toPkByteArray(match.captured(2).toLatin1());
        // base64 解码走 Qt（过渡期）
        data = toPkByteArray(QByteArray::fromBase64(toQByteArray(data)));
    } else {
        data = toPkByteArray(context.fetchExternalFile(fileName));
    }

    if (!data.isEmpty()) {
        // 过渡期：PNG 解码走 Qt（QImage::load）
        QByteArray raw = toQByteArray(data);
        QBuffer buffer(&raw);
        buffer.open(QIODevice::ReadOnly);
        QImage loaded;
        loaded.load(&buffer, "");
        m_d->image = toPkImage(loaded);
    }

    const PkString aspectString = element.attribute("preserveAspectRatio", "xMidYMid meet");
    m_d->ratioParser.reset(new SvgUtil::PreserveAspectRatioParser(aspectString));

    if (!m_d->image.isNull()) {

        m_d->viewBoxTransform =
             PkTransform::fromScale(w / m_d->image.width(), h / m_d->image.height());

        PkTransform viewTransform = m_d->viewBoxTransform;
        SvgUtil::parseAspectRatio(*m_d->ratioParser,
                                  toPkRectF(PkRectF(PkPointF(), size())),
                                  toPkRectF(PkRectF(PkPoint(), m_d->image.size())),
                                  &viewTransform);
        m_d->viewBoxTransform = viewTransform;
    }

    if (m_d->ratioParser->defer) {
        // TODO:
    }

    return true;
}

void ImageShape::setImage(const PkImage &img)
{
    if (m_d->image != img) {
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
        m_d->viewBoxTransform = tf;
        shapeChanged(KoShape::GenericMatrixChange);
    }
}

PkTransform ImageShape::viewBoxTransform() const
{
    return m_d->viewBoxTransform;
}
