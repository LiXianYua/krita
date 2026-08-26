/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt boud @valdyas.org
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_kra_saver_test.h"

#include <PkEventLoop.h>

#include <KisDocument.h>
#include <KisDocumentRegistry.h>
#include <KoDocumentInfo.h>
#include <KoShapeContainer.h>
#include <KoPathShape.h>

#include "filter/kis_filter_registry.h"
#include "filter/kis_filter_configuration.h"
#include "filter/kis_filter.h"
#include "kis_image.h"
#include <KisImageResolutionProxy.h>
#include "kis_pixel_selection.h"
#include "kis_group_layer.h"
#include "kis_paint_layer.h"
#include "kis_clone_layer.h"
#include "kis_adjustment_layer.h"
#include "kis_shape_layer.h"
#include "kis_filter_mask.h"
#include "kis_transparency_mask.h"
#include "kis_selection_mask.h"
#include "kis_selection.h"
#include "kis_fill_painter.h"
#include "kis_shape_selection.h"
#include "util.h"
#include <testutil.h>
#include "kis_keyframe_channel.h"
#include "kis_image_animation_interface.h"
#include "kis_layer_properties_icons.h"
#include <KisGlobalResourcesInterface.h>

#include "KritaTransformMaskStubs.h"
#include "KisDumbTransformMaskParams.h"

#include "StoryboardItem.h"

#include <generator/kis_generator_registry.h>

#include <KoResourcePaths.h>
#include <filestest.h>
#include <testui.h>


const PkString KraMimetype = "application/x-krita";

void KisKraSaverTest::initTestCase()
{
    KoResourcePaths::addAssetDir(ResourceType::Patterns, PkString(SYSTEM_RESOURCES_DATA_DIR) + "/patterns");

    KisFilterRegistry::instance();
    KisGeneratorRegistry::instance();

    TestUtil::registerTransformMaskStubs();
}

void KisKraSaverTest::testCrashyShapeLayer()
{
    /**
     * KisShapeLayer used to call setImage from its destructor and
     * therefore causing an infinite recursion (when at least one transparency
     * mask was preset. This testcase just checks that.
     */

    //PkScopedPointer<KisDocument> doc(createCompleteDocument(true));
    //Q_UNUSED(doc);
}

void KisKraSaverTest::testRoundTrip()
{
    PkScopedPointer<KisDocument> doc(createCompleteDocument());
    KoColor bgColor(PkColor(255, 0, 0), doc->image()->colorSpace());
    doc->image()->setDefaultProjectionColor(bgColor);
    doc->image()->waitForDone(); // wait to make sure the image can be locked for saving!
    bool result = doc->exportDocumentSync("roundtriptest.kra", doc->mimeType());
    PK_VERIFY(result);

    PkStringList list;
    KisCountVisitor cv1(list, KoProperties());
    doc->image()->rootLayer()->accept(cv1);

    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    result = doc2->loadNativeFormat("roundtriptest.kra");
    PK_VERIFY(result);

    KisCountVisitor cv2(list, KoProperties());
    doc2->image()->rootLayer()->accept(cv2);
    PK_COMPARE(cv1.count(), cv2.count());

    // check whether the BG color is saved correctly
    PK_COMPARE(doc2->image()->defaultProjectionColor(), bgColor);

    // test round trip of a transform mask
    KisNode* tnode =
        TestUtil::findNode(doc2->image()->rootLayer(), "testTransformMask").data();
    PK_VERIFY(tnode);
    KisTransformMask *tmask = dynamic_cast<KisTransformMask*>(tnode);
    PK_VERIFY(tmask);
    PkSharedPointer<KisDumbTransformMaskParams> params = tmask->transformParams().dynamicCast<KisDumbTransformMaskParams>();
    PK_VERIFY(params);
    PkTransform t = params->testingGetTransform();
    PK_COMPARE(t, createTestingTransform());
}

