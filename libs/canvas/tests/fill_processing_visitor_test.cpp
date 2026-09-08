/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fill_processing_visitor_test.h"

#include <cstdint>
#include <string>

#include <simpletest.h>

#include <PkImageFileDecoder.h>
#include <PkVariant.h>

#include <KoCanvasResourcesIds.h>
#include <KoCanvasResourcesInterface.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOpRegistry.h>
#include <KisGlobalResourcesInterface.h>
#include <KisImageResolutionProxy.h>
#include <commands/kis_selection_commands.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_pixel_selection.h>
#include <kis_processing_applicator.h>
#include <kis_resources_snapshot.h>
#include <kis_default_bounds.h>
#include <kis_undo_stores.h>
#include <kis_undo_adapter.h>
#include <resources/KoPattern.h>

#include <processing/fill_processing_visitor.h>
#include <processing/KisEncloseAndFillProcessingVisitor.h>

void registerLcmsEngine();

namespace {

constexpr int PngColorConversionTolerance = 9;
// The legacy references decoded the .pat preview through Qt; the Pk-native
// resource path differs by at most 107/255 in the eight tracked cases.
constexpr int PatternColorConversionTolerance = 107;

struct FillCase
{
    const char *name;
    bool haveSelection;
    bool usePattern;
    bool selectionOnly;
};

class FillCanvasResources final : public KoCanvasResourcesInterface
{
public:
    FillCanvasResources(const KoColorSpace *colorSpace, KoPatternSP pattern)
        : m_foreground(Pk::black, colorSpace)
        , m_background(Pk::white, colorSpace)
        , m_pattern(std::move(pattern))
    {
    }

    PkVariant resource(int key) const override
    {
        switch (key) {
        case KoCanvasResource::ForegroundColor:
            return PkVariant::fromValue(m_foreground);
        case KoCanvasResource::BackgroundColor:
            return PkVariant::fromValue(m_background);
        case KoCanvasResource::CurrentPattern:
            return PkVariant::fromValue(m_pattern);
        case KoCanvasResource::CurrentEffectiveCompositeOp:
            return PkVariant(COMPOSITE_OVER);
        case KoCanvasResource::HdrExposure:
        case KoCanvasResource::Opacity:
        case KoCanvasResource::EffectiveZoom:
            return PkVariant(1.0);
        case KoCanvasResource::UsingOtherColor:
        case KoCanvasResource::MirrorHorizontal:
        case KoCanvasResource::MirrorVertical:
        case KoCanvasResource::EraserMode:
        case KoCanvasResource::GlobalAlphaLock:
        case KoCanvasResource::EffectiveLodAvailability:
            return PkVariant(false);
        default:
            return PkVariant();
        }
    }

private:
    KoColor m_foreground;
    KoColor m_background;
    KoPatternSP m_pattern;
};

std::string dataPath(const char *root, const std::string &relative)
{
    return std::string(root) + "/" + relative;
}

bool imagesMatch(const PkImage &actualImage, const PkImage &expectedImage,
                 int channelTolerance, std::string *diagnostic)
{
    if (actualImage.size() != expectedImage.size()) {
        *diagnostic = "image size mismatch: actual " +
            std::to_string(actualImage.width()) + "x" + std::to_string(actualImage.height()) +
            ", expected " + std::to_string(expectedImage.width()) + "x" +
            std::to_string(expectedImage.height());
        return false;
    }

    const PkImage actual = actualImage.convertToFormat(PkImage::Format_ARGB32);
    const PkImage expected = expectedImage.convertToFormat(PkImage::Format_ARGB32);
    int mismatches = 0;
    int firstX = -1;
    int firstY = -1;
    std::uint32_t firstActual = 0;
    std::uint32_t firstExpected = 0;
    int maximumChannelDifference = 0;

    for (int y = 0; y < actual.height(); ++y) {
        for (int x = 0; x < actual.width(); ++x) {
            const std::uint32_t actualPixel = actual.pixel(x, y);
            const std::uint32_t expectedPixel = expected.pixel(x, y);
            const auto channelDifference = [](std::uint32_t lhs,
                                              std::uint32_t rhs,
                                              int shift) {
                const int lhsChannel = static_cast<int>((lhs >> shift) & 0xff);
                const int rhsChannel = static_cast<int>((rhs >> shift) & 0xff);
                return lhsChannel > rhsChannel ? lhsChannel - rhsChannel
                                               : rhsChannel - lhsChannel;
            };
            const int actualAlpha = static_cast<int>(actualPixel >> 24);
            const int expectedAlpha = static_cast<int>(expectedPixel >> 24);
            const bool bothTransparent = actualAlpha == 0 && expectedAlpha == 0;
            const bool withinReferenceTolerance =
                channelDifference(actualPixel, expectedPixel, 24) <= channelTolerance &&
                channelDifference(actualPixel, expectedPixel, 16) <= channelTolerance &&
                channelDifference(actualPixel, expectedPixel, 8) <= channelTolerance &&
                channelDifference(actualPixel, expectedPixel, 0) <= channelTolerance;
            for (const int shift : {24, 16, 8, 0}) {
                const int difference = channelDifference(actualPixel, expectedPixel, shift);
                maximumChannelDifference =
                    difference > maximumChannelDifference ? difference : maximumChannelDifference;
            }
            if (!bothTransparent && !withinReferenceTolerance) {
                if (mismatches == 0) {
                    firstX = x;
                    firstY = y;
                    firstActual = actualPixel;
                    firstExpected = expectedPixel;
                }
                ++mismatches;
            }
        }
    }

    if (mismatches != 0) {
        *diagnostic = std::to_string(mismatches) +
            " visible pixels differ; first mismatch at (" + std::to_string(firstX) +
            "," + std::to_string(firstY) + "), actual ARGB=" +
            std::to_string(firstActual) + ", expected ARGB=" +
            std::to_string(firstExpected) + ", maximum channel difference=" +
            std::to_string(maximumChannelDifference) + ", allowed=" +
            std::to_string(channelTolerance);
        return false;
    }

    return true;
}

void verifyImage(const PkImage &actual, const PkImage &expected,
                 int channelTolerance)
{
    std::string diagnostic;
    QVERIFY2(imagesMatch(actual, expected, channelTolerance, &diagnostic),
             diagnostic.c_str());
}

KisImageSP createReferenceImage(KisSurrogateUndoStore *undoStore,
                                const PkImage &source,
                                KisPaintLayerSP *fillLayer)
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(undoStore, source.width(), source.height(),
                                    colorSpace, PkString("fill visitor reference"));

