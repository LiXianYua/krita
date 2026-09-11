/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * paint_smoke —— M5 的验收载体：一个**纯 C++ CLI**（零 Qt 头、零 Qt 链接），
 * 证明剥离后的绘画内核能被非 Qt 程序驱动跑通三件事：
 *
 *   事① 建文档    KisDocumentRegistry::instance()->createDocument()
 *   事② 画一笔    KisTransaction -> KisPainter -> commit(undoAdapter)
 *   事③ 存 .kra   doc->exportDocumentSync(path, "application/x-krita") + 读回
 *
 * 落点 sdk/smoke/ 的依据（Ruling 3）：pk 层是**独立薄壳工程**（不接入 Krita
 * 主构建），而 paint_smoke 必须链 krita* 主树库；benchmarks/ 是 benchmark 的
 * 专属目录；sdk/ 正好是「宿主/驱动侧基础设施」（与 sdk/tests/ 并列）。
 *
 * 本文件的输入面与每一处写法的依据见 task-5-brief.md「实测出来的三件事的调用面」
 * 与「五个端口的现成实现」；对应关系见同目录 report 文件。
 */

// ---- pk 侧（零 Qt 内核接口） -------------------------------------------------
#include <PkByteArray.h>
#include <PkColor.h>
#include <PkPainterPath.h>
#include <PkRect.h>
#include <PkString.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>

// ---- 文档层（libs/impex） ----------------------------------------------------
#include <KisDocument.h>
#include <KisDocumentRegistry.h>
#include <KoDocumentInfo.h> // doc->documentInfo()->setAboutInfo(...) 需完整类型

// ---- 图像层（libs/image） ----------------------------------------------------
#include <kis_image.h>
#include <kis_group_layer.h> // rootLayer() 返回 KisGroupLayerSP，转 KisNodeSP 需完整类型
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <kis_surrogate_undo_adapter.h>
#include <kis_transaction.h>

// ---- 色彩层（libs/pigment） --------------------------------------------------
#include <KoColor.h>
#include <KoColorSpaceRegistry.h>

#include <cstdio>
#include <cstdlib> // getenv / setenv

// 资源根目录，由 CMakeLists.txt 提供（= ${CMAKE_SOURCE_DIR}/data）。
#ifndef PAINT_SMOKE_RESOURCE_DIRS
#define PAINT_SMOKE_RESOURCE_DIRS ""
#endif

// 三个静态注册入口（D-12 静态注册，S-09-g 已 VERIFIED；impex 侧 40 STATIC +
// 2 SHARED、0 MODULE ⇒ 直接链 + 直接调 extern "C"，不走 Qt 插件加载）。
// 按 plugins/register_all_plugins.cpp:44-59 与
// plugins/impex/libkra/tests/kis_kra_loader_test.cpp:31-33 的既有写法**自行声明**，
// 不 include 插件私有头 —— 那些目录不在 kritaimpex 的 include 接口里，include
// 它们会把插件的私有 include 路径硬编码进来。
extern "C" bool registerKraImportFilter();
extern "C" bool registerKraExportFilter();
void registerLcmsEngine();

namespace
{

int g_failures = 0;

void check(bool ok, const char *what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
}

// S-10 义务②：驱动侧的**持续 pump**。内核里没有任何隐式后台 pump，也不该有
// —— 投到某线程的调用，要等该线程**自己**调 processPendingCalls() 才会被抽干；
// 不 pump 的表现是纯静默的行为缺失（不报错、不崩溃、不打日志）。
// 真源：pk/concurrent/PkThreadCallQueue.h:26-30 与 sdk/tests/
// PkThreadCallQueuePumpHost.h:18-38。这里把它包一个短名，主流程每次需要抽干
// 队列时显式调用它。
void pump()
{
    PkThreadCallQueue::processPendingCalls();
}

} // namespace

