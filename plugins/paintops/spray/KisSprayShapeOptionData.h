#include <PkString.h>
#include <PkSize.h>
#include <PkImage.h>
/*
 *  SPDX-FileCopyrightText: 2022 Agata Cacko <cacko.azh@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_SPRAY_SHAPE_OPTION_DATA_H
#define KIS_SPRAY_SHAPE_OPTION_DATA_H


#include "kis_types.h"
#include <boost/operators.hpp>
#include <kritapaintop_export.h>

#include <PkImage.h>

class KisPropertiesConfiguration;

struct KisSprayShapeOptionData : boost::equality_comparable<KisSprayShapeOptionData>
{
    inline friend bool operator==(const KisSprayShapeOptionData &lhs, const KisSprayShapeOptionData &rhs) {
        return lhs.shape == rhs.shape
			&& lhs.size == rhs.size
			&& lhs.enabled == rhs.enabled
			&& lhs.proportional == rhs.proportional
			&& lhs.imageUrl == rhs.imageUrl;
    }
    
    // particle type size
    quint8 shape;
    PkSize size;
    bool enabled;
    bool proportional;
    
    // rotation
    PkImage image;
    PkString imageUrl;

    bool read(const KisPropertiesConfiguration *setting);
    void write(KisPropertiesConfiguration *setting) const;

    PkSize effectiveSize(int diameter, qreal scale) const;
};

#endif // KIS_SPRAY_SHAPE_OPTION_DATA_H
