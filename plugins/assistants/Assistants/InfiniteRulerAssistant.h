/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-FileCopyrightText: 2022 Julian Schmidt <julisch1107@web.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _INFINITERULER_ASSISTANT_H_
#define _INFINITERULER_ASSISTANT_H_

#include "RulerAssistant.h"

#include <PkLine.h>



class InfiniteRulerAssistant : public RulerAssistant
{
public:
    InfiniteRulerAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;
    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 2; }
    bool isAssistantComplete() const override;

private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin, const bool checkForInitialMovement, qreal moveThresholdPt);
    explicit InfiniteRulerAssistant(const InfiniteRulerAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);
  
    // Helper struct for clipLineParametric's return type
    struct ClippingResult {
        bool intersects;
        qreal tmin;
        qreal tmax;
    };
    // Like KisAlgebra2D::clipLineRect, but returns the parametric positions
    static ClippingResult clipLineParametric(PkLineF line, PkRectF rect, bool extendFirst=true, bool extendSecond=true);
};

class InfiniteRulerAssistantFactory : public KisPaintingAssistantFactory
{
public:
    InfiniteRulerAssistantFactory();
    ~InfiniteRulerAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
