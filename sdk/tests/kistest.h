/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kistest.h 阻塞登记（S-06 Task 9）
//
// 本文件不进薄壳，保留 Qt 类型。KISTEST_MAIN 创建 GUI 应用对象并做资源目录
// 初始化（QStandardPaths/QLocale/QImageReader/QImageWriter），Pk 侧尚无应用
// 对象与图像文件 I/O 的对应物；依赖 KoTestConfig.h 等未剥头。
// 关闭条件：Pk 应用对象 + PkImage 文件 I/O（R-15）交付后按 simpletest.h 的
// R-10 模式改写。


// ===========================================================================
// R-77 Task 2 · pk 栈分支（KRITA_TESTSDK_PK_NATIVE）
//
// 上面的 [GAP] 登记描述的是 **Qt 分支**（未定义 KRITA_TESTSDK_PK_NATIVE 时）。
// 那条路径原封不动地留在下面 `#else` 里——字节就是 BASE 的字节，一行未动。
// 本块只**新增** pk 栈需要的最小面，形制照 sdk/tests/simpletest.h 的 R-10
// 端口化与 sdk/tests/filestest.h 的 R-77 端口化。
//
// 为什么必须有这一支：`testui.h` 只有两行 —— `#define TESTUI` +
// `#include "kistest.h"`（引号，先查 sdk/tests/ 自己目录 ⇒ 必然命中本文件），
// 而 41 个直接消费者 + 经 testui.h 的 18 个都靠本文件拿 KISTEST_MAIN。
// pk 测试栈（kritatestsdk_pk，不链 Qt5::Test/Widgets ⇒ 没有 Qt 头路径）上，
// 原来的无条件 Qt 头链在第 20-25 行就当掉（`'QApplication' file not found`）；
// 即便 Qt 头可达（混血 target 走 kritatestsdk），第 25 行拉进的真 Qt
// `qglobal.h` 也会与 pk 兼容头双份定义 —— 实测 `redefinition of 'qAbs'`
// @ libs/image/tests/{kis_fixed_paint_device_test,kis_transaction_test}。
// 补上本支之后，这些消费者**源码一个字节不用改**（锁外只剩
// `kis_add_test` → `pk_add_test` 这一处注册宏的机械改动）。
//
// 本支**给什么**（就这一样）：
//   KISTEST_MAIN(TestObject) → SIMPLE_TEST_MAIN(TestObject)
//   —— sdk/tests/simpletest.h 的 pk 入口：PkThread::registerMainThread()
//   （登记当前线程为主线程）+ PkThreadCallQueue::warmUpCurrentThread()（备好本线程
//   调用队列）+ PkTest::qExec。测试对象构造与退出码语义与 Qt 支同形。
//
// 本支**刻意不给**什么（逐条有据，见 R-77 Task 2 报告 §4.1）：
//   * `registerResources()` / `addResourceTypes()` / KisTestUiResource 一族：
//     pk 栈上真实调用点只有两个，都在**锁外**
//     （plugins/impex/libkra/tests/kis_kra_loader_test.cpp:218、
//      kis_kra_saver_test.cpp:647），且这两处**在 BASE 就是红的**；锁内没有任何
//     「可跑绿」的调用点需要它。给一个空实现只会让资源测试静默跑绿（假绿），
//     故**不引入**（R线-spec〈判据必须在收尾路径上〉）。
//   * `KRITA_PLUGIN_PATH` / `EXTRA_RESOURCE_DIRS` 的 qputenv：决策 D-12 之后插件层
//     是静态注册、没有可加载的插件 DSO（plugins/** 里 0 个 MODULE 目标，已由
//     S-09-g 落地），该环境变量在**任何**栈上都不再生效。
//   * QApplication / QLocale::setDefault / QStandardPaths::setTestModeEnabled /
//     AA_Use96Dpi / 键盘导航禁用：D-30 明写要删；pk 侧由 PkThread +
//     PkThreadCallQueue 承接（同 sdk/tests/simpletest.h 的那段注释）。
// ===========================================================================
#ifdef KRITA_TESTSDK_PK_NATIVE

#ifndef KISTEST
#define KISTEST

#include <simpletest.h>

#define KISTEST_MAIN(TestObject) SIMPLE_TEST_MAIN(TestObject)

#endif // KISTEST

#else


#ifndef KISTEST
#define KISTEST

#include <KoTestConfig.h>
#include <QApplication>
#include <simpletest.h>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QtTest/qtestsystem.h>
#include <set>
#include <QLocale>
#include <KisSynchronizedConnection.h>
#include <PkByteArray.h>
#include <PkStream.h>
#include <PkString.h>

