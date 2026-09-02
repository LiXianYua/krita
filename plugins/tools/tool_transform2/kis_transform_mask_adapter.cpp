/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_transform_mask_adapter.h"

#include <PkTransform.h>
#include <PkXmlElement.h>
#include "kis_dom_utils.h"

#include "tool_transform_args.h"
#include "kis_transform_utils.h"
#include "KisAnimatedTransformMaskParamsHolder.h"

#include "kis_node.h"


struct KisTransformMaskAdapter::Private
{
    PkSharedPointer<ToolTransformArgs> args;
    bool isHidden {false};
    bool isInitialized {true};
};


KisTransformMaskAdapter::KisTransformMaskAdapter()
    : m_d(new Private)
{
    m_d->args.reset(new ToolTransformArgs());
}

KisTransformMaskAdapter::KisTransformMaskAdapter(const ToolTransformArgs &args, bool isHidden, bool isInitialized)
    : m_d(new Private)
{
    m_d->args = toQShared(new ToolTransformArgs(args));
    m_d->isHidden = isHidden;
    m_d->isInitialized = isInitialized;
}

KisTransformMaskAdapter::~KisTransformMaskAdapter()
{
}

PkTransform KisTransformMaskAdapter::finalAffineTransform() const
{
    KisTransformUtils::MatricesPack m(*transformArgs());
    return m.finalTransform();
}

bool KisTransformMaskAdapter::isAffine() const
{
    const ToolTransformArgs args = *transformArgs();

    return args.mode() == ToolTransformArgs::FREE_TRANSFORM ||
            args.mode() == ToolTransformArgs::PERSPECTIVE_4POINT;
}

bool KisTransformMaskAdapter::isInitialized() const
{
    return m_d->isInitialized;
}

void KisTransformMaskAdapter::setHidden(bool value)
{
    m_d->isHidden = value;
}

bool KisTransformMaskAdapter::isHidden() const
{
    return m_d->isHidden;
}

void KisTransformMaskAdapter::transformDevice(KisNodeSP node, KisPaintDeviceSP src, KisPaintDeviceSP dst, bool forceSubPixelTranslation) const
{
    dst->prepareClone(src);

    KisProcessingVisitor::ProgressHelper helper(node);

    KisTransformUtils::transformDeviceWithCroppedDst(*transformArgs(), src, dst, &helper, forceSubPixelTranslation);
}

const PkSharedPointer<ToolTransformArgs> KisTransformMaskAdapter::transformArgs() const {
    return m_d->args;
}

void KisTransformMaskAdapter::setBaseArgs(const ToolTransformArgs &args)
{
    *m_d->args = args;
}

PkString KisTransformMaskAdapter::id() const
{
    return "tooltransformparams";
}

void KisTransformMaskAdapter::toXML(PkXmlElement *e) const
{
    // bounds rotation cannot be used on transform masks currently
    KIS_SAFE_ASSERT_RECOVER_NOOP(qFuzzyIsNull(m_d->args->boundsRotation()));
    m_d->args->toXML(e);
}

KisTransformMaskParamsInterfaceSP KisTransformMaskAdapter::fromXML(const PkXmlElement &e)
{
    ToolTransformArgs args(ToolTransformArgs::fromXML(e));

    // bounds rotation cannot be used on transform masks currently
    KIS_SAFE_ASSERT_RECOVER_NOOP(qFuzzyIsNull(args.boundsRotation()));

    return KisTransformMaskParamsInterfaceSP(
        new KisTransformMaskAdapter(args));
}

KisTransformMaskParamsInterfaceSP KisTransformMaskAdapter::fromDumbXML(const PkXmlElement &e)
{
    ToolTransformArgs args;

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

        args.translateDstSpace(PkPointF(transform.dx(), transform.dy()));
    }

    // bounds rotation cannot be used on transform masks currently
    KIS_SAFE_ASSERT_RECOVER_NOOP(qFuzzyIsNull(args.boundsRotation()));

    return KisTransformMaskParamsInterfaceSP(
        new KisTransformMaskAdapter(args));
}

void KisTransformMaskAdapter::translateSrcAndDst(const PkPointF &offset)
{
    m_d->args->translateSrcAndDst(offset);
}

void KisTransformMaskAdapter::transformSrcAndDst(const PkTransform &t)
{
    m_d->args->transformSrcAndDst(t);
}

void KisTransformMaskAdapter::translateDstSpace(const PkPointF &offset)
{
    m_d->args->translateDstSpace(offset);
}

PkRect KisTransformMaskAdapter::nonAffineChangeRect(const PkRect &rc)
{
    return KisTransformUtils::changeRect(*transformArgs(), rc);
}

PkRect KisTransformMaskAdapter::nonAffineNeedRect(const PkRect &rc, const PkRect &srcBounds)
{
    return KisTransformUtils::needRect(*transformArgs(), rc, srcBounds);
}

KisKeyframeChannel *KisTransformMaskAdapter::getKeyframeChannel(const PkString &id, KisDefaultBoundsBaseSP defaultBounds)
{
    (void)id;
    (void)defaultBounds;
    return 0;
}

KisTransformMaskParamsInterfaceSP KisTransformMaskAdapter::clone() const {
    return toQShared(new KisTransformMaskAdapter(*this->transformArgs(), this->isHidden(), this->isInitialized()));
}

bool KisTransformMaskAdapter::compareTransform(KisTransformMaskParamsInterfaceSP rhs) const
{
    PkSharedPointer<KisTransformMaskAdapter> adapter = rhs.dynamicCast<KisTransformMaskAdapter>();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(adapter, false);

    PkSharedPointer<ToolTransformArgs> lhsArgs = transformArgs();
    PkSharedPointer<ToolTransformArgs> rhsArgs = adapter->transformArgs();

    return lhsArgs->isSameMode(*rhsArgs);
}
