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
#include <QBuffer>
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

    QImage image;
    QScopedPointer<SvgUtil::PreserveAspectRatioParser> ratioParser;
    QTransform viewBoxTransform;
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
    painter.setClipRect(QRectF(QPointF(), size()), Qt::IntersectClip);
    painter.setTransform(m_d->viewBoxTransform, true);
    painter.drawImage(QPoint(), m_d->image);
}

void ImageShape::setSize(const QSizeF &size)
{
    KoShape::setSize(size);
}

bool ImageShape::saveSvg(SvgSavingContext &context)
{
    const QString uid = context.createUID("image");

    context.shapeWriter().startElement("image");
    context.shapeWriter().addAttribute("id", uid.toUtf8().constData());
    SvgUtil::writeTransformAttributeLazy("transform", toPkTransform(transformation()), context.shapeWriter());
    context.shapeWriter().addAttribute("width", QString("%1px").arg(toQString(KisDomUtils::toString(size().width()))).toUtf8().constData());
    context.shapeWriter().addAttribute("height", QString("%1px").arg(toQString(KisDomUtils::toString(size().height()))).toUtf8().constData());

    QString aspectString = m_d->ratioParser ? toQString(m_d->ratioParser->toString()) : QString();
    if (!aspectString.isEmpty()) {
        context.shapeWriter().addAttribute("preserveAspectRatio", aspectString.toUtf8().constData());
    }

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    if (m_d->image.save(&buffer, "PNG")) {
        const QString mimeType = toQString(KisMimeDatabase::mimeTypeForSuffix("*.png"));
        context.shapeWriter().addAttribute("xlink:href", ("data:" + mimeType + ";base64," + buffer.data().toBase64()).toUtf8().constData());
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

    setSize(QSizeF(w, h));
    setPosition(QPointF(x, y));

    if (w == 0.0 || h == 0.0) {
        setVisible(false);
    }

    const PkString fileName = element.attribute("xlink:href");

    QByteArray data;

    if (fileName.startsWith("data:")) {

        QRegularExpression re("data:(.+?);base64,(.+)");
        QRegularExpressionMatch match = re.match(toQString(fileName));

        data = match.captured(2).toLatin1();
        data = QByteArray::fromBase64(data);
    } else {
        data = toQByteArray(context.fetchExternalFile(fileName));
    }

    if (!data.isEmpty()) {
        QBuffer buffer(&data);
        m_d->image.load(&buffer, "");
    }

    const PkString aspectString = element.attribute("preserveAspectRatio", "xMidYMid meet");
    m_d->ratioParser.reset(new SvgUtil::PreserveAspectRatioParser(aspectString));

    if (!m_d->image.isNull()) {

        m_d->viewBoxTransform =
             QTransform::fromScale(w / m_d->image.width(), h / m_d->image.height());

        PkTransform viewTransform = toPkTransform(m_d->viewBoxTransform);
        SvgUtil::parseAspectRatio(*m_d->ratioParser,
                                  toPkRectF(QRectF(QPointF(), size())),
                                  toPkRectF(QRectF(QPoint(), m_d->image.size())),
                                  &viewTransform);
        m_d->viewBoxTransform = toQTransform(viewTransform);
    }

    if (m_d->ratioParser->defer) {
        // TODO:
    }

    return true;
}

void ImageShape::setImage(const QImage &img)
{
    if (m_d->image != img) {
        m_d->image = img;
        shapeChanged(KoShape::ContentChanged);
    }
}

QImage ImageShape::image() const
{
    return m_d->image;
}

void ImageShape::setViewBoxTransform(const QTransform &tf)
{
    if (m_d->viewBoxTransform != tf) {
        m_d->viewBoxTransform = tf;
        shapeChanged(KoShape::GenericMatrixChange);
    }
}

QTransform ImageShape::viewBoxTransform() const
{
    return m_d->viewBoxTransform;
}
