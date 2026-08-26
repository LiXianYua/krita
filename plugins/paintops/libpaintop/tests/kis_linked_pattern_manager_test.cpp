/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_linked_pattern_manager_test.h"

// 原 #include <testresources.h>（→ sdk/tests/kistest.h，Qt-tainted）已删除，改为
// 显式 include 本测试真正用到的头。kistest.h 是跨锁阻塞（归 S-00 后续/I 线）：
// 它建 QApplication + 资源目录，Pk 侧要等 S-00 交付 PkObject 应用对象 + PkImage
// 文件 I/O（R-15）后才解锁。本测试还依赖 KoResourceServer/KisResourceModel 资源
// 系统与 PkTest::currentTestFunction（S0 交付），壳内跑不了——源码先迁 Qt-free
// 并登记缺口，资源系统剥完前这条测试路径无法编译运行。
// 原 #include <QPainter> 已删除：画图 fixture 改 PkImage 像素直写（见
// createPattern 与 sourceOverArgb32，字节等价）。

#include <filesystem>

#include <KoResourceServer.h>
#include <resources/KoPattern.h>
#include <KisResourceTypes.h>
#include <KisMimeDatabase.h>
#include <KisResourceLoaderRegistry.h>

#include "KisEmbeddedTextureData.h"

#include <kis_properties_configuration.h>
#include <KisGlobalResourcesInterface.h>
#include <KoResourceLoadResult.h>
#include <KoMD5Generator.h>

#include <kis_assert.h>

#include <PkAuxTypes.h>      // PkByteArray
#include <PkImage.h>         // PkImage（KoPattern.h 已带，直接引用更清晰）
#include <PkMemoryStream.h>  // libs/store

#include <simpletest.h>

// PkTest::currentTestFunction：S0 待交付的取值器（pk/test/README.md 登记，唯一
// 真实调用点就是本测试）。PkTest.h 目前只有 currentDataTag，它没有声明/定义——
// 这里前向声明占位，让本文件在真树里可编译；S0 交付后与头文件声明一致，不冲突。
namespace PkTest {
const char *currentTestFunction();
}

// ---- fixture：QPainter::fillRect → 像素直写（字节等价） ----
//
// 原 fixture 用 QPainter::fillRect(100,100,312,312, fillColor) 在 fill(255) 写出的
// 底上画方块；改 PkImage::setPixelColor 直写，并复刻 Qt 5.15 ARGB32 SourceOver
// 合成路径。实测对 13 个真实测试色（由实际文件名 MD5 推出）与通用不透明背景
// 逐字节等于真 Qt QPainter（探针 Qt 5.15.7 offscreen，与 krita-ci-env 同源）。
// Qt 在 ARGB32 上把色转 premultiplied 合成，再 unpremultiply 存回。
static inline uint32_t byteMulArgb32(uint32_t c, uint32_t a)
{
    uint32_t t = (c & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;
    c = ((c >> 8) & 0xff00ff) * a;
    c = (c + ((c >> 8) & 0xff00ff) + 0x800080);
    c &= 0xff00ff00;
    c |= t;
    return c & 0xff;
}

static inline uint32_t unpremultiplyChannelArgb32(uint32_t c, uint32_t a)
{
    if (a == 0) return 0;
    return (2 * c * 255 + a - 1) / (2 * a);
}

static inline uint32_t sourceOverArgb32(uint32_t src, uint32_t dst)
{
    const uint32_t sa = (src >> 24) & 0xff;
    const uint32_t da = (dst >> 24) & 0xff;
    const uint32_t sr = byteMulArgb32((src >> 16) & 0xff, sa);
    const uint32_t sg = byteMulArgb32((src >> 8) & 0xff, sa);
    const uint32_t sb = byteMulArgb32(src & 0xff, sa);
    const uint32_t dr = byteMulArgb32((dst >> 16) & 0xff, da);
    const uint32_t dg = byteMulArgb32((dst >> 8) & 0xff, da);
    const uint32_t db = byteMulArgb32(dst & 0xff, da);
    const uint32_t inv = 255 - sa;
    const uint32_t or_ = sr + byteMulArgb32(dr, inv);
    const uint32_t og = sg + byteMulArgb32(dg, inv);
    const uint32_t ob = sb + byteMulArgb32(db, inv);
    const uint32_t oa = sa + byteMulArgb32(da, inv);
    return (oa << 24) | (unpremultiplyChannelArgb32(or_, oa) << 16)
         | (unpremultiplyChannelArgb32(og, oa) << 8)
         | unpremultiplyChannelArgb32(ob, oa);
}

// ---- PkByteArray 的 hex/base64 兜底（R-31：pk/variant/PkAuxTypes.h 只到
// number/data/resize，缺 fromHex/toBase64）。实现照 KisEmbeddedTextureData.cpp 的
// 同名单函数——lowercase hex、标准 base64（'=' 结束），保证字符串与真 Qt 等价。----
static int hexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static PkByteArray fromHexString(const PkString &hex)
{
    const std::string s = hex.PkToUtf8();
    PkByteArray result;
    result.resize(static_cast<int>(s.size() / 2));
    char *out = result.data();
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        out[i / 2] = static_cast<char>((hexNibble(s[i]) << 4) | hexNibble(s[i + 1]));
    }
    return result;
}