void KisKraSaverTest::testSaveEmpty()
{
    KisDocument* doc = createEmptyDocument();
    doc->exportDocumentSync("emptytest.kra", doc->mimeType());
    PkStringList list;
    KisCountVisitor cv1(list, KoProperties());
    doc->image()->rootLayer()->accept(cv1);

    KisDocument *doc2 = KisDocumentRegistry::instance()->createDocument();
    doc2->loadNativeFormat("emptytest.kra");

    KisCountVisitor cv2(list, KoProperties());
    doc2->image()->rootLayer()->accept(cv2);
    PK_COMPARE(cv1.count(), cv2.count());

    delete doc2;
    delete doc;
}

#include <generator/kis_generator.h>

void testRoundTripFillLayerImpl(const PkString &testName, KisFilterConfigurationSP config)
{
    TestUtil::ReferenceImageChecker chk(testName, "fill_layer");
    chk.setFuzzy(2);

    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());

    // mask parent should be destructed before the document!
    PkRect refRect(0,0,512,512);
    TestUtil::MaskParent p(refRect);

    doc->setCurrentImage(p.image);
    doc->documentInfo()->setAboutInfo("title", p.image->objectName());

    KisSelectionSP selection;
    KisGeneratorLayerSP glayer = new KisGeneratorLayer(p.image, "glayer", config->cloneWithResourcesSnapshot(), selection);

    p.image->addNode(glayer, p.image->root(), KisNodeSP());
    glayer->setDirty();

    p.image->waitForDone();
    chk.checkImage(p.image, "00_initial_layer_update");

    doc->exportDocumentSync("roundtrip_fill_layer_test.kra", doc->mimeType());

    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    doc2->loadNativeFormat("roundtrip_fill_layer_test.kra");

    doc2->image()->waitForDone();
    chk.checkImage(doc2->image(), "01_fill_layer_round_trip");

    PK_VERIFY(chk.testPassed());
}

void KisKraSaverTest::testRoundTripFillLayerColor()
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();

    KisGeneratorSP generator = KisGeneratorRegistry::instance()->get("color");
    PK_VERIFY(generator);

    // warning: we pass null paint device to the default constructed value
    KisFilterConfigurationSP config = generator->defaultConfiguration(KisGlobalResourcesInterface::instance());
    PK_VERIFY(config);

    PkVariant v;
    v.setValue(KoColor(PkColor(255, 0, 0), cs));
    config->setProperty("color", v);

    testRoundTripFillLayerImpl("fill_layer_color", config);
}

void KisKraSaverTest::testRoundTripFillLayerPattern()
{
    KisGeneratorSP generator = KisGeneratorRegistry::instance()->get("pattern");
    PK_VERIFY(generator);

    // warning: we pass null paint device to the default constructed value
    KisFilterConfigurationSP config = generator->defaultConfiguration(KisGlobalResourcesInterface::instance());
    PK_VERIFY(config);

    PkVariant v;
    v.setValue(PkString("11_drawed_furry.png"));
    config->setProperty("pattern", v);

    testRoundTripFillLayerImpl("fill_layer_pattern", config);
}

#include "kis_psd_layer_style.h"


