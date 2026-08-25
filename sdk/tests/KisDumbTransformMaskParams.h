/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISDUMBTRANSFORMMASKPARAMS_H
#define KISDUMBTRANSFORMMASKPARAMS_H

#include "kritatransformmaskstubs_export.h"
#include "kis_transform_mask_params_interface.h"

#include <PkTransform.h>
#include <PkRect.h>
#include <PkString.h>
#include <PkScopedPointer.h>

class PkXmlElement;

class KRITATRANSFORMMASKSTUBS_EXPORT KisDumbTransformMaskParams : public KisTransformMaskParamsInterface
{
public:
    KisDumbTransformMaskParams();
    KisDumbTransformMaskParams(const PkTransform &transform);
    KisDumbTransformMaskParams(bool isHidden);
    ~KisDumbTransformMaskParams() override;


    PkTransform finalAffineTransform() const override;
    bool isAffine() const override;
    bool isHidden() const override;
    void setHidden(bool value) override;
    void transformDevice(KisNodeSP node, KisPaintDeviceSP src, KisPaintDeviceSP dst, bool forceSubPixelTranslation) const override;

    PkString id() const override;
    void toXML(PkXmlElement *e) const override;
    static KisTransformMaskParamsInterfaceSP fromXML(const PkXmlElement &e);

    void translateSrcAndDst(const PkPointF &offset) override;
    void transformSrcAndDst(const PkTransform &t) override;
    void translateDstSpace(const PkPointF &offset) override;

    // for testing purposes only
    PkTransform testingGetTransform() const;
    void testingSetTransform(const PkTransform &t);

    PkRect nonAffineChangeRect(const PkRect &rc) override;
    PkRect nonAffineNeedRect(const PkRect &rc, const PkRect &srcBounds) override;

    bool isAnimated() const;
    KisKeyframeChannel *getKeyframeChannel(const PkString &id, KisDefaultBoundsBaseSP defaultBounds);

    KisTransformMaskParamsInterfaceSP clone() const override;

    bool compareTransform(KisTransformMaskParamsInterfaceSP rhs) const override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KISDUMBTRANSFORMMASKPARAMS_H
