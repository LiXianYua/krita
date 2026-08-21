/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LAYER_STYLE_FILTER_H
#define __KIS_LAYER_STYLE_FILTER_H

#include "kis_types.h"
#include "kis_shared.h"
#include "kritaimage_export.h"
#include <PkSharedPointer.h>

class KisPSDLayerStyle;
typedef PkSharedPointer<KisPSDLayerStyle> KisPSDLayerStyleSP;
#include <PkScopedPointer.h>

class KisLayerStyleFilterEnvironment;
class KisMultipleProjection;
class KisLayerStyleKnockoutBlower;

class KRITAIMAGE_EXPORT KisLayerStyleFilter : public KisShared
{
public:
    KisLayerStyleFilter(const KoID &id);
    virtual ~KisLayerStyleFilter();

    /**
     * \return Unique identifier for this filter
     */
    PkString id() const;

    virtual KisLayerStyleFilter* clone() const = 0;

    virtual void processDirectly(KisPaintDeviceSP src,
                                 KisMultipleProjection *dst,
                                 KisLayerStyleKnockoutBlower *blower,
                                 const PkRect &applyRect,
                                 KisPSDLayerStyleSP style,
                                 KisLayerStyleFilterEnvironment *env) const = 0;

    /**
     * Some filters need pixels outside the current processing rect to compute the new
     * value (for instance, convolution filters)
     */
    virtual PkRect neededRect(const PkRect & rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const = 0;

    /**
     * Similar to \ref neededRect: some filters will alter a lot of pixels that are
     * near to each other at the same time. So when you changed a single rectangle
     * in a device, the actual rectangle that will feel the influence of this change
     * might be bigger. Use this function to determine that rect.
     */
    virtual PkRect changedRect(const PkRect & rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const = 0;

protected:
    KisLayerStyleFilter(const KisLayerStyleFilter &rhs);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_LAYER_STYLE_FILTER_H */
