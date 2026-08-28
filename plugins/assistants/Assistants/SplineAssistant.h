/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _SPLINE_ASSISTANT_H_
#define _SPLINE_ASSISTANT_H_

#include "kis_painting_assistant.h"
#include <PkScopedPointer.h>

class SplineAssistant : public KisPaintingAssistant
{
public:
    SplineAssistant();
    ~SplineAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;
    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 4; }
    bool isAssistantComplete() const override;

private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin) const;
    explicit SplineAssistant(const SplineAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

class SplineAssistantFactory : public KisPaintingAssistantFactory
{
public:
    SplineAssistantFactory();
    ~SplineAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