void KisKraSaverTest::testRoundTripLayerStyles()
{
    TestUtil::ReferenceImageChecker chk("kra_saver_test", "layer_styles");

    PkRect imageRect(0,0,512,512);

    // the document should be created before the image!
    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());

    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(new KisSurrogateUndoStore(), imageRect.width(), imageRect.height(), cs, "test image");
    KisPaintLayerSP layer1 = new KisPaintLayer(image, "paint1", OPACITY_OPAQUE_U8);
    KisPaintLayerSP layer2 = new KisPaintLayer(image, "paint2", OPACITY_OPAQUE_U8);
    KisPaintLayerSP layer3 = new KisPaintLayer(image, "paint3", OPACITY_OPAQUE_U8);
    image->addNode(layer1);
    image->addNode(layer2);
    image->addNode(layer3);

    doc->setCurrentImage(image);
    doc->documentInfo()->setAboutInfo("title", image->objectName());

    layer1->paintDevice()->fill(PkRect(100, 100, 100, 100), KoColor(PkColor(255, 0, 0), cs));
    layer2->paintDevice()->fill(PkRect(200, 200, 100, 100), KoColor(PkColor(0, 255, 0), cs));
    layer3->paintDevice()->fill(PkRect(300, 300, 100, 100), KoColor(PkColor(0, 0, 255), cs));

    KisPSDLayerStyleSP style(new KisPSDLayerStyle());
    style->dropShadow()->setEffectEnabled(true);


    style->dropShadow()->setAngle(-90);
    style->dropShadow()->setUseGlobalLight(false);
    layer1->setLayerStyle(style->clone().dynamicCast<KisPSDLayerStyle>());

    style->dropShadow()->setAngle(180);
    style->dropShadow()->setUseGlobalLight(true);
    layer2->setLayerStyle(style->clone().dynamicCast<KisPSDLayerStyle>());

    style->dropShadow()->setAngle(90);
    style->dropShadow()->setUseGlobalLight(false);
    layer3->setLayerStyle(style->clone().dynamicCast<KisPSDLayerStyle>());

    image->initialRefreshGraph();
    chk.checkImage(image, "00_initial_layers");

    doc->exportDocumentSync("roundtrip_layer_styles.kra", doc->mimeType());


    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    doc2->loadNativeFormat("roundtrip_layer_styles.kra");

    doc2->image()->waitForDone();
    chk.checkImage(doc2->image(), "00_initial_layers");

    PK_VERIFY(chk.testPassed());
}

void KisKraSaverTest::testRoundTripAnimation()
{
    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());

    PkRect imageRect(0,0,512,512);
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(new KisSurrogateUndoStore(), imageRect.width(), imageRect.height(), cs, "test image");
    KisPaintLayerSP layer1 = new KisPaintLayer(image, "paint1", OPACITY_OPAQUE_U8);
    image->addNode(layer1);

    layer1->paintDevice()->fill(PkRect(100, 100, 50, 50), KoColor(PkColor(0, 0, 0), cs));
    layer1->paintDevice()->setDefaultPixel(KoColor(PkColor(255, 0, 0), cs));

    KUndo2Command parentCommand;

    layer1->enableAnimation();
    KisKeyframeChannel *rasterChannel = layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    PK_VERIFY(rasterChannel);

    rasterChannel->addKeyframe(10, &parentCommand);
    image->animationInterface()->switchCurrentTimeAsync(10);
    image->waitForDone();
    layer1->paintDevice()->fill(PkRect(200, 50, 10, 10), KoColor(PkColor(0, 0, 0), cs));
    layer1->paintDevice()->moveTo(25, 15);
    layer1->paintDevice()->setDefaultPixel(KoColor(PkColor(0, 255, 0), cs));

    rasterChannel->addKeyframe(20, &parentCommand);
    image->animationInterface()->switchCurrentTimeAsync(20);
    image->waitForDone();
    layer1->paintDevice()->fill(PkRect(150, 200, 30, 30), KoColor(PkColor(0, 0, 0), cs));
    layer1->paintDevice()->moveTo(100, 50);
    layer1->paintDevice()->setDefaultPixel(KoColor(PkColor(0, 0, 255), cs));

    PK_VERIFY(!layer1->isPinnedToTimeline());
    layer1->setPinnedToTimeline(true);

    doc->setCurrentImage(image);
    doc->exportDocumentSync("roundtrip_animation.kra", doc->mimeType());

    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    doc2->loadNativeFormat("roundtrip_animation.kra");
    KisImageSP image2 = doc2->image();
    KisNodeSP node = image2->root()->firstChild();

    PK_VERIFY(node->inherits("KisPaintLayer"));
    KisPaintLayerSP layer2 = dynamic_cast<KisPaintLayer*>(node.data());
    cs = layer2->paintDevice()->colorSpace();

    PK_COMPARE(image2->animationInterface()->currentTime(), 20);
    KisKeyframeChannel *channel = layer2->getKeyframeChannel(KisKeyframeChannel::Raster.id());
    PK_VERIFY(channel);
    PK_COMPARE(channel->keyframeCount(), 3);

    image2->animationInterface()->switchCurrentTimeAsync(0);
    image2->waitForDone();

    PK_COMPARE(layer2->paintDevice()->nonDefaultPixelArea(), PkRect(64, 64, 128, 128));
    PK_COMPARE(layer2->paintDevice()->x(), 0);
    PK_COMPARE(layer2->paintDevice()->y(), 0);
    PK_COMPARE(layer2->paintDevice()->defaultPixel(), KoColor(PkColor(255, 0, 0), cs));

    image2->animationInterface()->switchCurrentTimeAsync(10);
    image2->waitForDone();

    PK_COMPARE(layer2->paintDevice()->nonDefaultPixelArea(), PkRect(217, 15, 64, 64));
    PK_COMPARE(layer2->paintDevice()->x(), 25);
    PK_COMPARE(layer2->paintDevice()->y(), 15);
    PK_COMPARE(layer2->paintDevice()->defaultPixel(), KoColor(PkColor(0, 255, 0), cs));

    image2->animationInterface()->switchCurrentTimeAsync(20);
    image2->waitForDone();

    PK_COMPARE(layer2->paintDevice()->nonDefaultPixelArea(), PkRect(228, 242, 64, 64));
    PK_COMPARE(layer2->paintDevice()->x(), 100);
    PK_COMPARE(layer2->paintDevice()->y(), 50);
    PK_COMPARE(layer2->paintDevice()->defaultPixel(), KoColor(PkColor(0, 0, 255), cs));

    PK_VERIFY(layer2->isPinnedToTimeline());

}

