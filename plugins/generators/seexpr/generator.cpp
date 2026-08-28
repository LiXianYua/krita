/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisSequentialIteratorProgress.h>
#include <KoUpdater.h>
#include <cstring>
#include <filter/kis_filter_configuration.h>
#include <generator/kis_generator_registry.h>
#include <kis_debug.h>
#include <kis_fill_painter.h>
#include <kis_global.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>
#include <kis_selection.h>
#include <kis_types.h>

#include "SeExprExpressionContext.h"
#include "generator.h"

/****************************************************************************/
/*              KisSeExprGeneratorConfiguration                             */
/****************************************************************************/

class KisSeExprGeneratorConfiguration : public KisFilterConfiguration
{
public:
    KisSeExprGeneratorConfiguration(const PkString &name, qint32 version, KisResourcesInterfaceSP resourcesInterface)
        : KisFilterConfiguration(name, version, resourcesInterface)
    {
    }

    KisSeExprGeneratorConfiguration(const KisSeExprGeneratorConfiguration &rhs)
        : KisFilterConfiguration(rhs)
    {
    }

    virtual KisFilterConfigurationSP clone() const override
    {
        return new KisSeExprGeneratorConfiguration(*this);
    }

    PkString script() const
    {
        return this->getString("script", BASE_SCRIPT);
    }
};

namespace { const bool seExprGeneratorRegistered = [] {
    KisGeneratorRegistry::instance()->add(new KisSeExprGenerator());
    return true;
}(); }

KisSeExprGenerator::KisSeExprGenerator()
    : KisGenerator(id(), KoID("basic"), "&SeExpr...")
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsPainting(true);
}

KisFilterConfigurationSP KisSeExprGenerator::factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    return new KisSeExprGeneratorConfiguration(id().id(), 1, resourcesInterface);
}

KisFilterConfigurationSP KisSeExprGenerator::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);

    PkVariant v;
    v.setValue(PkString("Disney_noisecolor2"));
    config->setProperty("pattern", v);
    return config;
}

void KisSeExprGenerator::generate(KisProcessingInformation dstInfo, const PkSize &size, const KisFilterConfigurationSP config, KoUpdater *progressUpdater) const
{
    KisPaintDeviceSP device = dstInfo.paintDevice();

    Q_ASSERT(!device.isNull());
    Q_ASSERT(config);

    if (config) {
        PkString script = config->getString("script");

        PkRect bounds = PkRect(dstInfo.topLeft(), size);
        PkRect whole_image_bounds = device->defaultBounds()->bounds();

        SeExprExpressionContext expression(script);

        expression.m_vars["u"] = new SeExprVariable();
        expression.m_vars["v"] = new SeExprVariable();
        expression.m_vars["w"] = new SeExprVariable(whole_image_bounds.width());
        expression.m_vars["h"] = new SeExprVariable(whole_image_bounds.height());

        if (expression.isValid() && expression.returnType().isFP(3)) {
            double pixel_stride_x = 1. / whole_image_bounds.width();
            double pixel_stride_y = 1. / whole_image_bounds.height();
            double &u = expression.m_vars["u"]->m_value;
            double &v = expression.m_vars["v"]->m_value;

            // SeExpr already outputs floating-point RGB
            const KoColorSpace *dst = device->colorSpace();
            const KoColorSpace *src = KoColorSpaceRegistry::instance()->colorSpace(RGBAColorModelID.id(), Float32BitsColorDepthID.id(), KoColorSpaceRegistry::instance()->p709SRGBProfile());
            auto conv = KoColorSpaceRegistry::instance()->createColorConverter(src, dst, KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags());

            KisSequentialIteratorProgress it(device, bounds, progressUpdater);

            while (it.nextPixel()) {
                u = pixel_stride_x * (it.x() + .5);
                v = pixel_stride_y * (it.y() + .5);

                const double *value = expression.evalFP();

                KoColor c(src);
                reinterpret_cast<float *>(c.data())[0] = value[0];
                reinterpret_cast<float *>(c.data())[1] = value[1];
                reinterpret_cast<float *>(c.data())[2] = value[2];
                c.setOpacity(OPACITY_OPAQUE_F);

                conv->transform(c.data(), it.rawData(), 1);
            }
            delete conv;
        }
    }
}