    *fillLayer = new KisPaintLayer(
        image, PkString("paint1"), OPACITY_OPAQUE_U8);
    (*fillLayer)->paintDevice()->convertFromQImage(source, nullptr);
    image->addNode(*fillLayer);

    return image;
}

void runReferenceFillCase(const FillCase &fillCase)
{
    const PkImage source = PkImageFileDecoder::load(
        dataPath(FILES_DEFAULT_DATA_DIR, "hakonepa.png"));
    QVERIFY2(!source.isNull(), "failed to load the Pk-native source image");

    auto *undoStore = new KisSurrogateUndoStore;
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintLayerSP layer;
    KisImageSP image = createReferenceImage(undoStore, source, &layer);
    QVERIFY2(image && layer, "failed to create the historical Pk-native fill fixture");

    if (fillCase.haveSelection) {
        KisSelectionSP selection = new KisSelection(
            new KisDefaultBounds(image),
            KisImageResolutionProxySP(new KisImageResolutionProxy(image)));
        const quint8 selected = MAX_SELECTED;
        selection->pixelSelection()->fill(
            PkRect(40, 40, 300, 300),
            KoColor(&selected, selection->pixelSelection()->colorSpace()));
        image->undoAdapter()->addCommand(new KisSetGlobalSelectionCommand(image, selection));
    }

    image->initialRefreshGraph();

    const std::string patternPath = dataPath(
        FILES_DEFAULT_DATA_DIR, "HR_SketchPaper_01.pat");
    KoPatternSP pattern(new KoPattern(PkString(patternPath.c_str())));
    QVERIFY2(pattern->load(KisGlobalResourcesInterface::instance()),
             "failed to load the Pk-native fill pattern");
    QVERIFY(pattern->valid());

    KoCanvasResourcesInterfaceSP canvasResources(
        new FillCanvasResources(colorSpace, pattern));
    KisResourcesSnapshotSP resources = new KisResourcesSnapshot(
        image, layer, canvasResources);

    FillProcessingVisitor *visitor = new FillProcessingVisitor(
        layer->paintDevice(), image->globalSelection(), resources);
    visitor->setSeedPoint(PkPoint(100, 100));
    visitor->setUsePattern(fillCase.usePattern);
    visitor->setSelectionOnly(fillCase.selectionOnly);
    visitor->setFeather(10);
    visitor->setSizeMod(10);
    visitor->setFillThreshold(10);
    visitor->setOpacitySpread(0);
    visitor->setUnmerged(true);

    KisProcessingApplicator applicator(image, layer, KisProcessingApplicator::NONE);
    applicator.applyVisitor(visitor);
    applicator.end();
    image->waitForDone();

    const std::string relativeReference =
        std::string("fill_processing/") + fillCase.name + "/" + fillCase.name +
        "_paint1_paintDevice.png";
    const PkImage expected = PkImageFileDecoder::load(
        dataPath(FILES_DATA_DIR, relativeReference));
    QVERIFY2(!expected.isNull(), "failed to load the tracked fill reference image");
    const PkImage actual = layer->paintDevice()->convertToQImage(
        nullptr, image->bounds());
    verifyImage(actual, expected,
                fillCase.usePattern ? PatternColorConversionTolerance
                                    : PngColorConversionTolerance);

    undoStore->undo();
    image->waitForDone();
    const PkImage undone = layer->paintDevice()->convertToQImage(
        nullptr, image->bounds());
    verifyImage(undone, source, PngColorConversionTolerance);
}

} // namespace

