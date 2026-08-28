/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2021 Nabil Maghfur Usman <nmaghfurusman@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _TWO_POINT_ASSISTANT_H_
#define _TWO_POINT_ASSISTANT_H_

#include "kis_painting_assistant.h"
#include <PkLine.h>
#include <PkTransform.h>

class TwoPointAssistant : public KisPaintingAssistant
{
public:

    enum TwoPointHandle {
        FirstHandle,
        SecondHandle,
        VerticalHandle,
        LocalFirstHandle,
        LocalSecondHandle
    };


    TwoPointAssistant();
    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    void endStroke() override;
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;

    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return isLocal() ? 5 : 3; }

    void saveCustomXml(PkXmlStreamWriter* xml) override;
    bool loadCustomXml(PkXmlStreamReader* xml) override;

    double gridDensity();
    void setGridDensity(double density);

    /* If true, it means the assistant will have three handles
     * If false,
     * */
    bool useVertical();
    void setUseVertical(bool value);

    bool isAssistantComplete() const override;
    bool canBeLocal() const override;

    /* Generate a transform for converting handles into easier local
       coordinate system that has the following properties:
       - Rotated so horizon is perfectly horizontal
       - Translated so 3rd handle is the origin
       Parameters are the first VP, second VP, a 3rd point which
       defines the center of vision, and lastly a reference to a size
       variable which is the radius of the 90 degree cone of vision
       (useful for computing snapping behaviour and drawing grid
       lines) */
    PkTransform localTransform(PkPointF vp_a, PkPointF vp_b, PkPointF pt_c, qreal* size);

protected:
    KisPaintingAssistantHandleSP firstLocalHandle() const override;
    KisPaintingAssistantHandleSP secondLocalHandle() const override;


private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThreshold);
    explicit TwoPointAssistant(const TwoPointAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);
    PkLineF m_snapLine;
    double m_gridDensity {1.0};
    bool m_useVertical {true};

    int m_lastUsedPoint {-1}; // last used vanishing point

};

class TwoPointAssistantFactory : public KisPaintingAssistantFactory
{
public:
    TwoPointAssistantFactory();
    ~TwoPointAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