/**
 * There is a hierarchy of libraries built on the kritaresources library
 * that provide resources:
 *
 * pigment: kocolorset, kosegmentgradient, kostopgradient, kopattern
 *   flake: koseexprscript, kogamutmask, kosvgsymbolcollection
 *     image: kispaintoppreset, kispsdlayerstyle
 *       brush: kisgbrbrush, kisimagepipebrush, kissvgbrush, kispngbrush
 *         ui: kiswindowloyout, kissession, kisworkspace
 *
 * Depending on which library the test links again, it should use
 *
 *  testresources.h
 *  testpigment.h
 *  testflake.h
 *  testimage.h
 *  testbrush.h
 *  testui.h
 *
 * To get the right KISTEST_MAIN for the resources it needs access to.
 *
 * This means that adding a new resource means not only adding it in
 * KisApplication, but also this file.
 */



#if defined(QT_NETWORK_LIB)
#  include <QtTest/qtest_network.h>
#endif
#include <QtTest/qtest_widgets.h>

#ifdef QT_KEYPAD_NAVIGATION
#  define QTEST_DISABLE_KEYPAD_NAVIGATION QApplication::setNavigationMode(Qt::NavigationModeNone);
#else
#  define QTEST_DISABLE_KEYPAD_NAVIGATION
#endif


#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTBRUSH) || defined(TESTIMAGE) || defined(TESTUI)
#include <QImageReader>
#include <QList>
#include <QByteArray>
#include <QStringList>
#include <QStandardPaths>
#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QImageWriter>

#include <KisResourceTypes.h>
#include <KisResourceLoaderRegistry.h>
#include <KisMimeDatabase.h>
#include <KisResourceLoader.h>
#include <KisResourceCacheDb.h>
#include <KisResourceLocator.h>
#include <KoResourcePaths.h>

#include <resources/KoSegmentGradient.h>
#include <resources/KoStopGradient.h>
#include <resources/KoColorSet.h>
#include <resources/KoPattern.h>

#if defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
#if defined HAVE_SEEXPR
#include <KisSeExprScript.h>
#endif
#include <resources/KoFontFamily.h>
#include <resources/KoGamutMask.h>
#include <resources/KoSvgSymbolCollectionResource.h>
#endif

#if defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
#include <kis_paintop_preset.h>
#include <kis_psd_layer_style.h>
#endif

#if defined(TESTBRUSH) || defined(TESTUI)
#include <kis_gbr_brush.h>
#include <kis_imagepipe_brush.h>
#include <kis_svg_brush.h>
#include <kis_png_brush.h>
#endif

namespace {

#if defined(TESTUI)
/**
 * UI tests need loaders for these resource types so that the shared resource
 * database can be initialized.  They do not need to construct or apply real
 * windows, sessions, or workspaces.  Keeping the test resource local avoids
 * pulling application-window APIs into every TESTUI executable.
 */
class KisTestUiResource final : public KoResource
{
public:
    KisTestUiResource(const PkString &filename, const PkString &type)
        : KoResource(filename)
        , m_type(type)
    {
    }

    KisTestUiResource(const KisTestUiResource &rhs)
        : KoResource(rhs)
        , m_type(rhs.m_type)
        , m_data(rhs.m_data)
    {
    }

    KoResourceSP clone() const override
    {
        return KoResourceSP(new KisTestUiResource(*this));
    }

    bool loadFromDevice(PkStream *device, KisResourcesInterfaceSP) override
    {
        m_data = device->readAll();
        setValid(true);
        return true;
    }

    bool saveToDevice(PkStream *device) const override
    {
        return device->write(m_data.constData(), m_data.size()) == m_data.size();
    }

    std::pair<PkString, PkString> resourceType() const override
    {
        return std::make_pair(m_type, PkString());
    }

private:
    PkString m_type;
    PkByteArray m_data;
};

class KisTestUiResourceLoader final : public KisResourceLoaderBase
{
public:
    using KisResourceLoaderBase::KisResourceLoaderBase;

