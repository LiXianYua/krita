/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDumbTransformMaskParams.h"

#include <PkXmlElement.h>
#include <PkXmlDocument.h>
#include "kis_algebra_2d.h"
#include "kis_dom_utils.h"
#include "kis_node.h"
#include "kis_painter.h"
#include <kis_perspectivetransform_worker.h>

struct Q_DECL_HIDDEN KisDumbTransformMaskParams::Private
{
    Private() : isHidden(false) {}

    PkTransform transform;
    bool isHidden;
};

KisDumbTransformMaskParams::KisDumbTransformMaskParams()
    : m_d(new Private)
{
}

KisDumbTransformMaskParams::KisDumbTransformMaskParams(const PkTransform &transform)
    : m_d(new Private)
{
    m_d->isHidden = false;
    m_d->transform = transform;
}

KisDumbTransformMaskParams::KisDumbTransformMaskParams(bool isHidden)
    : m_d(new Private)
{
    m_d->isHidden = isHidden;
}

KisDumbTransformMaskParams::~KisDumbTransformMaskParams()
{
}

PkTransform KisDumbTransformMaskParams::finalAffineTransform() const
{
    return m_d->transform;
}

bool KisDumbTransformMaskParams::isAffine() const
{
    return true;
}

bool KisDumbTransformMaskParams::isHidden() const
{
    return m_d->isHidden;
}

void KisDumbTransformMaskParams::setHidden(bool value)
{
    m_d->isHidden = value;
}

void KisDumbTransformMaskParams::transformDevice(KisNodeSP node, KisPaintDeviceSP src, KisPaintDeviceSP dst, bool forceSubPixelTranslation) const
{
    Q_UNUSED(node);
    Q_UNUSED(forceSubPixelTranslation);

    PkRect rc = src->exactBounds();
    PkPoint dstTopLeft = rc.topLeft();

    PkTransform t = finalAffineTransform();
    if (t.type() <= PkTransform::TxTranslate) {
        dstTopLeft = t.map(dstTopLeft);
        KisPainter::copyAreaOptimized(dstTopLeft, src, dst, rc);
    } else if (!t.isIdentity()) {
        KisPerspectiveTransformWorker worker(nullptr, PkTransform(), true, 0);
        worker.setForceSubPixelTranslation(forceSubPixelTranslation);
        worker.setForwardTransform(t);
        worker.runPartialDst(src, dst, src->defaultBounds()->bounds());
    } else {
        KisPainter::copyAreaOptimized(dstTopLeft, src, dst, rc);
    }
}

PkString KisDumbTransformMaskParams::id() const
{
    return "dumbparams";
}

void KisDumbTransformMaskParams::toXML(PkXmlElement *e) const
{
    PkXmlDocument doc = e->ownerDocument();
    PkXmlElement transformEl = doc.createElement("dumb_transform");
    e->appendChild(transformEl);

    KisDomUtils::saveValue(&transformEl, "transform", m_d->transform);
}

KisTransformMaskParamsInterfaceSP KisDumbTransformMaskParams::fromXML(const PkXmlElement &e)
{
    PkXmlElement transformEl;
    bool result = false;

    PkTransform transform;

    result =
        KisDomUtils::findOnlyElement(e, "dumb_transform", &transformEl) &&
        KisDomUtils::loadValue(transformEl, "transform", &transform);

    if (!result) {
        warnKrita << "WARNING: couldn't load dumb transform. Ignoring...";
    }

    return KisTransformMaskParamsInterfaceSP(
        new KisDumbTransformMaskParams(transform));
}

void KisDumbTransformMaskParams::translateSrcAndDst(const PkPointF &offset)
{
    Q_UNUSED(offset);

    /**
     * Normal translation doesn't change affine transformations
     * in full-featured KisTransformMaskAdapter, so we should resemble
     * this behavior in the dumb one
     */
}

void KisDumbTransformMaskParams::transformSrcAndDst(const PkTransform &t)
{
    Q_UNUSED(t);

    /**
     * Normal translation doesn't change affine transformations
     * in full-featured KisTransformMaskAdapter, so we should resemble
     * this behavior in the dumb one
     */
}

void KisDumbTransformMaskParams::translateDstSpace(const PkPointF &offset)
{
    m_d->transform.translate(offset.x(), offset.y());
}

PkRect KisDumbTransformMaskParams::nonAffineChangeRect(const PkRect &rc)
{
    return rc;
}

PkRect KisDumbTransformMaskParams::nonAffineNeedRect(const PkRect &rc, const PkRect &srcBounds)
{
    Q_UNUSED(srcBounds);
    return rc;
}

bool KisDumbTransformMaskParams::isAnimated() const
{
    return false;
}

KisKeyframeChannel *KisDumbTransformMaskParams::getKeyframeChannel(const PkString&, KisDefaultBoundsBaseSP)
{
    return 0;
}

KisTransformMaskParamsInterfaceSP KisDumbTransformMaskParams::clone() const
{
    return toQShared(new KisDumbTransformMaskParams(m_d->transform));
}

bool KisDumbTransformMaskParams::compareTransform(KisTransformMaskParamsInterfaceSP rhs) const
{
    PkSharedPointer<KisDumbTransformMaskParams> rhsParams =
            rhs.dynamicCast<KisDumbTransformMaskParams>();

    return KisAlgebra2D::fuzzyMatrixCompare(m_d->transform, rhsParams->m_d->transform, 1e-5);
}

PkTransform KisDumbTransformMaskParams::testingGetTransform() const
{
    return m_d->transform;
}

void KisDumbTransformMaskParams::testingSetTransform(const PkTransform &t)
{
    m_d->transform = t;
}
