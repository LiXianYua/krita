/*
 *  SPDX-FileCopyrightText: 2008-2009 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_MASK_GENERATOR_H_
#define _KIS_MASK_GENERATOR_H_

#include <PkGlobal.h>
#include <PkList.h>
#include <PkScopedPointer.h>
#include <PkContainerAlgo.h>
#include <PkString.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include <KoID.h>
#include "kritaimage_export.h"

class PkXmlElement;
class PkXmlDocument;
class KisBrushMaskApplicatorBase;

const KoID DefaultId("default", PkString("Default")); ///< generate Krita default mask generator
const KoID SoftId("soft", PkString("Soft")); ///< generate brush mask from former softbrush paintop, where softness is based on curve
const KoID GaussId("gauss", PkString("Gaussian")); ///< generate brush mask with a Gaussian-blurred edge

static const int OVERSAMPLING = 4;

/**
 * This is the base class for mask shapes
 * You should subclass it if you want to create a new
 * shape.
 */
class KRITAIMAGE_EXPORT KisMaskGenerator
{
public:
    enum Type {
        CIRCLE, RECTANGLE
    };
public:

    /**
     * This function creates an auto brush shape with the following values:
     * @param radius radius
     * @param ratio aspect ratio
     * @param fh horizontal fade
     * @param fv vertical fade
     * @param spikes number of spikes
     * @param antialiasEdges whether to antialias edges
     * @param type type
     * @param id the brush identifier
     */
    KisMaskGenerator(qreal radius, qreal ratio, qreal fh, qreal fv, int spikes, bool antialiasEdges, Type type, const KoID& id = DefaultId);
    KisMaskGenerator(const KisMaskGenerator &rhs);

    virtual ~KisMaskGenerator();

    virtual KisMaskGenerator* clone() const = 0;

private:

    void init();

public:
    /**
     * @return the alpha value at the position (x,y)
     */
    virtual quint8 valueAt(qreal x, qreal y) const = 0;

    virtual bool shouldSupersample() const;

    virtual bool shouldSupersample6x6() const;

    virtual bool shouldVectorize() const;

    virtual KisBrushMaskApplicatorBase *applicator() const = 0;

    virtual void toXML(PkXmlDocument& , PkXmlElement&) const;

    /**
     * Unserialise a \ref KisMaskGenerator
     */
    static KisMaskGenerator* fromXML(const PkXmlElement&);

    qreal width() const;

    qreal height() const;

    qreal diameter() const;    
    void setDiameter(qreal value);

    qreal ratio() const;
    qreal horizontalFade() const;
    qreal verticalFade() const;
    int spikes() const;
    Type type() const;
    bool isEmpty() const;
    void fixRotation(qreal &xr, qreal &yr) const;
    
    inline PkString id() const { return m_id.id(); }
    inline PkString name() const { return m_id.name(); }

    static PkList<KoID> maskGeneratorIds();
    
    qreal softness() const;
    virtual void setSoftness(qreal softness);
    
    PkString curveString() const;
    void setCurveString(const PkString& curveString);

    bool antialiasEdges() const;
    virtual void setScale(qreal scaleX, qreal scaleY);

protected:
    qreal effectiveSrcWidth() const;
    qreal effectiveSrcHeight() const;

private:
    struct Private;
    const PkScopedPointer<Private> d;
    const KoID& m_id;
};

#endif
