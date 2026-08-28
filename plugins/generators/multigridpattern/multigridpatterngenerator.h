/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MULTIGRID_PATTERN_GENERATOR_H
#define MULTIGRID_PATTERN_GENERATOR_H

#include <PkVariant.h>
#include "generator/kis_generator.h"


/*
 * This is de Bruijn's 1981 multigrid approach for generating aperiodic tilings
 * of the plane with rhombs. At dimensions 5, offset .2 and .4 this generator
 * makes penrose tilings.
 * Other configurations show up in nature as quasicrystals.
 * 
 * Based off the explanation of the algorithm here:
 * http://www.physics.emory.edu/faculty/weeks//software/exquasi.html
 * With extra explanation here: https://www.schoengeometry.com/c-infintil.html
 */

struct KisMultiGridRhomb {
    PkPolygonF shape;
    int parallel1;
    int parallel2;
    int line1;
    int line2;
};

class KisMultigridPatternGenerator : public KisGenerator
{
public:

    enum Connector{
        None,
        Acute,
        Obtuse,
        Cross,
        CenterDot,
        CornerDot
    };

    KisMultigridPatternGenerator();

    using KisGenerator::generate;

    void generate(KisProcessingInformation dst,
                  const PkSize& size,
                  const KisFilterConfigurationSP config,
                  KoUpdater* progressUpdater
                 ) const override;

    static inline KoID id() {
        return KoID("multigrid", "Multigrid");
    }
    
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

    // XXX: Fix the generation to work with tiles
    virtual bool allowsSplittingIntoPatches() const override { return false; }

private:
    PkList<KisMultiGridRhomb> generateRhombs(int lines, int divisions, qreal offset) const;

    PkList<int> getIndicesFromPoint(PkPointF point, PkList<qreal> angles, qreal offset) const;

    /**
     * Projects the 5d vertice to a point.
     */
    PkPointF getVertice(PkList<int> indices, PkList<qreal> angles) const;
};

#endif