#include "lazybrush/kis_lazy_fill_tools.h"

void KisKraSaverTest::testRoundTripColorizeMask()
{
    PkRect imageRect(0,0,512,512);
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    const KoColorSpace * weirdCS = KoColorSpaceRegistry::instance()->rgb16();

    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(new KisSurrogateUndoStore(), imageRect.width(), imageRect.height(), cs, "test image");
    doc->setCurrentImage(image);

    KisPaintLayerSP layer1 = new KisPaintLayer(image, "paint1", OPACITY_OPAQUE_U8, weirdCS);
    image->addNode(layer1);

    KisColorizeMaskSP mask = new KisColorizeMask(image, "mask1");
    image->addNode(mask, layer1);
    mask->initializeCompositeOp();
    delete mask->setColorSpace(layer1->colorSpace());

    {
        KisPaintDeviceSP key1 = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
        key1->fill(PkRect(50,50,10,20), KoColor(PkColor(0, 0, 0), key1->colorSpace()));
        mask->testingAddKeyStroke(key1, KoColor(PkColor(0, 255, 0), layer1->colorSpace()));
        // KIS_DUMP_DEVICE_2(key1, refRect, "key1", "dd");
    }

    {
        KisPaintDeviceSP key2 = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
        key2->fill(PkRect(150,50,10,20), KoColor(PkColor(0, 0, 0), key2->colorSpace()));
        mask->testingAddKeyStroke(key2, KoColor(PkColor(255, 0, 0), layer1->colorSpace()));
        // KIS_DUMP_DEVICE_2(key2, refRect, "key2", "dd");
    }

    {
        KisPaintDeviceSP key3 = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
        key3->fill(PkRect(0,0,10,10), KoColor(PkColor(0, 0, 0), key3->colorSpace()));
        mask->testingAddKeyStroke(key3, KoColor(PkColor(0, 0, 255), layer1->colorSpace()), true);
        // KIS_DUMP_DEVICE_2(key3, refRect, "key3", "dd");
    }

    KisLayerPropertiesIcons::setNodePropertyAutoUndo(mask, KisLayerPropertiesIcons::colorizeEditKeyStrokes, false, image);
    KisLayerPropertiesIcons::setNodePropertyAutoUndo(mask, KisLayerPropertiesIcons::colorizeShowColoring, false, image);
    image->waitForDone();



    doc->exportDocumentSync("roundtrip_colorize.kra", doc->mimeType());

    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    doc2->loadNativeFormat("roundtrip_colorize.kra");
    KisImageSP image2 = doc2->image();
    KisNodeSP node = image2->root()->firstChild()->firstChild();

    KisColorizeMaskSP mask2 = dynamic_cast<KisColorizeMask*>(node.data());
    PK_VERIFY(mask2);

    PK_COMPARE(mask2->compositeOpId(), mask->compositeOpId());
    PK_COMPARE(*mask2->colorSpace(), *mask->colorSpace());
    PK_COMPARE(KisLayerPropertiesIcons::nodeProperty(mask, KisLayerPropertiesIcons::colorizeEditKeyStrokes, true).toBool(), false);
    PK_COMPARE(KisLayerPropertiesIcons::nodeProperty(mask, KisLayerPropertiesIcons::colorizeShowColoring, true).toBool(), false);

    PkList<KisLazyFillTools::KeyStroke> strokes = mask->fetchKeyStrokesDirect();

    PK_COMPARE(strokes[0].dev->exactBounds(), PkRect(50,50,10,20));
    PK_COMPARE(strokes[0].isTransparent, false);
    PK_COMPARE(strokes[0].color.colorSpace(), weirdCS);

    PK_COMPARE(strokes[1].dev->exactBounds(), PkRect(150,50,10,20));
    PK_COMPARE(strokes[1].isTransparent, false);
    PK_COMPARE(strokes[1].color.colorSpace(), weirdCS);

    PK_COMPARE(strokes[2].dev->exactBounds(), PkRect(0,0,10,10));
    PK_COMPARE(strokes[2].isTransparent, true);
    PK_COMPARE(strokes[2].color.colorSpace(), weirdCS);
}

