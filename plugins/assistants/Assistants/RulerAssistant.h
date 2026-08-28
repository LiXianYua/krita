/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-FileCopyrightText: 2022 Julian Schmidt <julisch1107@web.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _RULER_ASSISTANT_H_
#define _RULER_ASSISTANT_H_

#include <PkMap.h>

#include "kis_painting_assistant.h"

class Ruler;

class RulerAssistant : public KisPaintingAssistant
{
public:
    RulerAssistant();
    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;
    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 2; }
    bool isAssistantComplete() const override;
    void saveCustomXml(PkXmlStreamWriter *xml) override;
    bool loadCustomXml(PkXmlStreamReader *xml) override;
    
    int subdivisions() const;
    void setSubdivisions(int subdivisions);
    int minorSubdivisions() const;
    void setMinorSubdivisions(int subdivisions);
    bool hasFixedLength() const;
    void enableFixedLength(bool enabled);
    qreal fixedLength() const;
    void setFixedLength(qreal length);
    PkString fixedLengthUnit() const;
    void setFixedLengthUnit(PkString unit);
    
    void ensureLength();

protected:
    explicit RulerAssistant(const PkString& id, const PkString& name);
    explicit RulerAssistant(const RulerAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);

  private:
    PkPointF project(const PkPointF& pt) const;
    int m_subdivisions {0};
    int m_minorSubdivisions {0};
    bool m_hasFixedLength {false};
    qreal m_fixedLength {0.0};
    PkString m_fixedLengthUnit {"px"};
};

class RulerAssistantFactory : public KisPaintingAssistantFactory
{
public:
    RulerAssistantFactory();
    ~RulerAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