static PkString toBase64String(const PkByteArray &ba)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *d = ba.constData();
    const int n = ba.size();
    std::string out;
    out.reserve(static_cast<size_t>(4 * ((n + 2) / 3)));
    for (int i = 0; i < n; i += 3) {
        const unsigned int b0 = static_cast<unsigned char>(d[i]);
        const unsigned int b1 = (i + 1 < n) ? static_cast<unsigned char>(d[i + 1]) : 0;
        const unsigned int b2 = (i + 2 < n) ? static_cast<unsigned char>(d[i + 2]) : 0;
        out.push_back(b64[b0 >> 2]);
        out.push_back(b64[((b0 & 0x03) << 4) | (b1 >> 4)]);
        out.push_back((i + 1 < n) ? b64[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=');
        out.push_back((i + 2 < n) ? b64[b2 & 0x3F] : '=');
    }
    return PkString(out.c_str());
}

KoPatternSP createPattern(const PkString &name, const PkString &fileName)
{
    PkImage image(512, 512, PkImage::Format_ARGB32);
    image.fill(255);

    /**
     * Make sure that MD5 of every generated resource is different
     */
    const std::string hashInput = fileName.PkToUtf8();
    const PkString hash = KoMD5Generator::generateHash(PkByteArray(hashInput.c_str(), static_cast<int>(hashInput.size())));

    // 原 QColor(hash[0], hash[1], hash[2], hash[3])：R=hash[0] G=hash[1] B=hash[2] A=hash[3]。
    // 底是 fill(255) 写出的 0x000000ff（透明蓝），fillRect 实际是 fillColor 叠在它上面。
    const uint32_t fillColorArgb = (uint32_t(hash[3]) << 24)
                                 | (uint32_t(hash[0]) << 16)
                                 | (uint32_t(hash[1]) << 8)
                                 | uint32_t(hash[2]);
    const uint32_t rectColor = sourceOverArgb32(fillColorArgb, 0x000000ff);
    for (int y = 100; y < 412; ++y) {
        for (int x = 100; x < 412; ++x) {
            image.setPixelColor(x, y, rectColor);
        }
    }

    KoPatternSP pattern (new KoPattern(image, name, fileName));
    return pattern;
}

KoResourceServer<KoPattern> *patternServer()
{
    static KoResourceServer<KoPattern> server(ResourceType::Patterns);
    return &server;
}


void KisLinkedPatternManagerTest::testRoundTrip_data()
{
    PkTest::addColumn<PkString>("loadMode");

    PkTest::newRow("old-md5") << "old-md5";
    PkTest::newRow("new-md5") << "new-md5";
    PkTest::newRow("name") << "name";
    PkTest::newRow("filename") << "filename";
    PkTest::newRow("filename-with-path") << "filename-with-path";
}

void KisLinkedPatternManagerTest::testRoundTrip()
{
    PK_FETCH(PkString, loadMode);

    const PkString tagName(PkTest::currentDataTag());
    const PkString fileName = PkString(PkTest::currentTestFunction()) + "_" + tagName + "_pattern.pat";
    const PkString name = PkString(PkTest::currentTestFunction()) + "_" + tagName + "_pattern";

    KoPatternSP pattern = createPattern(name, fileName);

    KoResourceServer<KoPattern> *resourceServer = patternServer();
    resourceServer->addResource(pattern);

    KisPropertiesConfigurationSP config(new KisPropertiesConfiguration);

    KisEmbeddedTextureData data1 = KisEmbeddedTextureData::fromPattern(pattern);
    data1.write(config.data());

    /**
     * Test if each of the four tags alone is enough to load the pattern.
     * Basically, we test if each tag is round-tripped correctly
     */

    if (loadMode != "old-md5") {
        config->removeProperty("Texture/Pattern/PatternMD5");
    }

    if (loadMode != "new-md5") {
        config->removeProperty("Texture/Pattern/PatternMD5Sum");
    }

    if (loadMode != "name") {
        config->removeProperty("Texture/Pattern/Name");
    }

    if (loadMode != "filename") {
        config->removeProperty("Texture/Pattern/PatternFileName");
    }

    if (loadMode == "filename-with-path") {
        PkString path = patternServer()->saveLocation() + "/" + fileName;
        config->setProperty("Texture/Pattern/PatternFileName", path);
    }

    KisEmbeddedTextureData data2;
    data2.read(config.data());

    KoResourceLoadResult result = data2.loadLinkedPattern(KisGlobalResourcesInterface::instance());

    PK_COMPARE(result.type(), KoResourceLoadResult::ExistingResource);
    KoPatternSP newPattern = result.resource<KoPattern>();
    PK_VERIFY(newPattern);

    PK_COMPARE(newPattern->pattern(), pattern->pattern());
    PK_COMPARE(newPattern->name(), pattern->name());
    PK_COMPARE(std::filesystem::path(newPattern->filename().PkToUtf8()).filename().string(),
              std::filesystem::path(pattern->filename().PkToUtf8()).filename().string());
}

void KisLinkedPatternManagerTest::init()
{
    // 原按 index(row,0)/resourceForIndex 逐行取；KisResourceModel 直接给
    // resources() 全量列表，等价。
    const PkVector<KoResourceSP> resourceList = patternServer()->resourceModel()->resources();
    for (const KoResourceSP &pa : resourceList) {
        if (pa) {
            patternServer()->removeResourceFile(pa->filename());
        }
    }
}

KisPropertiesConfigurationSP KisLinkedPatternManagerTest::createXML(SaveDataFlags flags,
                                                                    KoPatternSP pattern)
{

    KisPropertiesConfigurationSP setting(new KisPropertiesConfiguration);

    if (flags.testFlag(SaveFileName)) {
        setting->setProperty("Texture/Pattern/PatternFileName", pattern->filename());
    }

    if (flags.testFlag(SaveFileNameWithPath)) {
        PkString path = patternServer()->saveLocation() + "/" + pattern->filename();
        setting->setProperty("Texture/Pattern/PatternFileName", path);
    }

    if (flags.testFlag(SaveName)) {
        setting->setProperty("Texture/Pattern/Name", pattern->name());
    }

    if (flags.testFlag(SaveOldMd5Base64)) {
        PkString patternMD5 = pattern->md5Sum();
        KIS_ASSERT(!patternMD5.isEmpty());

        /// WARNING: KisPropertiesConfiguration saved QByteArray as a base64 string!
        ///          We don't do this conversion manually here!
        setting->setProperty("Texture/Pattern/PatternMD5",
                             toBase64String(fromHexString(patternMD5)));
    }

    if (flags.testFlag(SaveEmbeddedData)) {
        PkMemoryStream buffer;
        buffer.open(PkStream::WriteOnly);
        pattern->saveToDevice(&buffer);
        setting->setProperty("Texture/Pattern/Pattern",
                             toBase64String(PkByteArray(buffer.data(), static_cast<int>(buffer.size()))));
    }

    return setting;
}

KoPatternSP findOnServer(const PkString &md5)
{
    KoPatternSP pattern;

    if (!md5.isEmpty()) {
        return patternServer()->resource(md5, "", "");
    }

    return pattern;
}

void KisLinkedPatternManagerTest::testLoadingLegacyXML_data()
{
    PkTest::addColumn<bool>("isOnServer");
    PkTest::addColumn<SaveDataFlags>("saveDataFlags");


    PkTest::newRow("lnk-filename") << true << (SaveFileName | SaveEmbeddedData);
    PkTest::newRow("lnk-filename-with-path") << true << (SaveFileNameWithPath | SaveEmbeddedData);
    PkTest::newRow("lnk-name") << true << (SaveName | SaveEmbeddedData);
    PkTest::newRow("lnk-old-md5base64") << true << (SaveOldMd5Base64 | SaveEmbeddedData);

    PkTest::newRow("emb-filename") << false << (SaveFileName | SaveEmbeddedData);
    PkTest::newRow("emb-filename-with-path") << false << (SaveFileNameWithPath | SaveEmbeddedData);
    PkTest::newRow("emb-name") << false << (SaveName | SaveEmbeddedData);
    PkTest::newRow("emb-old-md5base64") << false << (SaveOldMd5Base64 | SaveEmbeddedData);

}

void KisLinkedPatternManagerTest::testLoadingLegacyXML()
{
    PK_FETCH(SaveDataFlags, saveDataFlags);
    PK_FETCH(bool, isOnServer);

    const PkString tagName(PkTest::currentDataTag());
    const PkString fileName = PkString(PkTest::currentTestFunction()) + "_" + tagName + "_pattern.pat";
    const PkString name = PkString(PkTest::currentTestFunction()) + "_" + tagName + "_pattern";
    PkSharedPointer<KoPattern> basePattern(createPattern(name, fileName));

    if (isOnServer) {
        // upload the resource to the server if requested
        patternServer()->addResource(basePattern);
        PK_VERIFY(findOnServer(basePattern->md5Sum()));
    }

    KisPropertiesConfigurationSP setting = createXML(saveDataFlags, basePattern);

    KisEmbeddedTextureData data2;
    data2.read(setting.data());

    KoResourceLoadResult result = data2.loadLinkedPattern(KisGlobalResourcesInterface::instance());

    if (isOnServer) {
        KoPatternSP pattern = result.resource<KoPattern>();

        PK_VERIFY(pattern);
        PK_COMPARE(pattern->pattern(), basePattern->pattern());
        PK_COMPARE(pattern->name(), basePattern->name());
        PK_COMPARE(pattern->filename(), basePattern->filename());
    } else {
        PK_COMPARE(result.type(), KoResourceLoadResult::EmbeddedResource);
        KoEmbeddedResource embeddedResource = result.embeddedResource();
        PK_VERIFY(embeddedResource.sanityCheckMd5());

        if (saveDataFlags.testFlag(SaveOldMd5Base64)) {
            PK_COMPARE(embeddedResource.signature().md5sum, basePattern->md5Sum());
        }

        /// WARNING: it seems like mimeTypeForFile doesn't handle it gracefully
        /// when the filename is empty. This code does **not** test handling of
        /// this issue in the real code.
        const PkString effectiveFileName =
                !embeddedResource.signature().filename.isEmpty() ?
                    embeddedResource.signature().filename :
                    basePattern->filename();

        KisResourceLoaderBase *loader =
            KisResourceLoaderRegistry::instance()->loader(
                embeddedResource.signature().type,
                KisMimeDatabase::mimeTypeForFile(effectiveFileName));

        PkByteArray ba = embeddedResource.data();
        // 原 QBuffer buf(&ba)：把已有字节包进只读流。PkMemoryStream 没有
        // wrap-from-bytes 构造（libs/store/PkMemoryStream.h 只有默认构造 +
        // data()/size()），用「先写后重开」等价替代：open(WriteOnly) 把字节灌进
        // 内部 vector，再 open(ReadOnly)——PkStream::open→setOpenMode 把游标拨回 0。
        // loader 只读消费，与 QBuffer 包外部字节的可观察行为一致。
        PkMemoryStream buf;
        buf.open(PkStream::WriteOnly);
        buf.write(ba.data(), ba.size());
        buf.open(PkStream::ReadOnly);

        KoResourceSP resource = loader->load(effectiveFileName, buf, KisGlobalResourcesInterface::instance());

        PK_VERIFY(resource);
        PK_COMPARE(resource->name(), basePattern->name());
        PK_COMPARE(resource->filename(), basePattern->filename());
        // NOTE: md5 is explicitly set by the loading code, we cannot
        //       verify it, since it can change after loading

        KoPatternSP loadedPattern = resource.dynamicCast<KoPattern>();
        PK_VERIFY(loadedPattern);
        PK_COMPARE(basePattern->pattern(), loadedPattern->pattern());
    }
}

SIMPLE_TEST_MAIN(KisLinkedPatternManagerTest)
