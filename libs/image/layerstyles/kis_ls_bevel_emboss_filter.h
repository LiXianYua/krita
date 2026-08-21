/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_LS_BEVEL_EMBOSS_FILTER_H
#define KIS_LS_BEVEL_EMBOSS_FILTER_H


#include "kis_layer_style_filter.h"
#include "kis_psd_layer_style.h"
#include <kritaimage_export.h>

struct psd_layer_effects_bevel_emboss;


class KRITAIMAGE_EXPORT KisLsBevelEmbossFilter : public KisLayerStyleFilter
{
public:
    KisLsBevelEmbossFilter();

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
    KisLsBevelEmbossFilter(const KisLsBevelEmbossFilter &rhs);

    void applyBevelEmboss(KisPaintDeviceSP srcDevice,
                          KisMultipleProjection *dst,
                          const PkRect &applyRect,
                          const psd_layer_effects_bevel_emboss *config, KisResourcesInterfaceSP resourcesInterface,
                          KisLayerStyleFilterEnvironment *env) const;
};

#endif
