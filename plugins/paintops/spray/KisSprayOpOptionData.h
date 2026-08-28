#include <PkString.h>
/*
 *  SPDX-FileCopyrightText: 2022 Agata Cacko <cacko.azh@gmail.com>
 *  SPDX-FileCopyrightText: 2008-2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_SPRAYOP_OPTION_DATA_H
#define KIS_SPRAYOP_OPTION_DATA_H


#include "kis_types.h"
#include <boost/operators.hpp>
#include <kritapaintop_export.h>

#include <kis_cubic_curve.h>

#include "KisSprayRandomDistributions.h"

class KisPropertiesConfiguration;


const PkString SPRAY_DIAMETER = "Spray/diameter";
const PkString SPRAY_ASPECT = "Spray/aspect";
const PkString SPRAY_ROTATION = "Spray/rotation";
const PkString SPRAY_SCALE = "Spray/scale";
const PkString SPRAY_SPACING = "Spray/spacing";
const PkString SPRAY_JITTER_MOVEMENT = "Spray/jitterMovement";
const PkString SPRAY_JITTER_MOVE_AMOUNT = "Spray/jitterMoveAmount";
const PkString SPRAY_USE_DENSITY = "Spray/useDensity";
const PkString SPRAY_PARTICLE_COUNT = "Spray/particleCount";
const PkString SPRAY_COVERAGE = "Spray/coverage";
const PkString SPRAY_ANGULAR_DISTRIBUTION_TYPE = "Spray/angularDistributionType";
const PkString SPRAY_ANGULAR_DISTRIBUTION_CURVE = "Spray/angularDistributionCurve";
const PkString SPRAY_ANGULAR_DISTRIBUTION_CURVE_REPEAT = "Spray/angularDistributionCurveRepeat";
const PkString SPRAY_RADIAL_DISTRIBUTION_TYPE = "Spray/radialDistributionType";
const PkString SPRAY_RADIAL_DISTRIBUTION_STD_DEVIATION = "Spray/radialDistributionStdDeviation";
const PkString SPRAY_RADIAL_DISTRIBUTION_CLUSTERING_AMOUNT = "Spray/radialDistributionClusteringAmount";
const PkString SPRAY_RADIAL_DISTRIBUTION_CURVE = "Spray/radialDistributionCurve";
const PkString SPRAY_RADIAL_DISTRIBUTION_CURVE_REPEAT = "Spray/radialDistributionCurveRepeat";
const PkString SPRAY_RADIAL_DISTRIBUTION_CENTER_BIASED = "Spray/radialDistributionCenterBiased";
const PkString SPRAY_GAUSS_DISTRIBUTION = "Spray/gaussianDistribution";


struct KisSprayOpOptionData : boost::equality_comparable<KisSprayOpOptionData>
{
	enum ParticleDistribution
    {
        ParticleDistribution_Uniform,
        ParticleDistribution_Gaussian,
        ParticleDistribution_ClusterBased,
        ParticleDistribution_CurveBased
    };
	
    inline friend bool operator==(const KisSprayOpOptionData &lhs, const KisSprayOpOptionData &rhs) {
        return lhs.diameter == rhs.diameter // 10 entries
			&& lhs.aspect == rhs.aspect
			&& lhs.brushRotation == rhs.brushRotation
			&& lhs.scale == rhs.scale
			&& lhs.spacing == rhs.spacing
			&& lhs.jitterMovement == rhs.jitterMovement
			&& lhs.jitterAmount == rhs.jitterAmount
			&& lhs.useDensity == rhs.useDensity
			&& lhs.particleCount == rhs.particleCount
			&& lhs.coverage == rhs.coverage
			// 9 entries
			&& lhs.angularDistributionType == rhs.angularDistributionType
			&& lhs.angularDistributionCurve == rhs.angularDistributionCurve
			&& lhs.angularDistributionCurveRepeat == rhs.angularDistributionCurveRepeat
			&& lhs.radialDistributionType == rhs.radialDistributionType
			&& lhs.radialDistributionStdDeviation == rhs.radialDistributionStdDeviation
			&& lhs.radialDistributionClusteringAmount == rhs.radialDistributionClusteringAmount
			&& lhs.radialDistributionCurve == rhs.radialDistributionCurve
			&& lhs.radialDistributionCurveRepeat == rhs.radialDistributionCurveRepeat
			&& lhs.radialDistributionCenterBiased == rhs.radialDistributionCenterBiased;
			// 7 entries - but there is no need to compare functors
			
    }

	// sane defaults (for Coverity)
	// NOTE: if you add any new variable, make sure it's present in all places! including == function
	// 10 entries
    quint16 diameter {100};
    qreal aspect {1.0};
    qreal brushRotation {0.0};
    qreal scale {1.0};
    qreal spacing {0.5};
    bool jitterMovement {false};
    qreal jitterAmount {1.0};
    bool useDensity {false};
    quint16 particleCount {12};
    qreal coverage {0.003};
	
    // 9 entries
    ParticleDistribution angularDistributionType {ParticleDistribution_Uniform};
    PkString angularDistributionCurve {DEFAULT_CURVE_STRING};
    int angularDistributionCurveRepeat {1};
    ParticleDistribution radialDistributionType {ParticleDistribution_Uniform};
    qreal radialDistributionStdDeviation {0.5};
    qreal radialDistributionClusteringAmount {0.0};
    PkString radialDistributionCurve {DEFAULT_CURVE_STRING};
    int radialDistributionCurveRepeat {1};
    bool radialDistributionCenterBiased {false};

	// functions
    bool read(const KisPropertiesConfiguration *setting);
    void write(KisPropertiesConfiguration *setting) const;
};

#endif // KIS_SPRAYOP_OPTION_DATA_H
