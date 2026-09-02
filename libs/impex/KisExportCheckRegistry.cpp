/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qalgorithms.h>
#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>

#include "KisExportCheckRegistry.h"
#include <KoID.h>
#include <kis_assert.h>
#include <kis_image.h>
#include <KoColorSpace.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpaceRegistry.h>
#include <kis_generator_registry.h>

#include <AnimationCheck.h>
#include <ColorModelCheck.h>
#include <ColorModelHomogenousCheck.h>
#include <ColorModelPerLayerCheck.h>
#include <CompositionsCheck.h>
#include <ExifCheck.h>
#include <FillLayerTypeCheck.h>
#include <ImageSizeCheck.h>
#include <IntegralFrameDuration.h>
#include <LayerOpacityCheck.h>
#include <MultiLayerCheck.h>
#include <MultiTransparencyMaskCheck.h>
#include <NodeTypeCheck.h>
#include <PSDLayerStylesCheck.h>
#include <sRGBProfileCheck.h>
#include <ShapeLayerTypeCheck.h>

#include <PkString.h>

KisExportCheckRegistry *KisExportCheckRegistry::instance()
{
    static KisExportCheckRegistry s_instance;
    return &s_instance;
}

KisExportCheckRegistry::KisExportCheckRegistry ()
{
    KisExportCheckFactory *chkFactory = 0;

    // Multilayer check
    chkFactory = new MultiLayerCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Multi transparency mask check
    chkFactory = new MultiTransparencyMaskCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Animation check
    chkFactory = new AnimationCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Compositions
    chkFactory = new CompositionsCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Layer styles
    chkFactory = new PSDLayerStyleCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Check the layers for the presence of exiv info: note this is also
    // done for multilayer images even though jpeg, which supports exiv,
    // only can handle one layer.
    chkFactory = new ExifCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Check for saving exiv info in multi layered images.
    // This is specific to TIFF, which treats Exif as part of the file format itself.
    chkFactory = new TiffExifCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Check whether the image is sRGB
    chkFactory = new sRGBProfileCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Image size
    chkFactory = new ImageSizeCheckFactory();
    add(chkFactory->id(), chkFactory);

    // Do all layer have the image colorspace
    chkFactory = new ColorModelHomogenousCheckFactory();
    add(chkFactory->id(), chkFactory);

    chkFactory = new IntegralFrameDurationCheckFactory();
    add(chkFactory->id(), chkFactory);

    chkFactory = new LayerOpacityCheckFactory();
    add(chkFactory->id(), chkFactory);

    PkList<KoID> allColorModels = KoColorSpaceRegistry::instance()->colorModelsList(KoColorSpaceRegistry::AllColorSpaces);
    for (const KoID &colorModelID : allColorModels) {
        PkList<KoID> allColorDepths = KoColorSpaceRegistry::instance()->colorDepthList(colorModelID.id(), KoColorSpaceRegistry::AllColorSpaces);
        for (const KoID &colorDepthID : allColorDepths) {

            KIS_SAFE_ASSERT_RECOVER_NOOP(!colorModelID.name().isEmpty());
            KIS_SAFE_ASSERT_RECOVER_NOOP(!colorDepthID.name().isEmpty());

            // Per layer color model/channel depth checks
            chkFactory = new ColorModelPerLayerCheckFactory(colorModelID, colorDepthID);
            add(chkFactory->id(), chkFactory);

            // Image color model/channel depth checks
            chkFactory = new ColorModelCheckFactory(colorModelID, colorDepthID);
            add(chkFactory->id(), chkFactory);
        }
    }

    // Node type checks
    chkFactory = new NodeTypeCheckFactory("KisCloneLayer", PkString("Clone Layer"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisGroupLayer", PkString("Group"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisFileLayer", PkString("File Layer"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisShapeLayer", PkString("Vector Layer"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisAdjustmentLayer", PkString("Filter Layer"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisGeneratorLayer", PkString("Generator Layer"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisColorizeMask", PkString("Colorize Mask"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisFilterMask", PkString("Filter Mask"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisTransformMask", PkString("Transform Mask"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisTransparencyMask", PkString("Transparency Mask"));
    add(chkFactory->id(), chkFactory);
    chkFactory = new NodeTypeCheckFactory("KisSelectionMask", PkString("Selection Mask"));
    add(chkFactory->id(), chkFactory);

    // Fill layer/generator types.
    for (PkString generatorId : KisGeneratorRegistry::instance()->keys()) {
        chkFactory = new FillLayerTypeCheckFactory(generatorId);
        add(chkFactory->id(), chkFactory);
    }

    // Vector shapes
    chkFactory = new ShapeLayerTypeCheckFactory("KoPathShape");
    add(chkFactory->id(), chkFactory);
    chkFactory = new ShapeLayerTypeCheckFactory("KoPathShape", "RectangleShape");
    add(chkFactory->id(), chkFactory);
    chkFactory = new ShapeLayerTypeCheckFactory("KoPathShape", "EllipseShape");
    add(chkFactory->id(), chkFactory);
    chkFactory = new ShapeLayerTypeCheckFactory("KoPathShape", "StarShape");
    add(chkFactory->id(), chkFactory);
    chkFactory = new ShapeLayerTypeCheckFactory("KoPathShape", "SpiralShape");
    add(chkFactory->id(), chkFactory);
    chkFactory = new ShapeLayerTypeCheckFactory("ImageShape");
    add(chkFactory->id(), chkFactory);
    chkFactory = new ShapeLayerTypeCheckFactory("KoShapeGroup");
    add(chkFactory->id(), chkFactory);
    chkFactory = new ShapeLayerTypeCheckFactory("KoSvgTextShapeID");
    add(chkFactory->id(), chkFactory);
}

KisExportCheckRegistry::~KisExportCheckRegistry ()
{
    const auto registered = values();
    for (auto *item : registered) {
        delete item;
    }
}
