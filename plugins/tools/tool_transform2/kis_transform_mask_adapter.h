/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_TRANSFORM_MASK_ADAPTER_H
#define __KIS_TRANSFORM_MASK_ADAPTER_H

#include <PkScopedPointer.h>
#include "kis_transform_mask_params_interface.h"
#include "kritatooltransform_export.h"

class ToolTransformArgs;


class KRITATOOLTRANSFORM_EXPORT KisTransformMaskAdapter : public KisTransformMaskParamsInterface
{
public:
    KisTransformMaskAdapter();
    KisTransformMaskAdapter(const ToolTransformArgs &args, bool isHidden = false, bool isInitialized = true);
    ~KisTransformMaskAdapter() override;

    PkTransform finalAffineTransform() const override;
    bool isAffine() const override;

    bool isInitialized() const;

    void setHidden(bool value) override;
    bool isHidden() const override;

    void transformDevice(KisNodeSP node, KisPaintDeviceSP src, KisPaintDeviceSP dst, bool forceSubPixelTranslation) const override;

    virtual const PkSharedPointer<ToolTransformArgs> transformArgs() const;
    void setBaseArgs(const ToolTransformArgs& args);

    PkString id() const override;
    void toXML(PkXmlElement *e) const override;
    static KisTransformMaskParamsInterfaceSP fromXML(const PkXmlElement &e);
    static KisTransformMaskParamsInterfaceSP fromDumbXML(const PkXmlElement &e);

    void translateSrcAndDst(const PkPointF &offset) override;
    void transformSrcAndDst(const PkTransform &t) override;
    void translateDstSpace(const PkPointF &offset) override;


    PkRect nonAffineChangeRect(const PkRect &rc) override;
    PkRect nonAffineNeedRect(const PkRect &rc, const PkRect &srcBounds) override;

    KisKeyframeChannel *getKeyframeChannel(const PkString &id, KisDefaultBoundsBaseSP defaultBounds);

    KisTransformMaskParamsInterfaceSP clone() const override;

    bool compareTransform(KisTransformMaskParamsInterfaceSP rhs) const override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_TRANSFORM_MASK_ADAPTER_H */
