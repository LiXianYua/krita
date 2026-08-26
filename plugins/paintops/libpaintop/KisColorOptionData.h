/*
 *  SPDX-FileCopyrightText: 2022 Agata Cacko <cacko.azh@gmail.com>
 *  SPDX-FileCopyrightText: 2009, 2010 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_COLOR_OPTION_DATA_H
#define KIS_COLOR_OPTION_DATA_H


#include "kis_types.h"
#include <boost/operators.hpp>
#include <kritapaintop_export.h>

class KisPropertiesConfiguration;


const PkString COLOROP_HUE = "ColorOption/hue";
const PkString COLOROP_SATURATION = "ColorOption/saturation";
const PkString COLOROP_VALUE = "ColorOption/value";

const PkString COLOROP_USE_RANDOM_HSV = "ColorOption/useRandomHSV";
const PkString COLOROP_USE_RANDOM_OPACITY = "ColorOption/useRandomOpacity";
const PkString COLOROP_SAMPLE_COLOR = "ColorOption/sampleInputColor";

const PkString COLOROP_FILL_BG = "ColorOption/fillBackground";
const PkString COLOROP_COLOR_PER_PARTICLE = "ColorOption/colorPerParticle";
const PkString COLOROP_MIX_BG_COLOR = "ColorOption/mixBgColor";


struct PAINTOP_EXPORT KisColorOptionData : boost::equality_comparable<KisColorOptionData>
{	
	
    inline friend bool operator==(const KisColorOptionData &lhs, const KisColorOptionData &rhs) {
        return lhs.useRandomHSV == rhs.useRandomHSV
			&& lhs.useRandomOpacity == rhs.useRandomOpacity
			&& lhs.sampleInputColor == rhs.sampleInputColor
			
			&& lhs.fillBackground == rhs.fillBackground
			&& lhs.colorPerParticle == rhs.colorPerParticle
			&& lhs.mixBgColor == rhs.mixBgColor
			
			&& lhs.hue == rhs.hue
			&& lhs.saturation == rhs.saturation
			&& lhs.value == rhs.value
			;
    }


	bool useRandomHSV {false};
    bool useRandomOpacity {false};
    bool sampleInputColor {false};

    bool fillBackground {false};
    bool colorPerParticle {false};
    bool mixBgColor {false};

    int hue {0};
    int saturation {0};
    int value {0};
	
	// functions
    bool read(const KisPropertiesConfiguration *setting);
    void write(KisPropertiesConfiguration *setting) const;
    
};

#endif // KIS_COLOR_OPTION_DATA_H
