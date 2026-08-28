/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _PARALLELRULER_ASSISTANT_H_
#define _PARALLELRULER_ASSISTANT_H_

#include "kis_painting_assistant.h"
#include <PkLine.h>
/* Design:
 */
class ParallelRuler;

class ParallelRulerAssistant : public KisPaintingAssistant
{
public:
    ParallelRulerAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;
    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return isLocal() ? 4 : 2; }
    bool isAssistantComplete() const override;
    bool canBeLocal() const override;

    void saveCustomXml(PkXmlStreamWriter* xml) override;
    bool loadCustomXml(PkXmlStreamReader* xml) override;

protected:
    KisPaintingAssistantHandleSP firstLocalHandle() const override;
    KisPaintingAssistantHandleSP secondLocalHandle() const override;

private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin, qreal moveThresholdPt);
    explicit ParallelRulerAssistant(const ParallelRulerAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);

};

class ParallelRulerAssistantFactory : public KisPaintingAssistantFactory
{
public:
    ParallelRulerAssistantFactory();
    ~ParallelRulerAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
