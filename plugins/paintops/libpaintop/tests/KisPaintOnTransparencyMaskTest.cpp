/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KisPaintOnTransparencyMaskTest.h"

#include <simpletest.h>
#include <KoCompositeOpRegistry.h>
#include <KoColor.h>
#include "stroke_testing_utils.h"
#include "strokes/freehand_stroke.h"
#include "strokes/KisFreehandStrokeInfo.h"
#include "KisAsynchronousStrokeUpdateHelper.h"
#include "kis_resources_snapshot.h"
#include "kis_image.h"
#include <brushengine/kis_paint_information.h>
#include "kis_transparency_mask.h"

#include "kis_paint_device_debug_utils.h"
#include "kis_sequential_iterator.h"
#include "kis_layer_utils.h"
#include "kis_transaction.h"
#include "kis_command_utils.h"
#include "kis_processing_applicator.h"
#include "KisAnimAutoKey.h"

#include <memory>

namespace {

bool clearImage(KisImageSP image, KisNodeList nodes)
{
    KisNodeList masks;

    for (KisNodeSP node : std::as_const(nodes)) {
        if (node->inherits("KisMask")) {
            masks.append(node);
        }
    }

    KisLayerUtils::filterMergeableNodes(nodes);
    nodes.append(masks);

    KisProcessingApplicator applicator(image, 0, KisProcessingApplicator::NONE,
                                       KisImageSignalVector(), kundo2_i18n("Clear"));

    for (KisNodeSP node : std::as_const(nodes)) {
        KisLayerUtils::recursiveApplyNodes(node, [&applicator, masks] (KisNodeSP child) {
            if (child->inherits("KisMask") && !masks.contains(child)) {
                return;
            }

            if (child->hasEditablePaintDevice()) {
                KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
                    kundo2_i18n("Clear"), [child] () {
                        KisPaintDeviceSP device = child->paintDevice();
                        std::unique_ptr<KisCommandUtils::CompositeCommand> parentCommand(
                            new KisCommandUtils::CompositeCommand());

                        KUndo2Command *autoKeyframeCommand =
                            KisAutoKey::tryAutoCreateDuplicatedFrame(device);
                        if (autoKeyframeCommand) {
                            parentCommand->addCommand(autoKeyframeCommand);
                        }

                        KisTransaction transaction(kundo2_noi18n("internal-clear-command"), device);
                        const QRect dirtyRect = device->extent();
                        device->clear();
                        device->setDirty(dirtyRect);
                        parentCommand->addCommand(transaction.endAndTake());
                        return parentCommand.release();
                    });
                applicator.applyCommand(cmd, KisStrokeJobData::CONCURRENT);
            }
        });
    }

    applicator.end();
    return true;
}

} // namespace

class PaintOnTransparencyMaskTester : public utils::StrokeTester
{
public:
    PaintOnTransparencyMaskTester(const QString &presetFilename)
        : StrokeTester("freehand_benchmark", QSize(5000, 5000), presetFilename)
    {
        setBaseFuzziness(3);
    }

protected:
    using utils::StrokeTester::initImage;
    void initImage(KisImageWSP image, KisNodeSP activeNode) override {

        activeNode->paintDevice()->fill(QRect(0,0,1024,1024), KoColor(Qt::red, image->colorSpace()));
        m_mask = new KisTransparencyMask(image, "tmask");
        m_mask->setSelection(new KisSelection());
        m_mask->paintDevice()->clear();
        image->addNode(m_mask, activeNode);
        image->setWorkingThreadsLimit(8);
    }

    using utils::StrokeTester::modifyResourceManager;
    void modifyResourceManager(KoCanvasResourceProvider *manager,
                               KisImageWSP image) override {

        KoColor color(Qt::red, image->colorSpace());
        color.setOpacity(0.5);

        QVariant i;
        i.setValue(color);
        manager->setResource(KoCanvasResource::ForegroundColor, i);
    }

    KisStrokeStrategy* createStroke(KisResourcesSnapshotSP resources,
                                    KisImageWSP image) override {
        Q_UNUSED(image);

        resources->setCurrentNode(m_mask);

        KisFreehandStrokeInfo *strokeInfo = new KisFreehandStrokeInfo();

        std::unique_ptr<FreehandStrokeStrategy> stroke(
            new FreehandStrokeStrategy(resources, strokeInfo, kundo2_noi18n("Freehand Stroke")));

        return stroke.release();
    }

    void addPaintingJobs(KisImageWSP image,
                                 KisResourcesSnapshotSP resources) override
    {
        addPaintingJobs(image, resources, 0);
    }

    void addPaintingJobs(KisImageWSP image, KisResourcesSnapshotSP resources, int iteration) override {
        Q_UNUSED(iteration);
        Q_UNUSED(resources);


        KisPaintInformation pi1;
        KisPaintInformation pi2;

        pi1 = KisPaintInformation(QPointF(100, 100), 1.0);
        pi2 = KisPaintInformation(QPointF(800, 800), 1.0);

        std::unique_ptr<KisStrokeJobData> data(
                    new FreehandStrokeStrategy::Data(0, pi1, pi2));

        image->addJob(strokeId(), data.release());

        image->addJob(strokeId(), new KisAsynchronousStrokeUpdateHelper::UpdateData(true));
    }


    void checkDeviceIsEmpty(KisPaintDeviceSP dev, const QString &name)
    {
        const KoColorSpace *cs = dev->colorSpace();
        KisSequentialConstIterator it(dev, QRect(0,0,1024,1024));
        while (it.nextPixel()) {
            if (cs->opacityU8(it.rawDataConst()) > 0) {
                KIS_DUMP_DEVICE_2(dev, QRect(0,0,1024,1024), "image", "dd");
                qFatal("%s", QString("failed: %1").arg(name).toLatin1().data());
            }
        }
    }

    void beforeCheckingResult(KisImageWSP image, KisNodeSP activeNode) override {
        ENTER_FUNCTION() << ppVar(image) << ppVar(activeNode);

        clearImage(image, {activeNode});
        image->waitForDone();

        checkDeviceIsEmpty(m_mask->paintDevice(), "mask");
        checkDeviceIsEmpty(m_mask->parent()->projection(), "projection");
        checkDeviceIsEmpty(image->projection(), "image");
    }

private:
    KisMaskSP m_mask;
};

#include <KoResourcePaths.h>

void KisPaintOnTransparencyMaskTest::initTestCase()
{
    KoResourcePaths::addAssetType(ResourceType::Brushes, "data", FILES_DATA_DIR);
}

void KisPaintOnTransparencyMaskTest::test()
{
    for (int i = 0; i < 1000; i++) {
        PaintOnTransparencyMaskTester tester("testing_wet_circle.kpp");
        tester.testSimpleStrokeNoVerification();
    }
}

SIMPLE_TEST_MAIN(KisPaintOnTransparencyMaskTest)
