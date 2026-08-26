/*
 *  SPDX-FileCopyrightText: 2005 Bart Coppens <kde@bartcoppens.be>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_boundary.h"
#include <PkPolygon.h>
#include <PkPainterPath.h>

#include "KoColorSpace.h"
#include "kis_fixed_paint_device.h"
#include "kis_outline_generator.h"

// PkPainterPath::addPolygon 只收 PkPolygonF（浮点），这里把 int 坐标的
// PkPolygon 显式转一次（真 Qt 里 addPolygon(PkPolygon) 走隐式 PkPolygon→PkPolygonF
// 转换；同 libs/flake/text/KisTofuGlyph.cpp 的 toPolygonF 处置）。
static PkPolygonF toPolygonF(const PkPolygon &poly)
{
    PkVector<PkPointF> pts;
    for (const PkPoint &pt : poly) {
        pts.push_back(PkPointF(pt.x(), pt.y()));
    }
    return PkPolygonF(pts);
}

struct KisBoundary::Private {
    KisFixedPaintDeviceSP m_device;
    PkVector<PkPolygon> m_boundary;
    PkPainterPath path;
};

KisBoundary::KisBoundary(KisFixedPaintDeviceSP dev) : d(new Private)
{
    d->m_device = dev;
}

KisBoundary::~KisBoundary()
{
    delete d;
}

void KisBoundary::generateBoundary()
{
    if (!d->m_device)
        return;

    KisOutlineGenerator generator(d->m_device->colorSpace(), OPACITY_TRANSPARENT_U8);
    generator.setSimpleOutline(true);
    d->m_boundary = generator.outline(d->m_device->data(), 0, 0, d->m_device->bounds().width(), d->m_device->bounds().height());

    d->path = PkPainterPath();
    for (const PkPolygon &polygon : d->m_boundary) {
        d->path.addPolygon(toPolygonF(polygon));
        d->path.closeSubpath();
    }

}

PkPainterPath KisBoundary::path() const
{
    return d->path;
}

