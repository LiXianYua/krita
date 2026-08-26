/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisColorfulBrush.h"


KisColorfulBrush::KisColorfulBrush(const PkString &filename)
    : KisScalingSizeBrush(filename)
{
}

#include <KoColorSpaceMaths.h>
#include <KoColorSpaceTraits.h>

namespace {

qreal estimateImageAverage(const PkImage &image) {
    qint64 lightnessSum = 0;
    qint64 alphaSum = 0;

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(image.format() == PkImage::Format_ARGB32, 0.5);

    for (int y = 0; y < image.height(); ++y) {
        const PkRgb *pixel = reinterpret_cast<const PkRgb*>(image.scanLine(y));

        for (int i = 0; i < image.width(); ++i) {
            lightnessSum += qRound(qGray(*pixel) * qAlpha(*pixel) / 255.0);
            alphaSum += qAlpha(*pixel);
            pixel++;
        }
    }

    if (alphaSum == 0) {
        return 0;
    }
    return 255.0 * qreal(lightnessSum) / alphaSum;
}

}

qreal KisColorfulBrush::estimatedSourceMidPoint() const
{
    return estimateImageAverage(KisBrush::brushTipImage());
}

qreal KisColorfulBrush::adjustedMidPoint() const
{
    return estimateImageAverage(this->brushTipImage());
}

bool KisColorfulBrush::autoAdjustMidPoint() const
{
    return m_autoAdjustMidPoint;
}

void KisColorfulBrush::setAutoAdjustMidPoint(bool autoAdjustMidPoint)
{
    m_autoAdjustMidPoint = autoAdjustMidPoint;
}

PkImage KisColorfulBrush::brushTipImage() const
{
    PkImage image = KisBrush::brushTipImage();
    if (isImageType() && brushApplication() != IMAGESTAMP) {

        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(image.format() == PkImage::Format_ARGB32, image);

        const qreal adjustmentMidPoint =
                m_autoAdjustMidPoint ?
                estimateImageAverage(image) :
                m_adjustmentMidPoint;

        if (qAbs(adjustmentMidPoint - 127.0) > 0.1 ||
            !qFuzzyIsNull(m_brightnessAdjustment) ||
            !qFuzzyIsNull(m_contrastAdjustment)) {

            const int half = KoColorSpaceMathsTraits<quint8>::halfValue;
            const int unit = KoColorSpaceMathsTraits<quint8>::unitValue;

            const qreal midX = adjustmentMidPoint;
            const qreal midY = m_brightnessAdjustment > 0 ?
                        KoColorSpaceMaths<qreal>::blend(unit, half, m_brightnessAdjustment) :
                        KoColorSpaceMaths<qreal>::blend(0, half, -m_brightnessAdjustment);

            qreal loA = 0.0;
            qreal hiA = 0.0;

            qreal loB = 0.0;
            qreal hiB = 255.0;

            if (!qFuzzyCompare(m_contrastAdjustment, 1.0)) {
                if (m_contrastAdjustment > 0.0) {
                    loA = midY / (1.0 - m_contrastAdjustment) / midX;
                    hiA = (unit - midY) / (1.0 - m_contrastAdjustment) / (unit - midX);
                } else {
                    loA = midY * (1.0 + m_contrastAdjustment) / midX;
                    hiA = (unit - midY) * (1.0 + m_contrastAdjustment) / (unit - midX);
                }

                loB = midY - midX * loA;
                hiB = midY - midX * hiA;
            }

            for (int y = 0; y < image.height(); y++) {
                PkRgb *pixel = reinterpret_cast<PkRgb *>(image.scanLine(y));
                for (int x = 0; x < image.width(); x++) {
                    PkRgb c = pixel[x];

                    int v = qGray(c);

                    if (v >= midX) {
                        v = qMin(unit, qRound(hiA * v + hiB));
                    } else {
                        v = qMax(0, qRound(loA * v + loB));
                    }

                    pixel[x] = qRgba(v, v, v, qAlpha(c));
                }
            }
        } else {
            for (int y = 0; y < image.height(); y++) {
                PkRgb *pixel = reinterpret_cast<PkRgb *>(image.scanLine(y));
                for (int x = 0; x < image.width(); x++) {
                    PkRgb c = pixel[x];

                    int v = qGray(c);
                    pixel[x] = qRgba(v, v, v, qAlpha(c));
                }
            }
        }
    }
    return image;
}

void KisColorfulBrush::setAdjustmentMidPoint(quint8 value)
{
    if (m_adjustmentMidPoint != value) {
        m_adjustmentMidPoint = value;
        clearBrushPyramid();
    }
}

void KisColorfulBrush::setBrightnessAdjustment(qreal value)
{
    if (m_brightnessAdjustment != value) {
        m_brightnessAdjustment = value;
        clearBrushPyramid();
    }
}

void KisColorfulBrush::setContrastAdjustment(qreal value)
{
    if (m_contrastAdjustment != value) {
        m_contrastAdjustment = value;
        clearBrushPyramid();
    }
}

bool KisColorfulBrush::isImageType() const
{
    return brushType() == IMAGE || brushType() == PIPE_IMAGE;
}

quint8 KisColorfulBrush::adjustmentMidPoint() const
{
    return m_adjustmentMidPoint;
}

qreal KisColorfulBrush::brightnessAdjustment() const
{
    return m_brightnessAdjustment;
}

qreal KisColorfulBrush::contrastAdjustment() const
{
    return m_contrastAdjustment;
}

#include <PkXmlElement.h>

void KisColorfulBrush::toXML(PkXmlDocument& d, PkXmlElement& e) const
{
    // legacy setting, now 'brushApplication' is used instead
    e.setAttribute("ColorAsMask", PkString("%1").arg((int)(brushApplication() != IMAGESTAMP)));

    e.setAttribute("AdjustmentMidPoint", PkString("%1").arg(m_adjustmentMidPoint));
    e.setAttribute("BrightnessAdjustment", PkString("%1").arg(m_brightnessAdjustment));
    e.setAttribute("ContrastAdjustment", PkString("%1").arg(m_contrastAdjustment));
    e.setAttribute("AutoAdjustMidPoint", PkString("%1").arg(m_autoAdjustMidPoint));
    e.setAttribute("AdjustmentVersion", PkString("%1").arg(2));
    KisBrush::toXML(d, e);
}

void KisColorfulBrush::setHasColorAndTransparency(bool value)
{
    m_hasColorAndTransparency = value;
}

bool KisColorfulBrush::hasColorAndTransparency() const
{
    return m_hasColorAndTransparency;
}
