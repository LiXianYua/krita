/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_LS_SATIN_FILTER_H
#define KIS_LS_SATIN_FILTER_H


#include "kis_layer_style_filter.h"
#include "kis_psd_layer_style.h"
#include <kritaimage_export.h>



class KRITAIMAGE_EXPORT KisLsSatinFilter : public KisLayerStyleFilter
{
public:
    KisLsSatinFilter();

    KisLayerStyleFilter* clone() const override;

    void processDirectly(KisPaintDeviceSP src,
                         KisMultipleProjection *dst,
                         KisLayerStyleKnockoutBlower *blower,
                         const PkRect &applyRect,
                         KisPSDLayerStyleSP style,
                         KisLayerStyleFilterEnvironment *env) const override;

    PkRect neededRect(const PkRect & rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const override;
    PkRect changedRect(const PkRect & rect, KisPSDLayerStyleSP style, KisLayerStyleFilterEnvironment *env) const override;

private:
    KisLsSatinFilter(const KisLsSatinFilter &rhs);


    void applySatin(KisPaintDeviceSP srcDevice,
                    KisMultipleProjection *dst,
                    const PkRect &applyRect,
                    const psd_layer_effects_context *context,
                    const psd_layer_effects_satin *config, KisResourcesInterfaceSP resourcesInterface,
                    KisLayerStyleFilterEnvironment *env) const;
};

#endif
