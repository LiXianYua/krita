/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt boud @valdyas.org
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <testui.h>

#include "kis_kra_loader_test.h"


#include <KisDocument.h>
#include <KoDocumentInfo.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorSpace.h>
#include <KoColor.h>

#include "kis_image.h"
#include <testutil.h>
#include "KisDocumentRegistry.h"

#include <filter/kis_filter_registry.h>
#include <generator/kis_generator_registry.h>

#include "kis_image_animation_interface.h"
#include "kis_keyframe_channel.h"
#include "kis_time_span.h"

#include <filestest.h>

extern "C" bool registerKraExportFilter();
extern "C" bool registerKraImportFilter();
void registerLcmsEngine();

const PkString KraMimetype = "application/x-krita";

namespace
{
bool loadKraFile(KisDocument *document, const PkString &fileName)
{
    document->setMimeType(KraMimetype.toUtf8());
    return document->loadNativeFormat(fileName);
}
}

void KisKraLoaderTest::initTestCase()
{
    KisFilterRegistry::instance();
    KisGeneratorRegistry::instance();
}

void KisKraLoaderTest::testLoading()
{
    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
    const bool loaded = loadKraFile(doc.data(), PkString(FILES_DATA_DIR) + '/' + "load_test.kra");
    PK_VERIFY2(loaded, doc->errorMessage().PkToUtf8());
    KisImageSP image = doc->image();
    image->waitForDone();
    PK_COMPARE(image->nlayers(), 12);
    PK_COMPARE(doc->documentInfo()->aboutInfo("title"), PkString("test image for loading"));
    PK_COMPARE(image->height(), 753);
    PK_COMPARE(image->width(), 1000);
    PK_COMPARE(image->colorSpace()->id(), KoColorSpaceRegistry::instance()->rgb8()->id());

    KisNodeSP node = image->root()->firstChild();
    PK_VERIFY(node);
    PK_COMPARE(node->name(), PkString("Background"));
    PK_VERIFY(node->inherits("KisPaintLayer"));

    node = node->nextSibling();
    PK_VERIFY(node);
    PK_COMPARE(node->name(), PkString("Group 1"));
    PK_VERIFY(node->inherits("KisGroupLayer"));
    PK_COMPARE((int) node->childCount(), 2);
}

void testObligeSingleChildImpl(bool transpDefaultPixel)
{

    PkString id = !transpDefaultPixel ?
        "single_layer_no_channel_flags_nontransp_def_pixel.kra" :
        "single_layer_no_channel_flags_transp_def_pixel.kra";

    PkString fileName = TestUtil::pkStringFromQString(
        TestUtil::fetchDataFileLazy(TestUtil::diagnosticQString(id)));

    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
    const bool result = loadKraFile(doc.data(), fileName);
    PK_VERIFY(result);

    KisImageSP image = doc->image();

    PK_VERIFY(image);
    PK_COMPARE(image->nlayers(), 2);

    KisNodeSP root = image->root();
    KisNodeSP child = root->firstChild();

    PK_VERIFY(child);

    PK_COMPARE(root->original(), root->projection());

    if (transpDefaultPixel) {
        PK_COMPARE(root->original(), child->projection());
    } else {
        PK_VERIFY(root->original() != child->projection());
    }
}

void KisKraLoaderTest::testObligeSingleChild()
{
    testObligeSingleChildImpl(true);
}

void KisKraLoaderTest::testObligeSingleChildNonTranspPixel()
{
    testObligeSingleChildImpl(false);
}

