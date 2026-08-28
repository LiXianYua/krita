/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "patterngenerator.h"

#include <PkPoint.h>



#include <KoColor.h>
#include <KisResourceTypes.h>
#include <resources/KoPattern.h>

#include <kis_debug.h>
#include <kis_fill_painter.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_layer.h>
#include <generator/kis_generator_registry.h>
#include <kis_global.h>
#include <kis_selection.h>
#include <kis_types.h>
#include <filter/kis_filter_configuration.h>
#include <kis_processing_information.h>
#include <KisResourcesInterface.h>
#include <KoResourceLoadResult.h>

namespace { const bool patternGeneratorRegistered = [] {
    KisGeneratorRegistry::instance()->add(new PatternGenerator());
    return true;
}(); }

/****************************************************************************/
/*              KoPatternGeneratorConfiguration                             */
/****************************************************************************/

class PatternGeneratorConfiguration : public KisFilterConfiguration
{
public:
    PatternGeneratorConfiguration(const PkString & name, qint32 version, KisResourcesInterfaceSP resourcesInterface)
        : KisFilterConfiguration(name, version, resourcesInterface)
    {
    }

    PatternGeneratorConfiguration(const PatternGeneratorConfiguration &rhs)
        : KisFilterConfiguration(rhs)
    {
    }

    virtual KisFilterConfigurationSP clone() const override {
        return new PatternGeneratorConfiguration(*this);
    }

    KoResourceLoadResult pattern(KisResourcesInterfaceSP resourcesInterface) const
    {
        const PkString patternMD5 = getString("md5sum", "");
        const PkString patternName = getString("pattern", "Grid01.pat");
        const PkString patternFileName = getString("fileName", "");
        auto source = resourcesInterface->source<KoPattern>(ResourceType::Patterns);
        KoResourceLoadResult res = source.bestMatchLoadResult(patternMD5, patternFileName, patternName);
        return res;
    }

    KoPatternSP pattern() const {
        return pattern(resourcesInterface()).resource<KoPattern>();
    }

    PkTransform transform() const {
        const bool constrainScale = getBool("transform_keep_scale_aspect", true);
        const qreal scaleX = getDouble("transform_scale_x", 1.0);
        // Ensure that the size y component is equal to the x component if keepSizeSquare is true
        const qreal scaleY = constrainScale ? scaleX : getDouble("transform_scale_y", 1.0);
        const qreal positionX = getInt("transform_offset_x", 0);
        const qreal positionY = getInt("transform_offset_y", 0);
        const qreal shearX = getDouble("transform_shear_x", 0.0);
        const qreal shearY = getDouble("transform_shear_y", 0.0);
        const qreal rotationX = getDouble("transform_rotation_x", 0.0);
        const qreal rotationY = getDouble("transform_rotation_y", 0.0);
        const qreal rotationZ = getDouble("transform_rotation_z", 0.0);
        const bool align = getBool("transform_align_to_pixel_grid", false);
        const qint32 alignX = getInt("transform_align_to_pixel_grid_x", 1);
        const qint32 alignY = getInt("transform_align_to_pixel_grid_y", 1);

        PkTransform transform;

        if (align && qFuzzyIsNull(rotationX) && qFuzzyIsNull(rotationY) && pattern()) {
            // STEP 1: compose the transformation
            transform.shear(shearX, shearY);
            transform.scale(scaleX, scaleY);
            transform.rotate(rotationZ);
            // STEP 2: transform the horizontal and vertical vectors of the
            //         "repetition rect" (which size is some multiple of the
            //         pattern size)
            const PkSizeF repetitionRectSize(
                static_cast<qreal>(alignX * pattern()->width()),
                static_cast<qreal>(alignY * pattern()->height())
            );
            // u1 is the unaligned vector that goes from the origin to the top-right
            // corner of the repetition rect. u2 is the unaligned vector that
            // goes from the origin to the bottom-left corner of the repetition rect
            const PkPointF u1 = transform.map(PkPointF(repetitionRectSize.width(), 0.0));
            const PkPointF u2 = transform.map(PkPointF(0.0, repetitionRectSize.height()));
            // STEP 3: align the transformed vectors to the pixel grid. v1 is
            //         the aligned version of u1 and v2 is the aligned version of u2
            PkPointF v1(qRound(u1.x()), qRound(u1.y()));
            PkPointF v2(qRound(u2.x()), qRound(u2.y()));
            // If the following condition is met, that means that the pattern is
            // transformed in such a way that the repetition rect corners are
            // colinear so we move v1 or v2 to a neighbor position
            if (qFuzzyCompare(v1.y() * v2.x(), v2.y() * v1.x()) &&
                !qFuzzyIsNull(v1.x() * v2.x() + v1.y() * v2.y())) {
                // Choose point to move based on distance from non aligned point to
                // aligned point
                const qreal dist1 = kisSquareDistance(u1, v1);
                const qreal dist2 = kisSquareDistance(u2, v2);
                const PkPointF *p_u = dist1 > dist2 ? &u1 : &u2;
                PkPointF *p_v = dist1 > dist2 ? &v1 : &v2;
                // Then we get the closest pixel aligned point to the current,
                // colinear, point
                std::pair<int, qreal> dists[4]{
                    {1, kisSquareDistance(*p_u, *p_v + PkPointF(0.0, -1.0))},
                    {2, kisSquareDistance(*p_u, *p_v + PkPointF(1.0, 0.0))},
                    {3, kisSquareDistance(*p_u, *p_v + PkPointF(0.0, 1.0))},
                    {4, kisSquareDistance(*p_u, *p_v + PkPointF(-1.0, 0.0))}
                };
                std::sort(
                    std::begin(dists), std::end(dists),
                    [](const std::pair<int, qreal> &a, const std::pair<int, qreal> &b)
                    {
                        return a.second < b.second;
                    }
                );
                // Move the point
                if (dists[0].first == 1) {
                    p_v->setY(p_v->y() - 1.0);
                } else if (dists[0].first == 2) {
                    p_v->setX(p_v->x() + 1.0);
                } else if (dists[0].first == 3) {
                    p_v->setY(p_v->y() + 1.0);
                } else {
                    p_v->setX(p_v->x() - 1.0);
                }
            }
            // STEP 4: get the transform that maps the aligned vectors to the
            //         untransformed rect (this is in fact the inverse transform)
            PkPolygonF quad;
            quad.append(PkPointF(0, 0));
            quad.append(v1 / repetitionRectSize.width());
            quad.append(v1 / repetitionRectSize.width() + v2 / repetitionRectSize.height());
            quad.append(v2 / repetitionRectSize.height());
            PkTransform::quadToSquare(quad, transform);
            // STEP 5: get the forward transform
            transform = transform.inverted();
            transform.translate(qRound(positionX), qRound(positionY));
        } else {
            transform.shear(shearX, shearY);
            transform.scale(scaleX, scaleY);
            transform.rotate(rotationX, Qt::XAxis);
            transform.rotate(rotationY, Qt::YAxis);
            transform.rotate(rotationZ, Qt::ZAxis);
            transform.translate(positionX, positionY);
        }
        
        return transform;
    }

