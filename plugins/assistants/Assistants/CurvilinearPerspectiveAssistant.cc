/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "CurvilinearPerspectiveAssistant.h"


#include <PkTransform.h>

#include <kis_algebra_2d.h>

#include <math.h>
#include <limits>

CurvilinearPerspectiveAssistant::CurvilinearPerspectiveAssistant()
    : KisPaintingAssistant("curvilinear-perspective", PkString("Curvilinear Perspective assistant"))
{
}

CurvilinearPerspectiveAssistant::CurvilinearPerspectiveAssistant(const CurvilinearPerspectiveAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
{
}

KisPaintingAssistantSP CurvilinearPerspectiveAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new CurvilinearPerspectiveAssistant(*this, handleMap));
}

void CurvilinearPerspectiveAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = PkPointF();
    strokeBegin = PkPointF();
}



PkLineF CurvilinearPerspectiveAssistant::identifyCircle(const PkPointF thirdPoint) {
    /*
    * Calculate center location and radius for an arbitrary point (usually the mouse location).
    * Given Formulas:
    * Radius^2 = HalfHandleDist^2 + CenterDist^2
    * avgX + CenterDist * dirX = CenterX
    * avgY + CenterDist * dirY = CenterY
    * 
    * For ease of use, let BetaX = MouseX - AvgX, BetaY = MouseY - AvgY
    * Calculated Formula for CenterDist:
    * CenterDist = (BetaX^2 + BetaY^2 - HalfHandleDist^2) / (2 * DirY * BetaX + 2 * DirY * BetaY)
    * 
    * Returns line from center to the arbitrary point.
    * 
    */
    PkPointF p1 = *handles()[0];
    PkPointF p2 = *handles()[1];

    double deltaX = p2.x() - p1.x();
    double deltaY = p2.y() - p1.y();

    double handleDistance = KisAlgebra2D::norm(PkPointF(deltaX, deltaY));
    double halfHandleDist = handleDistance / 2.0;

    double avgX = deltaX / 2.0 + p1.x();
    double avgY = deltaY / 2.0 + p1.y();

    double dirX = -deltaY / handleDistance;
    double dirY = deltaX / handleDistance;

    double betaX = thirdPoint.x() - avgX;
    double betaY = thirdPoint.y() - avgY;

    double centerDist = 
        (pow2(betaX) + pow2(betaY) - pow2(halfHandleDist)) 
        / 
        (2 * dirX * betaX + 2 * dirY * betaY);
    
    double circleCenterX = centerDist*dirX + avgX;
    double circleCenterY = centerDist*dirY + avgY;
    return PkLineF(PkPointF(circleCenterX, circleCenterY), thirdPoint);
}

PkPointF CurvilinearPerspectiveAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal /*moveThresholdPt*/)
{
    // Get the center and radius for the given point
    PkLineF initialCircle = identifyCircle(strokeBegin);

    // Set the new point onto the circle.
    PkLineF magnetizedCircle(initialCircle.p1(), pt);
    magnetizedCircle.setLength(initialCircle.length());

    return magnetizedCircle.p2();

}

PkPointF CurvilinearPerspectiveAssistant::getDefaultEditorPosition() const
{
    return (*handles()[0] + *handles()[1]) * 0.5;
}

bool CurvilinearPerspectiveAssistant::isAssistantComplete() const
{
    return handles().size() >= 2;
}


CurvilinearPerspectiveAssistantFactory::CurvilinearPerspectiveAssistantFactory()
{
}

CurvilinearPerspectiveAssistantFactory::~CurvilinearPerspectiveAssistantFactory()
{
}

PkString CurvilinearPerspectiveAssistantFactory::id() const
{
    return "curvilinear-perspective";
}

PkString CurvilinearPerspectiveAssistantFactory::name() const
{
    return PkString("Curvilinear Perspective");
}

KisPaintingAssistant* CurvilinearPerspectiveAssistantFactory::createPaintingAssistant() const
{
    return new CurvilinearPerspectiveAssistant;
}
