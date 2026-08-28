/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _CONCENTRIC_ELLIPSE_ASSISTANT_H_
#define _CONCENTRIC_ELLIPSE_ASSISTANT_H_

#include "kis_painting_assistant.h"
#include "Ellipse.h"
#include <PkLine.h>

#include "kritaassistanttool_export.h"

class KRITAASSISTANTTOOL_EXPORT ConcentricEllipseAssistant
    : public KisPaintingAssistant
{
public:
    ConcentricEllipseAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;

    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;

    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 3; }
    bool isAssistantComplete() const override;

    void transform(const PkTransform &transform) override;


protected:
    PkRect boundingRect() const override;
private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin) const;
    mutable Ellipse m_ellipse;
    explicit ConcentricEllipseAssistant(const ConcentricEllipseAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);
};

class KRITAASSISTANTTOOL_EXPORT ConcentricEllipseAssistantFactory
    : public KisPaintingAssistantFactory
{
public:
    ConcentricEllipseAssistantFactory();
    ~ConcentricEllipseAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
