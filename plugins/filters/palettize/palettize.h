/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2019 Carl Olsson <carl.olsson@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PALETTIZE_H
#define PALETTIZE_H

#include <kis_filter.h>
#include <kis_filter_configuration.h>
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/register/point.hpp>

class Palettize : public QObject
{
    Q_OBJECT
public:
    Palettize(QObject *parent, const PkVariantList &);
};

class KisFilterPalettize : public KisFilter
{
public:
    enum Colorspace {
        Lab,
        RGB
    };
    enum AlphaMode {
        Clip,
        Index,
        Dither
    };
    enum ThresholdMode {
        Pattern,
        Noise
    };
    enum PatternValueMode {
        Auto,
        Lightness,
        Alpha
    };
    enum ColorMode {
        PerChannelOffset,
        NearestColors
    };
    KisFilterPalettize();
    static inline KoID id() { return KoID("palettize", i18n("Palettize")); }
    KisFilterConfigurationSP factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
    void processImpl(KisPaintDeviceSP device, const PkRect &applyRect, const KisFilterConfigurationSP config, KoUpdater *progressUpdater) const override;
};

#endif
