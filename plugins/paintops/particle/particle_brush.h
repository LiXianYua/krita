/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef _PARTICLE_BRUSH_H_
#define _PARTICLE_BRUSH_H_

#include "kis_paint_device.h"
#include "kis_debug.h"
#include <PkPoint.h>

#include "KisParticleOpOptionData.h"


class KisParticleBrushProperties
{
public:
    quint16 particleCount;
    quint16 iterations;
    qreal weight;
    qreal gravity;
    PkPointF scale;
};

class KisRandomAccessor;
class KoColorSpace;
class KoColor;

class ParticleBrush
{

public:

    ParticleBrush();
    ~ParticleBrush();
    void initParticles();
    void draw(KisPaintDeviceSP dab, const KoColor& color, const PkPointF &pos);

    void setInitialPosition(const PkPointF &pos);
    void setProperties(KisParticleOpOptionData * properties) {
        m_properties = properties;
    }

private:
    /// paints wu particle, similar to spray version but you can turn on respecting opacity of the tool and add weight to opacity
    /// also the particle respects opacity in the destination pixel buffer
    void paintParticle(KisRandomAccessorSP writeAccessor, const KoColorSpace *cs,const PkPointF &pos, const KoColor& color, qreal weight, bool respectOpacity);

    PkVector<PkPointF> m_particlePos;
    PkVector<PkPointF> m_particleNextPos;
    PkVector<qreal> m_acceleration;

    KisParticleOpOptionData * m_properties;
};

#endif