#include <KoColorBackground.h>

void KisKraSaverTest::testRoundTripShapeLayer()
{
    TestUtil::ReferenceImageChecker chk("kra_saver_test", "shape_layer");

    PkRect refRect(0,0,512,512);

    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
    TestUtil::MaskParent p(refRect);

    const qreal resolution = 144.0 / 72.0;
    p.image->setResolution(resolution, resolution);

    doc->setCurrentImage(p.image);
    doc->documentInfo()->setAboutInfo("title", p.image->objectName());

    KoPathShape* path = new KoPathShape();
    path->setShapeId(KoPathShapeId);
    path->moveTo(PkPointF(10, 10));
    path->lineTo(PkPointF( 10, 110));
    path->lineTo(PkPointF(110, 110));
    path->lineTo(PkPointF(110,  10));
    path->close();
    path->normalize();
    path->setBackground(toQShared(new KoColorBackground(PkColor(255, 0, 0))));

    path->setName("my_precious_shape");

    KisShapeLayerSP shapeLayer = new KisShapeLayer(doc->shapeController(), p.image, "shapeLayer1", 75);
    shapeLayer->addShape(path);
    p.image->addNode(shapeLayer);
    shapeLayer->setDirty();

    PkEventLoop::processEvents();
    p.image->waitForDone();

    chk.checkImage(p.image, "00_initial_layer_update");

    doc->exportDocumentSync("roundtrip_shapelayer_test.kra", doc->mimeType());

    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    doc2->loadNativeFormat("roundtrip_shapelayer_test.kra");

    PkEventLoop::processEvents();
    doc2->image()->waitForDone();
    PK_COMPARE(doc2->image()->xRes(), resolution);
    PK_COMPARE(doc2->image()->yRes(), resolution);
    chk.checkImage(doc2->image(), "01_shape_layer_round_trip");

    PK_VERIFY(chk.testPassed());
}

