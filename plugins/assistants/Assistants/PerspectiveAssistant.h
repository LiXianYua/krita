/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _PERSPECTIVE_ASSISTANT_H_
#define _PERSPECTIVE_ASSISTANT_H_

#include "kis_abstract_perspective_grid.h"
#include "kis_painting_assistant.h"
#include <PkPolygon.h>
#include <PkLine.h>
#include <PkTransform.h>

#include <PerspectiveBasedAssistantHelper.h>

class PerspectiveAssistant : public KisAbstractPerspectiveGrid, public KisPaintingAssistant
{
public:
    PerspectiveAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;

    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    void endStroke() override;

    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 4; }

    bool contains(const PkPointF& point) const override;
    qreal distance(const PkPointF& point) const override;
    bool isActive() const override;

    int subdivisions() const;
    void setSubdivisions(int subdivisions);

    bool isAssistantComplete() const override;

    void saveCustomXml(PkXmlStreamWriter *xml) override;
    bool loadCustomXml(PkXmlStreamReader *xml) override;

private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin, const bool snapToAnyDirection, qreal moveThresholdPt);
    // creates the convex hull, returns false if it's not a quadrilateral
    // finds the transform from perspective coordinates (a unit square) to the document
    bool getTransform(PkPolygonF& polyOut, PkTransform& transformOut) const;
    explicit PerspectiveAssistant(const PerspectiveAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);

    // The number of subdivisions to draw
    int m_subdivisions {8};
    // which direction to snap to (in transformed coordinates)
    PkLineF m_snapLine;
    // cached information
    mutable PkTransform m_cachedTransform;
    mutable PkPolygonF m_cachedPolygon;
    mutable PkPointF m_cachedPoints[4];
    mutable bool m_cacheValid {false};

    mutable PerspectiveBasedAssistantHelper::CacheData m_cache;

};

class PerspectiveAssistantFactory : public KisPaintingAssistantFactory
{
public:
    PerspectiveAssistantFactory();
    ~PerspectiveAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