int main(int argc, char **argv)
{
    const PkString outPath = argc > 1 ? PkString(argv[1]) : PkString("/tmp/paint_smoke_out.kra");

    // 资源根目录：registerLcmsEngine() 会经 KoResourcePaths 的 "icc_profiles"
    // 资产类型递归扫描 *.icc 并注册内置 ICC 剖面；KoColorSpaceRegistry::
    // p709SRGBProfile()（= profileByName("sRGB-elle-V2-srgbtrc.icc")）在扫描不到
    // 时返回 nullptr —— 而 KisPngCodec::buildFile（libs/impex/KisPngCodec.cpp:1094）
    // 会解引用它，届时直接段错误（实测：EXC_BAD_ACCESS 在 IccColorProfile::
    // operator== 里对空引用做 dynamic_cast）。真源：KoResourcePaths.cpp:1381-1393
    // 的 findExtraResourceDirs() 读 EXTRA_RESOURCE_DIRS（';' 分隔）。
    // 测试的同一做法见 KoTestConfig.h.cmake:7 + kistest.h:328。这里只在调用方
    // 没设过时兜底（setenv overwrite=0），不覆盖外部显式配置。
    {
        const char *extra = std::getenv("EXTRA_RESOURCE_DIRS");
        if (!extra || !*extra) {
            ::setenv("EXTRA_RESOURCE_DIRS", PAINT_SMOKE_RESOURCE_DIRS, 0);
        }
    }

    // S-10 义务①：**预热先于发布线程 id**，且**只发布它的返回值**。
    // warmUpCurrentThread() = processPendingCalls() + 返回 currentThreadId()
    // （pk/concurrent/PkThreadCallQueue.cpp）—— 它把「目标线程首次触达队列」
    // 的一次性陈旧条目判定提前消耗在一个必然为空的队列上，之后任何人投给
    // 这个 id 的调用都不会再被当成陈旧条目丢弃。这里把它的返回值原样用作
    // 本进程的「主线程 id」，不另外调用 currentThreadId() 去发布。
    const PkThreadId mainThreadId = PkThreadCallQueue::warmUpCurrentThread();
    PkThread::registerMainThread();

    // ---- S-10 义务③：契约验证 --------------------------------------------------
    // 契约：post → （不 pump）静默不执行 → pump → 执行。两半都要验。
    {
        bool executedBeforePump = false;
        PkThreadCallQueue::post(mainThreadId, [&executedBeforePump]() { executedBeforePump = true; });
        // 不 pump：投递的调用必须仍然停着（Qt 的 Queued 不因同线程而折叠成 Direct）。
        check(!executedBeforePump, "S-10③ 契约: post 后未 pump，调用静默不执行");
        pump();
        check(executedBeforePump, "S-10③ 契约: post 后 pump，调用被执行");
    }

    // ---- 静态注册 --------------------------------------------------------------
    // 先注册 lcms 引擎与 kra impex 过滤器，否则 exportDocumentSync / loadNativeFormat
    // 找不到 "application/x-krita" 的编解码器。
    registerLcmsEngine();
    // sRGB 内置剖面必须注册上，否则保存路径会在 KisPngCodec::buildFile 解引用
    // 空剖面而崩溃（见上面 EXTRA_RESOURCE_DIRS 的注释）。先验一条显式 check。
    check(KoColorSpaceRegistry::instance()->p709SRGBProfile() != nullptr,
          "资源: 内置 sRGB 剖面 (sRGB-elle-V2-srgbtrc.icc) 已注册");
    check(registerKraImportFilter(), "registerKraImportFilter() 注册成功");
    check(registerKraExportFilter(), "registerKraExportFilter() 注册成功");

    // ============================================================================
    // 事① 建文档
    // ============================================================================
    // KisDocument 的 ctor 是 protected + friend KisDocumentRegistry（libs/impex/
    // KisDocument.h），所以**只能**走 registry。KisPart / KisDocumentFactory 已
    // 从树上删除，工厂职责归 registry（libs/impex/KisDocumentRegistry.h:29）。
    KisDocument *doc = KisDocumentRegistry::instance()->createDocument();
    check(doc != nullptr, "事① 建文档: KisDocumentRegistry::instance()->createDocument() 非空");
    if (!doc) {
        std::printf("FATAL: 文档创建失败，后续步骤无法进行\n");
        return 1;
    }

    // ---- 建图 ------------------------------------------------------------------
    // 用 KisImage 构造 + setCurrentImage（= KisDocument::newImage 的等价底层路径，
    // KisDocument.h:609/:630）。KisDocumentApplicationServices::instance() 有
    // headless 默认实现（KisDocumentApplicationServices.cpp「deliberately headless:
    // it never opens a dialog」），所以这一步不需要桌面壳。
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 256, 256, cs, PkString("paint_smoke"));
    KisPaintLayerSP layer = new KisPaintLayer(image, PkString("paintlayer1"), OPACITY_OPAQUE_U8);
    image->addNode(layer, image->rootLayer());
    doc->setCurrentImage(image);
    doc->documentInfo()->setAboutInfo(PkString("title"), image->objectName());
    check(doc->image() != nullptr, "建图: doc->setCurrentImage() 后 doc->image() 非空");

    // ============================================================================
    // 事② 画一笔
    // ============================================================================
    // KisTransaction（快照前态）-> KisPainter 操作 -> commit(undoAdapter)（产出
    // undo 命令）。逐行先例：libs/image/tests/kis_painter_test.cpp:177-179（
    // KisPainter + FillStyleForegroundColor + fillPainterPath + end）与
    // libs/image/tests/kis_transaction_test.cpp:44（KisTransaction + commit）。
    // 没有哪一步被迫要 QPainter/QImage/QWidget。
    KisPaintDeviceSP dev = layer->paintDevice();
    KisSurrogateUndoAdapter undoAdapter;
    {
        KisTransaction transaction(dev); // 2 参重载：不带名字的快照
        KisPainter painter(dev);
        painter.setPaintColor(KoColor(PkColor(255, 0, 0), dev->colorSpace()));
        painter.setFillStyle(KisPainter::FillStyleForegroundColor);
        painter.setStrokeStyle(KisPainter::StrokeStyleNone);
        painter.setAntiAliasPolygonFill(true);
        PkPainterPath path;
        path.moveTo(32, 32);
        path.lineTo(224, 32);
        path.lineTo(224, 224);
        path.closeSubpath();
        painter.fillPainterPath(path, PkRect());
        painter.end();
        transaction.commit(&undoAdapter); // 把这一笔做成 undo 命令
    }
    // 保存前等内部线程收尾（先例 kis_kra_saver_test.cpp:133 的
    // doc->image()->waitForDone()）。不这么做 image 可能仍被占用、无法锁存。
    image->waitForDone();

    // 证明这一笔真的落进了像素（不是「调了没报错」）：读笔触内部的点。
    {
        PkColor px;
        const bool got = dev->pixel(100, 100, &px);
        check(got && px.red() > 200 && px.green() < 60 && px.blue() < 60,
              "事② 画一笔: 笔触内部像素 (100,100) 确为红色");
    }

    // ============================================================================
    // 事③ 存 .kra（并读回）
    // ============================================================================
    // 先例：plugins/impex/libkra/tests/util.h:196 createEmptyDocument() 的骨架 +
    // kis_kra_saver_test.cpp 的 saveKraFile/loadKraFile（建图→上色→存→读回全流程）。
    doc->setMimeType(PkByteArray("application/x-krita"));
    const bool saved = doc->exportDocumentSync(outPath, PkByteArray("application/x-krita"));
    check(saved, "事③ 存 .kra: exportDocumentSync(application/x-krita) 返回 true");

    KisDocument *doc2 = KisDocumentRegistry::instance()->createDocument();
    doc2->setMimeType(PkByteArray("application/x-krita"));
    const bool loaded = doc2->loadNativeFormat(outPath);
    check(loaded, "事③ 存 .kra: loadNativeFormat() 读回成功");

    std::printf("\nOUT_PATH=%s\n", outPath.toUtf8().constData());
    std::printf("RESULT=%s failures=%d\n", g_failures == 0 ? "done" : "stuck", g_failures);
    return g_failures == 0 ? 0 : 1;
}