    KoResourceSP create(const PkString &filename) override
    {
        return KoResourceSP(new KisTestUiResource(filename, resourceType()));
    }
};
#endif

void addResourceTypes()
{
#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
    // All Krita's resource types
    KoResourcePaths::addAssetType("markers", "data", "/styles/");
    KoResourcePaths::addAssetType("kis_pics", "data", "/pics/");
    KoResourcePaths::addAssetType("kis_images", "data", "/images/");
    KoResourcePaths::addAssetType("metadata_schema", "data", "/metadata/schemas/");
    KoResourcePaths::addAssetType("gmic_definitions", "data", "/gmic/");
    KoResourcePaths::addAssetType("kis_defaultpresets", "data", "/defaultpresets/");
    KoResourcePaths::addAssetType("psd_layer_style_collections", "data", "/asl");
    KoResourcePaths::addAssetType("kis_shortcuts", "data", "/shortcuts/");
    KoResourcePaths::addAssetType("kis_actions", "data", "/actions");
    KoResourcePaths::addAssetType("kis_actions", "data", "/pykrita");
    KoResourcePaths::addAssetType("icc_profiles", "data", "/color/icc");
    KoResourcePaths::addAssetType("icc_profiles", "data", "/profiles/");
    KoResourcePaths::addAssetType("tags", "data", "/tags/");
    KoResourcePaths::addAssetType("templates", "data", "/templates");
    KoResourcePaths::addAssetType("pythonscripts", "data", "/pykrita");
    KoResourcePaths::addAssetType("preset_icons", "data", "/preset_icons");

    // Make directories for all resources we can save, and tags
    QDir d;
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tags/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/asl/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bundles/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/brushes/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/gradients/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/paintoppresets/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/palettes/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/patterns/");
    // between 4.2.x and 4.3.0 there was a change from 'taskset' to 'tasksets'
    // so to make older resource folders compatible with the new version, let's rename the folder
    // so no tasksets are lost.
    if (d.exists(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/taskset/")) {
        d.rename(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/taskset/",
                 QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tasksets/");
    }
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tasksets/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/workspaces/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/input/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/pykrita/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/symbols/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/color-schemes/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/preset_icons/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/preset_icons/tool_icons/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/preset_icons/emblem_icons/");
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/gamutmasks/");
#if defined HAVE_SEEXPR
    d.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/seexpr_scripts/");
#endif
#endif

}

void registerResources()
{

#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (dir.exists("resourcecache.sqlite")) {
        bool result = dir.removeRecursively();
        qDebug() << "Result of deleting resourcecache.sqlite:" << (result);
    }

    addResourceTypes();

    KisResourceLoaderRegistry *reg = KisResourceLoaderRegistry::instance();

    QList<QByteArray> src = QImageReader::supportedMimeTypes();
    PkStringList allImageMimes;
    Q_FOREACH(const QByteArray ba, src) {
        if (QImageWriter::supportedMimeTypes().contains(ba)) {
            allImageMimes << PkString::PkFromUtf8(ba.constData(), ba.size());
        }
    }
    allImageMimes << KisMimeDatabase::mimeTypeForSuffix("pat");

    reg->add(new KisResourceLoader<KoPattern>(ResourceType::Patterns, ResourceType::Patterns, ResourceName::Patterns, allImageMimes));
    reg->add(new KisResourceLoader<KoSegmentGradient>(ResourceSubType::SegmentedGradients, ResourceType::Gradients, ResourceName::Gradients, PkStringList() << "application/x-gimp-gradient"));
    reg->add(new KisResourceLoader<KoStopGradient>(ResourceSubType::StopGradients, ResourceType::Gradients, ResourceName::Gradients, PkStringList() << "image/svg+xml"));

    reg->add(new KisResourceLoader<KoColorSet>(ResourceType::Palettes, ResourceType::Palettes, ResourceName::Palettes,
                                     PkStringList() << KisMimeDatabase::mimeTypeForSuffix("kpl")
                                               << KisMimeDatabase::mimeTypeForSuffix("gpl")
                                               << KisMimeDatabase::mimeTypeForSuffix("pal")
                                               << KisMimeDatabase::mimeTypeForSuffix("act")
                                               << KisMimeDatabase::mimeTypeForSuffix("aco")
                                               << KisMimeDatabase::mimeTypeForSuffix("css")
                                               << KisMimeDatabase::mimeTypeForSuffix("colors")
                                               << KisMimeDatabase::mimeTypeForSuffix("xml")
                                               << KisMimeDatabase::mimeTypeForSuffix("sbz")));

#endif

#if defined (TESTFLAKE) || defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
#if defined HAVE_SEEXPR
    reg->add(new KisResourceLoader<KisSeExprScript>(ResourceType::SeExprScripts, ResourceType::SeExprScripts, ResourceName::SeExprScripts, PkStringList() << "application/x-krita-seexpr-script"));
#endif
    reg->add(new KisResourceLoader<KoGamutMask>(ResourceType::GamutMasks, ResourceType::GamutMasks, ResourceName::GamutMasks, PkStringList() << "application/x-krita-gamutmasks"));
    reg->add(new KisResourceLoader<KoSvgSymbolCollectionResource>(ResourceType::Symbols, ResourceType::Symbols, ResourceName::Symbols, PkStringList() << "image/svg+xml"));
    reg->add(new KisResourceLoader<KoFontFamily>(ResourceType::FontFamilies, ResourceType::FontFamilies, ResourceName::FontFamilies, PkStringList() << "application/x-font-ttf" << "application/x-font-otf"));
#endif


#if defined(TESTIMAGE) || defined(TESTBRUSH) || defined(TESTUI)
     reg->add(new KisResourceLoader<KisPaintOpPreset>(ResourceType::PaintOpPresets, ResourceType::PaintOpPresets, ResourceName::PaintOpPresets, PkStringList() << "application/x-krita-paintoppreset"));
     reg->add(new KisResourceLoader<KisPSDLayerStyle>(ResourceType::LayerStyles,
                                                     ResourceType::LayerStyles,
                                                     ResourceType::LayerStyles,
                                                     PkStringList() << "application/x-photoshop-style"));
#endif

#if defined(TESTBRUSH) || defined(TESTUI)

    reg->add(new KisResourceLoader<KisGbrBrush>(ResourceSubType::GbrBrushes, ResourceType::Brushes, ResourceName::Brushes, PkStringList() << "image/x-gimp-brush"));
    reg->add(new KisResourceLoader<KisImagePipeBrush>(ResourceSubType::GihBrushes, ResourceType::Brushes, ResourceName::Brushes, PkStringList() << "image/x-gimp-brush-animated"));
    reg->add(new KisResourceLoader<KisSvgBrush>(ResourceSubType::SvgBrushes, ResourceType::Brushes, ResourceName::Brushes, PkStringList() << "image/svg+xml"));
    reg->add(new KisResourceLoader<KisPngBrush>(ResourceSubType::PngBrushes, ResourceType::Brushes, ResourceName::Brushes, PkStringList() << "image/png"));

#endif

#if defined(TESTUI)
    reg->add(new KisTestUiResourceLoader(ResourceType::WindowLayouts, ResourceType::WindowLayouts, ResourceName::WindowLayouts, PkStringList() << "application/x-krita-windowlayout"));
    reg->add(new KisTestUiResourceLoader(ResourceType::Sessions, ResourceType::Sessions, ResourceName::Sessions, PkStringList() << "application/x-krita-session"));
    reg->add(new KisTestUiResourceLoader(ResourceType::Workspaces, ResourceType::Workspaces, ResourceName::Workspaces, PkStringList() << "application/x-krita-workspace"));
#endif

#if defined(TESTRESOURCES) || defined(TESTPIGMENT) || defined (TESTFLAKE) || defined(TESTBRUSH) || defined(TESTIMAGE) || defined(TESTUI)
    const QByteArray appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toUtf8();
    if (!KisResourceCacheDb::initialize(PkString::PkFromUtf8(appDataDir.constData(), appDataDir.size()))) {
        qFatal("Could not initialize the resource cachedb");
    }

    KisResourceLocator::instance()->initialize(KoResourcePaths::getApplicationRoot() + PkString("/share/krita"));
#endif

}

#define KISTEST_MAIN(TestObject) \
int main(int argc, char *argv[]) \
{ \
    qputenv("LANGUAGE", "en"); \
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); \
    qputenv("QT_LOGGING_RULES", ""); \
    QStandardPaths::setTestModeEnabled(true); \
    qputenv("EXTRA_RESOURCE_DIRS", QByteArray(KRITA_RESOURCE_DIRS_FOR_TESTS)); \
    qputenv("KRITA_PLUGIN_PATH", QByteArray(KRITA_PLUGINS_DIR_FOR_TESTS)); \
    QApplication app(argc, argv); \
    app.setAttribute(Qt::AA_Use96Dpi, true); \
    QTEST_DISABLE_KEYPAD_NAVIGATION \
    registerResources(); \
    TestObject tc; \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}

}
#else
#define KISTEST_MAIN(TestObject) \
int main(int argc, char *argv[]) \
{ \
    qputenv("LANGUAGE", "en"); \
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); \
    qputenv("QT_LOGGING_RULES", ""); \
    qputenv("EXTRA_RESOURCE_DIRS", QByteArray(KRITA_RESOURCE_DIRS_FOR_TESTS)); \
    qputenv("KRITA_PLUGIN_PATH", QByteArray(KRITA_PLUGINS_DIR_FOR_TESTS)); \
    KisSynchronizedConnectionBase::setAutoModeForUnittestsEnabled(true); \
    QStandardPaths::setTestModeEnabled(true); \
    QApplication app(argc, argv); \
    app.setAttribute(Qt::AA_Use96Dpi, true); \
    QTEST_DISABLE_KEYPAD_NAVIGATION \
    TestObject tc; \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}
#endif


#endif

#endif // KRITA_TESTSDK_PK_NATIVE
