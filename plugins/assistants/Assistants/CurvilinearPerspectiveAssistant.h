/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _CURVILINEARPERSPECTIVE_ASSISTANT_H_
#define _CURVILINEARPERSPECTIVE_ASSISTANT_H_

#include "kis_painting_assistant.h"
#include <PkLine.h>
//class CurvilinearPerspective;

class CurvilinearPerspectiveAssistant : public KisPaintingAssistant
{
public:
    CurvilinearPerspectiveAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;

    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;

    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 2; }

    bool isAssistantComplete() const override;

private:
    PkLineF identifyCircle(const PkPointF thirdPoint);
    explicit CurvilinearPerspectiveAssistant(const CurvilinearPerspectiveAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);
};

class CurvilinearPerspectiveAssistantFactory : public KisPaintingAssistantFactory
{
public:
    CurvilinearPerspectiveAssistantFactory();
    ~CurvilinearPerspectiveAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
