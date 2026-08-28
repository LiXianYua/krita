/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _FISHEYEPOINT_ASSISTANT_H_
#define _FISHEYEPOINT_ASSISTANT_H_

#include "kis_painting_assistant.h"
#include "Ellipse.h"
#include <PkLine.h>
//class FisheyePoint;

class FisheyePointAssistant : public KisPaintingAssistant
{
public:
    FisheyePointAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;

    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;

    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 3; }

    bool isAssistantComplete() const override;

protected:
    PkRect boundingRect() const override;
private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin);
    explicit FisheyePointAssistant(const FisheyePointAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);
    mutable Ellipse e;
    mutable Ellipse extraE;
};

class FisheyePointAssistantFactory : public KisPaintingAssistantFactory
{
public:
    FisheyePointAssistantFactory();
    ~FisheyePointAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
