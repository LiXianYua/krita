/*
 *  SPDX-FileCopyrightText: 2012 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_DISPLAY_FILTER_H
#define KIS_DISPLAY_FILTER_H

#include <cstdint>

#include <kritacanvas_export.h>

struct KisExposureGammaCorrectionInterface;

/**
 * @brief The KisDisplayFilter class is the base class for filters that
 * are applied by the canvas to the projection before displaying.
 */
class KRITACANVAS_EXPORT KisDisplayFilter
{
public:
    KisDisplayFilter();
    virtual ~KisDisplayFilter();

    virtual void filter(std::uint8_t *pixels, std::uint32_t numPixels) = 0;
    virtual void approximateInverseTransformation(std::uint8_t *pixels, std::uint32_t numPixels) = 0;
    virtual void approximateForwardTransformation(std::uint8_t *pixels, std::uint32_t numPixels) = 0;
    virtual bool useInternalColorManagement() const = 0;
    virtual KisExposureGammaCorrectionInterface *correctionInterface() const = 0;
    virtual bool lockCurrentColorVisualRepresentation() const = 0;
};


#endif