    PkList<KoResourceLoadResult> linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const override
    {
        return {pattern(globalResourcesInterface)};
    }
};



/****************************************************************************/
/*              KoPatternGenerator                                          */
/****************************************************************************/

PatternGenerator::PatternGenerator()
    : KisGenerator(id(), KoID("basic"), "&Pattern...")
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsPainting(true);
}

KisFilterConfigurationSP PatternGenerator::factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    return new PatternGeneratorConfiguration(id().id(), 1, resourcesInterface);
}

KisFilterConfigurationSP PatternGenerator::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);

    auto source = resourcesInterface->source<KoPattern>(ResourceType::Patterns);

    if (!source.fallbackResource()) {
        return config;
    }

    config->setProperty("md5sum", PkVariant::fromValue(source.fallbackResource()->md5Sum()));
    config->setProperty("fileName", PkVariant::fromValue(source.fallbackResource()->filename()));
    config->setProperty("pattern", PkVariant::fromValue(source.fallbackResource()->name()));

    config->setProperty("transform_shear_x", PkVariant::fromValue(0.0));
    config->setProperty("transform_shear_y", PkVariant::fromValue(0.0));

    config->setProperty("transform_scale_x", PkVariant::fromValue(1.0));
    config->setProperty("transform_scale_y", PkVariant::fromValue(1.0));

    config->setProperty("transform_rotation_x", PkVariant::fromValue(0.0));
    config->setProperty("transform_rotation_y", PkVariant::fromValue(0.0));
    config->setProperty("transform_rotation_z", PkVariant::fromValue(0.0));

    config->setProperty("transform_offset_x", PkVariant::fromValue(0));
    config->setProperty("transform_offset_y", PkVariant::fromValue(0));

    config->setProperty("transform_keep_scale_aspect", PkVariant::fromValue(true));

    config->setProperty("transform_align_to_pixel_grid", PkVariant::fromValue(false));
    config->setProperty("transform_align_to_pixel_grid_x", PkVariant::fromValue(1));
    config->setProperty("transform_align_to_pixel_grid_y", PkVariant::fromValue(1));

    return config;
}

void PatternGenerator::generate(KisProcessingInformation dstInfo,
                                 const PkSize& size,
                                 const KisFilterConfigurationSP _config,
                                 KoUpdater* progressUpdater) const
{
    KisPaintDeviceSP dst = dstInfo.paintDevice();

    Q_ASSERT(!dst.isNull());

    const PatternGeneratorConfiguration *config =
        dynamic_cast<const PatternGeneratorConfiguration*>(_config.data());

    KIS_SAFE_ASSERT_RECOVER_RETURN(config);
    KoPatternSP pattern = config->pattern();
    PkTransform transform = config->transform();

    KisFillPainter gc(dst);
    gc.setPattern(pattern);
    gc.setProgress(progressUpdater);
    gc.setChannelFlags(config->channelFlags());
    gc.setOpacityToUnit();
    gc.setSelection(dstInfo.selection());
    gc.setWidth(size.width());
    gc.setHeight(size.height());
    gc.setFillStyle(KisFillPainter::FillStylePattern);
    /**
     * HACK ALERT: using "no-compose" version of `fillRect` discards all the opacity,
     * selection, and channel flags options. Though it doesn't seem that we have a any
     * GUI in Krita that actually passes a selection to the generator itself. Fill
     * layers apply their settings on a later stage of the compositing pipeline.
     */
    gc.fillRectNoCompose(PkRect(dstInfo.topLeft(), size), pattern, transform);
    gc.end();

}
