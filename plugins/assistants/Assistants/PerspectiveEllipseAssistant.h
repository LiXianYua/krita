/*
 * SPDX-FileCopyrightText: 2022 Srirupa Datta <srirupa.sps@gmail.com>
 */

#ifndef _PERSPECTIVE_ELLIPSE_ASSISTANT_H_
#define _PERSPECTIVE_ELLIPSE_ASSISTANT_H_

#include "kis_abstract_perspective_grid.h"
#include "kis_painting_assistant.h"
#include "Ellipse.h"
#include <PkScopedPointer.h>

class PerspectiveEllipseAssistant : public KisAbstractPerspectiveGrid, public KisPaintingAssistant
{
public:
    PerspectiveEllipseAssistant();
    ~PerspectiveEllipseAssistant();


    KisPaintingAssistantSP clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const override;
    PkPointF adjustPosition(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt) override;
    void adjustLine(PkPointF &point, PkPointF& strokeBegin) override;
    
    PkPointF getDefaultEditorPosition() const override;
    int numHandles() const override { return 4; }
    bool isAssistantComplete() const override;

    // implements KisAbstractPerspectiveGrid
    bool contains(const PkPointF& point) const override;
    qreal distance(const PkPointF& point) const override;
    bool isActive() const  override;
    
protected:
    PkRect boundingRect() const override;
private:
    PkPointF project(const PkPointF& pt, const PkPointF& strokeBegin);

    // finds the transform from perspective coordinates (a unit square) to the document
    bool getTransform(PkPolygonF& polyOut, PkTransform& transformOut);


    bool isEllipseValid();
    void updateCache();


     
    explicit PerspectiveEllipseAssistant(const PerspectiveEllipseAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap);


    class Private;
    PkScopedPointer<Private> d;
    
};

class PerspectiveEllipseAssistantFactory : public KisPaintingAssistantFactory
{
public:
    PerspectiveEllipseAssistantFactory();
    ~PerspectiveEllipseAssistantFactory() override;
    PkString id() const override;
    PkString name() const override;
    KisPaintingAssistant* createPaintingAssistant() const override;
};

#endif