void KisKraLoaderTest::testLoadAnimated()
{
    PkScopedPointer<KisDocument> doc(KisDocumentRegistry::instance()->createDocument());
    const bool loaded = loadKraFile(doc.data(), PkString(FILES_DATA_DIR) + '/' + "load_test_animation.kra");
    PK_VERIFY2(loaded, doc->errorMessage().PkToUtf8());
    KisImageSP image = doc->image();

    KisNodeSP node1 = image->root()->firstChild();
    KisNodeSP node2 = node1->nextSibling();

    PK_VERIFY(node1->inherits("KisPaintLayer"));
    PK_VERIFY(node2->inherits("KisPaintLayer"));

    KisPaintLayerSP layer1 = dynamic_cast<KisPaintLayer*>(node1.data());
    KisPaintLayerSP layer2 = dynamic_cast<KisPaintLayer*>(node2.data());

    PK_VERIFY(layer1->isAnimated());
    PK_VERIFY(!layer2->isAnimated());

    KisKeyframeChannel *channel1 = layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id());
    PK_VERIFY(channel1);
    PK_COMPARE(channel1->keyframeCount(), 3);

    PK_COMPARE(image->animationInterface()->framerate(), 17);
    PK_COMPARE(image->animationInterface()->documentPlaybackRange(), KisTimeSpan::fromTimeToTime(15, 45));
    PK_COMPARE(image->animationInterface()->currentTime(), 19);

    KisPaintDeviceSP dev = layer1->paintDevice();

    const KoColorSpace *cs = dev->colorSpace();
    KoColor transparent(PkColor(0, 0, 0, 0), cs);
    KoColor white(PkColor(255, 255, 255), cs);
    KoColor red(PkColor(255, 0, 0), cs);

    image->animationInterface()->switchCurrentTimeAsync(0);
    image->waitForDone();

    const PkRect actualBounds = dev->exactBounds();
    PK_VERIFY2(actualBounds == PkRect(506, 378, 198, 198),
               PkString("actual bounds: x=%1 y=%2 width=%3 height=%4")
                   .arg(actualBounds.x())
                   .arg(actualBounds.y())
                   .arg(actualBounds.width())
                   .arg(actualBounds.height())
                   .PkToUtf8());
    PK_COMPARE(dev->x(), -26);
    PK_COMPARE(dev->y(), -128);
    PK_COMPARE(dev->defaultPixel(), transparent);

    image->animationInterface()->switchCurrentTimeAsync(20);
    image->waitForDone();

    PK_COMPARE(dev->nonDefaultPixelArea(), PkRect(615, 416, 129, 129));
    PK_COMPARE(dev->x(), 502);
    PK_COMPARE(dev->y(), 224);
    PK_COMPARE(dev->defaultPixel(), white);

    image->animationInterface()->switchCurrentTimeAsync(30);
    image->waitForDone();

    PK_COMPARE(dev->nonDefaultPixelArea(), PkRect(729, 452, 45, 44));
    PK_COMPARE(dev->x(), 645);
    PK_COMPARE(dev->y(), -10);
    PK_COMPARE(dev->defaultPixel(), red);
}



void KisKraLoaderTest::testImportFromWriteonly()
{
    TestUtil::testImportFromWriteonly(KraMimetype);
}


void KisKraLoaderTest::testImportIncorrectFormat()
{
    TestUtil::testImportIncorrectFormat(KraMimetype);
}



#ifdef PK_SHELL_MOC_BINDER
#include "pk_binder_kis_kra_loader_test.inc"
#endif

// R-82：本 main 从 Qt 测试栈切到 pk 测试栈（pk_add_test）。删掉的三族都已在
// `sdk/tests/kistest.h` 的 pk 分支里逐条登记过，**不是本任务的新判断**：
//   * `qputenv("LANGUAGE"/"QT_LOGGING_RULES"/"EXTRA_RESOURCE_DIRS"/
//     "KRITA_PLUGIN_PATH")`：决策 D-12 之后插件是静态注册、没有可加载的插件
//     DSO，这些环境变量在任何栈上都不再生效（kistest.h pk 分支同样不给；
//     本测试要的 kra filter 改由 `REGISTER_FILTERS` 静态拉入 —— 见本目录
//     CMakeLists.txt 的 `pk_add_test(... REGISTER_FILTERS registerKra*Filter ...)`）；
//   * `QLocale::setDefault` / `QStandardPaths::setTestModeEnabled` /
//     `QApplication` + `setAttribute(Qt::AA_Use96Dpi)`：D-30 明写要删；pk 侧没有
//     GUI 应用对象，「界定主线程 + 驱动事件循环」由下面的
//     `PkThread::registerMainThread()` + `PkThreadCallQueue::warmUpCurrentThread()`
//     承接（与 `SIMPLE_MAIN_IMPL` 同形）；
//   * `registerResources()`：R-77 已在 kistest.h 里裁定**不给空实现**
//     （空实现会让资源测试静默假绿），本处正是它点名的两个调用点之一。
// **断言、容差、测试用例一个未动**；保留的 `registerLcmsEngine()`、两个注册入口
// 调用与 `PkTest::qExec` 是原样带过来的。
int main(int argc, char *argv[])
{
    registerLcmsEngine();
    (void)registerKraExportFilter();
    (void)registerKraImportFilter();
    PkThread::registerMainThread();
    PkThreadCallQueue::warmUpCurrentThread();
    KisKraLoaderTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