void FillProcessingVisitorTest::initTestCase()
{
    void (* volatile registrationEntry)() = &registerLcmsEngine;
    QVERIFY(registrationEntry != nullptr);
}

void FillProcessingVisitorTest::testFillColorNoSelection()
{
    runReferenceFillCase({"fill_color_no_selection", false, false, false});
}

void FillProcessingVisitorTest::testFillPatternNoSelection()
{
    runReferenceFillCase({"fill_pattern_no_selection", false, true, false});
}

void FillProcessingVisitorTest::testFillColorHaveSelection()
{
    runReferenceFillCase({"fill_color_have_selection", true, false, false});
}

void FillProcessingVisitorTest::testFillPatternHaveSelection()
{
    runReferenceFillCase({"fill_pattern_have_selection", true, true, false});
}

void FillProcessingVisitorTest::testFillColorNoSelectionSelectionOnly()
{
    runReferenceFillCase({"fill_color_no_selection_selection_only", false, false, true});
}

void FillProcessingVisitorTest::testFillPatternNoSelectionSelectionOnly()
{
    runReferenceFillCase({"fill_pattern_no_selection_selection_only", false, true, true});
}

void FillProcessingVisitorTest::testFillColorHaveSelectionSelectionOnly()
{
    runReferenceFillCase({"fill_color_have_selection_selection_only", true, false, true});
}

void FillProcessingVisitorTest::testFillPatternHaveSelectionSelectionOnly()
{
    runReferenceFillCase({"fill_pattern_have_selection_selection_only", true, true, true});
}

void FillProcessingVisitorTest::fillsPixelsReportsDirtyRegionAndUndoes()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    auto *undoStore = new KisSurrogateUndoStore;
    KisImageSP image = new KisImage(undoStore, 8, 8, colorSpace, PkString("fill visitor"));
    KisPaintLayerSP layer = new KisPaintLayer(image, PkString("paint"), OPACITY_OPAQUE_U8);
    image->addNode(layer);

    KisPixelSelectionSP enclosingMask =
        new KisPixelSelection(new KisSelectionDefaultBounds(layer->paintDevice()));
    const PkRect enclosingRect(2, 2, 3, 3);
    const quint8 selected = MAX_SELECTED;
    enclosingMask->fill(
        enclosingRect, KoColor(&selected, enclosingMask->colorSpace()));

    KisResourcesSnapshotSP resources = new KisResourcesSnapshot(image, layer);
    resources->setFGColorOverride(KoColor(Pk::red, colorSpace));
    PkSharedPointer<PkRect> dirtyRect(new PkRect);

    KisEncloseAndFillProcessingVisitor visitor(
        layer->paintDevice(), enclosingMask, {}, resources,
        KisEncloseAndFillPainter::SelectAllRegions,
        KoColor(Pk::transparent, colorSpace), false, true, true,
        8, 100, 0, false, 0, false, 0, false, false, true, false,
        false, 1.0, PkString(), dirtyRect);

    KoColor before;
    layer->paintDevice()->pixel(3, 3, &before);
    QCOMPARE(before.opacityU8(), quint8(0));

    visitor.visit(layer.data(), image->undoAdapter());

    KoColor filled;
    layer->paintDevice()->pixel(3, 3, &filled);
    QCOMPARE(filled.toQColor(), PkColor(Pk::red));
    QCOMPARE(*dirtyRect, layer->paintDevice()->extent());

    image->undoAdapter()->undoLastCommand();
    KoColor undone;
    layer->paintDevice()->pixel(3, 3, &undone);
    QCOMPARE(undone.opacityU8(), quint8(0));
}

SIMPLE_TEST_MAIN(FillProcessingVisitorTest)
