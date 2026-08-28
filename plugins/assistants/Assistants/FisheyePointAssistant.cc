/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "FisheyePointAssistant.h"


#include <PkTransform.h>

#include <kis_algebra_2d.h>

#include <math.h>
#include <limits>

FisheyePointAssistant::FisheyePointAssistant()
    : KisPaintingAssistant("fisheye-point", PkString("Fish Eye Point assistant"))
{
}

FisheyePointAssistant::FisheyePointAssistant(const FisheyePointAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , e(rhs.e)
    , extraE(rhs.extraE)
{
}

KisPaintingAssistantSP FisheyePointAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new FisheyePointAssistant(*this, handleMap));
}

PkPointF FisheyePointAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin)
{
    const static PkPointF nullPoint(std::numeric_limits<qreal>::quiet_NaN(), std::numeric_limits<qreal>::quiet_NaN());
    assert(isAssistantComplete());
    e.set(*handles()[0], *handles()[1], *handles()[2]);

    //set the extrapolation ellipse.
    if (e.set(*handles()[0], *handles()[1], *handles()[2])){
        PkLineF radius(*handles()[1], *handles()[0]);
        radius.setAngle(fmod(radius.angle()+180.0,360.0));
        PkLineF radius2(*handles()[0], *handles()[1]);
        radius2.setAngle(fmod(radius2.angle()+180.0,360.0));
        if ( extraE.set(*handles()[0], *handles()[1],strokeBegin ) ) {
            return extraE.project(pt);
        } else if (extraE.set(radius.p1(), radius.p2(),strokeBegin)) {
            return extraE.project(pt);
        } else if (extraE.set(radius2.p1(), radius2.p2(),strokeBegin)){
            return extraE.project(pt);
        }
    }

    return nullPoint;

}

PkPointF FisheyePointAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal /*moveThresholdPt*/)
{
    return project(pt, strokeBegin);
}

void FisheyePointAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = PkPointF();
    strokeBegin = PkPointF();
}



PkRect FisheyePointAssistant::boundingRect() const
{
    if (!isAssistantComplete()) {
        return KisPaintingAssistant::boundingRect();
    }

    if (e.set(*handles()[0], *handles()[1], *handles()[2])) {
        return e.boundingRect().adjusted(-(e.semiMajor()*2), -2, (e.semiMajor()*2), 2).toAlignedRect();
    } else {
        return PkRect();
    }
}

PkPointF FisheyePointAssistant::getDefaultEditorPosition() const
{
    return (*handles()[0] + *handles()[1]) * 0.5;
}

bool FisheyePointAssistant::isAssistantComplete() const
{
    return handles().size() >= 3;
}


FisheyePointAssistantFactory::FisheyePointAssistantFactory()
{
}

FisheyePointAssistantFactory::~FisheyePointAssistantFactory()
{
}

PkString FisheyePointAssistantFactory::id() const
{
    return "fisheye-point";
}

PkString FisheyePointAssistantFactory::name() const
{
    return PkString("Fish Eye Point");
}

KisPaintingAssistant* FisheyePointAssistantFactory::createPaintingAssistant() const
{
    return new FisheyePointAssistant;
}
