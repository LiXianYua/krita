/*
 *  SPDX-FileCopyrightText: 2019 Tusooa Zhu <tusooa@vista.aero>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDocumentReplaceTest.h"

#include <KisDocument.h>
#include <PkScopedPointer.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_layer_utils.h>
#include <kis_types.h>

// D-03 把 KisDocument 从 libs/ui 搬进 libs/impex 后，测试不再经过 kritaui 的
// 初始化链：KisDocument 构造 addStorage → KisResourceLocator::addStorage →
// KisResourceCacheDb::addStorage 会因数据库未初始化报 "The database is not
// valid"（s_valid=false）。照 libs/resources/tests/ResourceTestHelper.h 的
// recreateDatabaseForATest 模式在本测试里最小复刻初始化序列（不 include 该头，
// 它在 S-02-b 锁内），全部落在临时目录，不碰真实 appdata。
#include <filesystem>
#include <KoResourcePaths.h>
#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <PkDebug.h>

class TestKisDocument : public KisDocument
{
public:
    TestKisDocument() : KisDocument() {}
};


void KisDocumentReplaceTest::init()
{
    m_doc = nullptr; // 防御：下方 PK_FAIL 提前 return 时避免悬空指针

    namespace fs = std::filesystem;
    const fs::path baseDir = fs::temp_directory_path() / "kis-document-replace-test";
    const fs::path resourcesDir = baseDir / "resources";
    fs::create_directories(resourcesDir);

    // AppDataLocation 重定向到临时目录，然后初始化缓存数据库与资源定位器。
    KoResourcePaths::s_overrideAppDataLocation = PkString(baseDir.c_str());
    const bool dbOk = KisResourceCacheDb::initialize(KoResourcePaths::getAppDataLocation());
    if (!dbOk) {
        const std::string message =
            "KisResourceCacheDb::initialize failed: " + KisResourceCacheDb::lastError().PkToUtf8();
        PK_FAIL(message);
    }
    const KisResourceLocator::LocatorError locatorError =
        KisResourceLocator::instance()->initialize(PkString(resourcesDir.c_str()));
    if (locatorError != KisResourceLocator::LocatorError::Ok) {
        PK_FAIL("KisResourceLocator::initialize failed");
    }

    m_doc = new TestKisDocument;
    qDebug() << m_doc->newImage("test", 512, 512, KoColorSpaceRegistry::instance()->colorSpace("RGBA", "U8", 0), KoColor(), KisDocument::NewImageBackgroundStyle::RasterLayer, 1, "", 96);
}

void KisDocumentReplaceTest::finalize()
{
    delete m_doc;
    m_doc = 0;
}

void KisDocumentReplaceTest::testCopyFromDocument()
{
    init();
    PkScopedPointer<KisDocument> clonedDoc(m_doc->lockAndCreateSnapshot());
    KisDocument *anotherDoc = new TestKisDocument;
    anotherDoc->newImage("test", 512, 512, KoColorSpaceRegistry::instance()->colorSpace("RGBA", "U8", 0), KoColor(), KisDocument::NewImageBackgroundStyle::RasterLayer, 2, "", 96);
    KisImageSP anotherImage(anotherDoc->image());
    KisNodeSP root(anotherImage->rootLayer());
    anotherDoc->copyFromDocument(*(clonedDoc.data()));
    // image pointer should not change
    PK_COMPARE(anotherImage.data(), anotherDoc->image().data());
    // root node should change
    PK_VERIFY(root.data() != anotherDoc->image()->rootLayer().data());
    // node count should be the same
    PkList<KisNodeSP> oldNodes, newNodes;
    KisLayerUtils::recursiveApplyNodes(clonedDoc->image()->root(), [&oldNodes](KisNodeSP node) { oldNodes << node; });
    KisLayerUtils::recursiveApplyNodes(anotherDoc->image()->root(), [&newNodes](KisNodeSP node) { newNodes << node; });
    PK_COMPARE(oldNodes.size(), newNodes.size());

    delete anotherDoc;
    finalize();
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt 的 ODR 硬规则；照 pk/time/tests/test_elapsed_timer.cpp:99）。
#include "pk_binder_kisdocumentreplacetest.inc"

SIMPLE_TEST_MAIN(KisDocumentReplaceTest)