void KisKraSaverTest::testRoundTripShapeSelection()
{
    TestUtil::ReferenceImageChecker chk("kra_saver_test", "shape_selection");

    PkRect refRect(0,0,512,512);

    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
    TestUtil::MaskParent p(refRect);
    doc->setCurrentImage(p.image);
    const qreal resolution = 144.0 / 72.0;
    p.image->setResolution(resolution, resolution);

    doc->setCurrentImage(p.image);
    doc->documentInfo()->setAboutInfo("title", p.image->objectName());

    p.layer->paintDevice()->setDefaultPixel(KoColor(PkColor(0, 255, 0), p.layer->colorSpace()));

    KisImageResolutionProxySP resolutionProxy(new KisImageResolutionProxy(p.image));
    KisSelectionSP selection = new KisSelection(p.layer->paintDevice() ->defaultBounds(), resolutionProxy);
    KisShapeSelection *shapeSelection = new KisShapeSelection(doc->shapeController(), selection);
    selection->convertToVectorSelectionNoUndo(shapeSelection);

    KoPathShape* path = new KoPathShape();
    path->setShapeId(KoPathShapeId);
    path->moveTo(PkPointF(10, 10));
    path->lineTo(PkPointF( 10, 110));
    path->lineTo(PkPointF(110, 110));
    path->lineTo(PkPointF(110,  10));
    path->close();
    path->normalize();
    path->setBackground(toQShared(new KoColorBackground(PkColor(255, 0, 0))));
    path->setName("my_precious_shape");

    shapeSelection->addShape(path);

    KisTransparencyMaskSP tmask = new KisTransparencyMask(p.image, "tmask");
    tmask->setSelection(selection);
    p.image->addNode(tmask, p.layer);

    tmask->setDirty(p.image->bounds());

    PkEventLoop::processEvents();
    p.image->waitForDone();

    chk.checkImage(p.image, "00_initial_shape_selection");

    doc->exportDocumentSync("roundtrip_shapeselection_test.kra", doc->mimeType());

    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    doc2->loadNativeFormat("roundtrip_shapeselection_test.kra");

    PkEventLoop::processEvents();
    doc2->image()->waitForDone();
    PK_COMPARE(doc2->image()->xRes(), resolution);
    PK_COMPARE(doc2->image()->yRes(), resolution);
    chk.checkImage(doc2->image(), "00_initial_shape_selection");

    KisNodeSP node = doc2->image()->root()->firstChild()->firstChild();
    KisTransparencyMask *newMask = dynamic_cast<KisTransparencyMask*>(node.data());
    PK_VERIFY(newMask);

    PK_VERIFY(newMask->selection()->hasNonEmptyShapeSelection());

    PK_VERIFY(chk.testPassed());
}


void KisKraSaverTest::testRoundTripStoryboard()
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    PkRect imageRect(0,0,512,512);

    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(new KisSurrogateUndoStore(), imageRect.width(), imageRect.height(), cs, "test image");
    doc->setCurrentImage(image);

    // TODO: make initialization of StoryboardItem more fool-proof
    StoryboardItemSP item(new StoryboardItem());
    item->appendChild(PkVariant::fromValue(ThumbnailData()));
    item->appendChild("scene0");
    item->appendChild(10);
    item->appendChild(2);

    StoryboardItemList list;
    list.append(item);

    doc->setStoryboardItemList(list);
    bool result = doc->exportDocumentSync("storyboardroundtriptest.kra", doc->mimeType());
    PK_VERIFY(result);

    PkScopedPointer<KisDocument> doc2(KisDocumentRegistry::instance()->createDocument());
    result = doc2->loadNativeFormat("storyboardroundtriptest.kra");
    PK_VERIFY(result);

    PK_COMPARE(doc2->getStoryboardItemList().count(), list.count());
}

void KisKraSaverTest::testExportToReadonly()
{
    TestUtil::testExportToReadonly(KraMimetype);
}

#ifdef PK_SHELL_MOC_BINDER
#include "pk_binder_kis_kra_saver_test.inc"
#endif

PK_TEST_GUILESS_MAIN(KisKraSaverTest)
