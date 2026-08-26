/*
 * SPDX-FileCopyrightText: 2015 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NORMALIZE_H
#define NORMALIZE_H

#include "filter/kis_color_transformation_filter.h"


class KisFilterNormalize : public KisColorTransformationFilter
{
public:
    KisFilterNormalize();
public:
    KoColorTransformation* createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const override;
};

class KisNormalizeTransformation : public KoColorTransformation
{
public:
    KisNormalizeTransformation(const KoColorSpace* cs);
    void transform(const quint8* src, quint8* dst, qint32 nPixels) const override;
private:
    const KoColorSpace* m_colorSpace;
    quint32 m_psize;
};

#endif
