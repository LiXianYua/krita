/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _VANISHINGPOINT_ASSISTANT_H_
#define _VANISHINGPOINT_ASSISTANT_H_

#include "kis_painting_assistant.h"
#include <PkLine.h>
/* Design:
 *The idea behind the vanishing point ruler is that in a perspective deformed landscape, a set of parallel
 *lines al share a single vanishing point.
 *Therefore, a perspective can contain an theoretical infinite of vanishing points.
 *It's a pity if we only allowed an artist to access 1, 2 or 3 of these at any given time, as other
 *solutions for perspective tools do.
 *Hence a vanishing point ruler.
 *
 *This ruler is relatively simple compared to the other perspective ruler:
 *It has only one vanishing point that is required to draw.
 *However, it does have it's own weaknesses in how to determine onto which of these infinite rulers to snap.
 *Furthermore, it has four extra handles for adding a perspective ruler to a preexisting perspective.
 */
//class VanishingPoint;

class VanishingPointAssistant : public KisPaintingAssistant
{
public:
    VanishingPointAssistant();

    enum VanishingPointAssistantHandle {
        VanishingPointHandle,
        LocalFirstHandle,
        LocalSecondHandle
    };

    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;
    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return isLocal() ? 3 : 1; }

    float referenceLineDensity();
    void setReferenceLineDensity(float value);

    bool isAssistantComplete() const override;
    bool canBeLocal() const override;

    void saveCustomXml(PkXmlStreamWriter* xml) override;
    bool loadCustomXml(PkXmlStreamReader* xml) override;

protected:
    KisPaintingAssistantHandleSP firstLocalHandle() const override;
    KisPaintingAssistantHandleSP secondLocalHandle() const override;

private:


    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin, qreal moveThresholdPt);
    explicit VanishingPointAssistant(const VanishingPointAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);

    float m_referenceLineDensity {15.0};
};

class VanishingPointAssistantFactory : public KisPaintingAssistantFactory
{
public:
    VanishingPointAssistantFactory();
    ~VanishingPointAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
