/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// R-61 常驻回归载体（(a) 项）。
//
// 场景：宿主没配资源目录（EXTRA_RESOURCE_DIRS 未设）+ 宿主未注册色彩引擎时，
// p709SRGBProfile() 返回 nullptr（R-59 裁决 A 的契约）。此时若导出配置要求
// force_srgb，KisWebPExport::convert 在 kis_webp_export.cpp:579 对
// `p709SRGBProfile()->rawData()` 裸解引用 —— 修前本用例走不到「导出成功」那一步，
// 进程在 convert 内当场 SIGSEGV（exit 139）。修后必须 exit 0。
//
// 本用例同时把 R-61 裁决 A 钉住：拿不到 elle sRGB 参照剖面时，写出的 ICCP 必须
// 逐字节等于「像素实际落进的那个空间」的剖面（convertToQImage(null,…) →
// rgb8(nullptr)），不是把 ICC 整块删掉、也不是凭空造一份。
//
// 命令行开关：默认（无参数）强制 EXTRA_RESOURCE_DIRS 缺席；手工传
// `--keep-resource-dirs` 才保留调用者设的 EXTRA_RESOURCE_DIRS（R-61 T1 Step 6 的
// 资源齐备对照组用）。
//
// 该调用点形状的复刻说明：真实生产路径是 KisDocument::exportDocumentSync —— 它
// 是唯一会把 KisDocument::savingImage() 置为非空的入口（kis_webp_export.cpp:163
// 的 kisImportExportSavingImage(document) 要求它非空）。直接 new KisWebPExport 后
// 调 convert() 会在 :164 image->bounds() 先崩（savingImage 为 null），到不了 :579。
// 故本 driver 走 exportDocumentSync，它内部仍调用同一个 KisWebPExport::convert。

#include <KisDocument.h>
#include <KisDocumentRegistry.h>

#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <LcmsEnginePlugin.h>
#include <PkByteArray.h>
#include <PkString.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_properties_configuration.h>
#include <kis_types.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

// kis_webp_export.cpp 里 extern "C" 定义（同 plugins/register_all_plugins.cpp:41 的声明）。
extern "C" bool registerKisWebPExportFilter();

namespace
{

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

// 临时 .webp：temp_directory_path() + 自增序号（照 KisPngCodecNoResourceDirsTest
// 的 TemporaryPng），析构时删除，不落死路径。
class TemporaryWebp
{
public:
    explicit TemporaryWebp(const char *stem)
    {
        static int serial = 0;
        m_path = std::filesystem::temp_directory_path()
            / (std::string("kis-webp-no-resource-dirs-") + stem + "-"
               + std::to_string(++serial) + ".webp");
    }

    ~TemporaryWebp()
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    std::string string() const { return m_path.string(); }

private:
    std::filesystem::path m_path;
};

bool readWholeFile(const std::string &path, std::vector<char> &bytes)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

// 扫 WEBP 顶层 chunk（RIFF 头 12 字节后：FourCC(4) + size(4, LE) + data，data
// 偶数对齐）。找到 ICCP 就把它的载荷拷出来。魔数不对返回 false。
bool readWebpIccp(const std::vector<char> &bytes, std::vector<char> &iccp)
{
    if (bytes.size() < 12) {
        return false;
    }
    if (std::memcmp(bytes.data(), "RIFF", 4) != 0
        || std::memcmp(bytes.data() + 8, "WEBP", 4) != 0) {
        return false;
    }
    std::size_t i = 12;
    while (i + 8 <= bytes.size()) {
        const std::string fourcc(bytes.data() + i, 4);
        const std::uint32_t size = std::uint8_t(bytes[i + 4])
            | (std::uint8_t(bytes[i + 5]) << 8)
            | (std::uint8_t(bytes[i + 6]) << 16)
            | (std::uint8_t(bytes[i + 7]) << 24);
        const std::size_t dataStart = i + 8;
        if (dataStart + size > bytes.size()) {
            return false;
        }
        if (fourcc == "ICCP") {
            iccp.assign(bytes.begin() + dataStart,
                        bytes.begin() + dataStart + size);
            return true;
        }
        i = dataStart + size + (size & 1u);
    }
    return false;
}

bool bytesEqual(const std::vector<char> &a, const PkByteArray &b)
{
    if (a.size() != static_cast<std::size_t>(b.size())) {
        return false;
    }
    return a.empty() || std::memcmp(a.data(), b.constData(), a.size()) == 0;
}

std::string describeProfile(const KoColorProfile *profile)
{
    if (!profile) {
        return "(null)";
    }
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "name=%s rawDataSize=%lld",
                  profile->name().toUtf8().constData(),
                  static_cast<long long>(profile->rawData().size()));
    return buffer;
}

} // namespace

int main(int argc, char **argv)
{
    // 1. 钉死「无资源」条件：本用例要证的就是这一刻。默认（ctest 调用）一律
    //    unsetenv，保证条件是封闭的、不随调用者环境漂移。
    //
    //    R-61 T1 Step 6 的「资源齐备」对照组需要相反的条件，而 brief 的 Step 1.1
    //    又要求这里无条件 unsetenv —— 两者物理上互斥。故给一个**显式**开关
    //    `--keep-resource-dirs`：只有手工传它时才保留调用者设的
    //    EXTRA_RESOURCE_DIRS，默认路径下这行是 no-op。对照组会在前提打印里看到
    //    EXTRA_RESOURCE_DIRS 的实际值，不会被冒充成「无资源」条件。
    bool keepResourceDirs = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--keep-resource-dirs") == 0) {
            keepResourceDirs = true;
        }
    }
    if (!keepResourceDirs) {
        ::unsetenv("EXTRA_RESOURCE_DIRS");
    }

    // 2. 注册 lcms 引擎 —— 不注册则 p709SRGBProfile() 恒为 nullptr，区分不出条件。
    registerLcmsEngine();

    // 3. 打印可观测的前提（不是假定的前提）。
    const char *extra = std::getenv("EXTRA_RESOURCE_DIRS");
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    const KoColorProfile *srgbRef = registry->p709SRGBProfile();
    const KoColorProfile *fallbackProfile = registry->rgb8(nullptr)->profile();

    std::printf("PREMISE EXTRA_RESOURCE_DIRS=%s\n", extra ? extra : "(unset)");
    std::printf("PREMISE p709SRGBProfile=%s\n", srgbRef ? "non-null" : "nullptr");
    if (srgbRef) {
        std::printf("PREMISE p709SRGBProfile %s\n", describeProfile(srgbRef).c_str());
    }
    std::printf("PREMISE rgb8(nullptr).profile %s\n",
                describeProfile(fallbackProfile).c_str());

    // 非 RGBA 源是让 needSrgbConversion 直接返 true 的确定路径（kis_webp_export.cpp:174）。
    const KoColorSpace *grayCs = registry->colorSpace(GrayAColorModelID.id(),
                                                      Integer8BitsColorDepthID.id(),
                                                      PkString());
    expect(grayCs != nullptr, "GRAYA/U8 colorspace must exist without resource dirs");
    if (!grayCs) {
        std::fflush(stdout);
        return 1;
    }
    std::printf("PREMISE image cs model=%s depth=%s profile %s\n",
                grayCs->colorModelId().id().toUtf8().constData(),
                grayCs->colorDepthId().id().toUtf8().constData(),
                describeProfile(grayCs->profile()).c_str());
    std::printf("PREMISE cs->profile()->hasColorants()=%d getTransferCharacteristics()=%d\n",
                grayCs->profile()->hasColorants(),
                static_cast<int>(grayCs->profile()->getTransferCharacteristics()));

    // 4. 把 webp 导出 filter 注册进 KisImportExportManager 的静态注册表 —— 不注册
    //    则 exportDocumentSync 找不到 image/webp 的 filter，走不到 convert。
    registerKisWebPExportFilter();

    // 5. 文档 + 一张非 RGBA 的图（GrayA/U8）。
    KisDocument *doc = KisDocumentRegistry::instance()->createDocument();
    expect(doc != nullptr, "KisDocumentRegistry::createDocument() must return a document");
    if (!doc) {
        std::fflush(stdout);
        return 1;
    }
    doc->setFileBatchMode(true);

    const bool created = doc->newImage(PkString("webp-no-resource-dirs"),
                                       8, 8,
                                       grayCs,
                                       KoColor(),
                                       KisDocument::NewImageBackgroundStyle::RasterLayer,
                                       1,
                                       PkString(),
                                       96.0);
    expect(created, "newImage must succeed");
    if (!created) {
        std::fflush(stdout);
        delete doc;
        return 1;
    }

    KisImageSP image = doc->image().toStrongRef();
    expect(image != nullptr, "document image must be non-null after newImage");
    if (!image) {
        std::fflush(stdout);
        delete doc;
        return 1;
    }
    std::printf("PREMISE projection cs model=%s depth=%s\n",
                image->projection()->colorSpace()->colorModelId().id().toUtf8().constData(),
                image->projection()->colorSpace()->colorDepthId().id().toUtf8().constData());

    // 6. force_srgb=true —— 不设它 :579 根本到不了（需要 needSrgbConversion 为真）。
    KisPropertiesConfigurationSP cfg(new KisPropertiesConfiguration());
    cfg->setProperty("force_srgb", true);

    TemporaryWebp output("graya");
    // 崩在 convert 内会丢掉未刷出的 stdout 缓冲 —— 上面几行前提值正是崩溃时要用的。
    std::fflush(stdout);
    const bool exportOk = doc->exportDocumentSync(PkString(output.string().c_str()),
                                                  PkByteArray("image/webp"),
                                                  cfg);

    // 7. 核心断言：修前这里根本到不了 —— convert 在无资源时 SIGSEGV。
    expect(exportOk, "exportDocumentSync must succeed with EXTRA_RESOURCE_DIRS unset");

    std::vector<char> bytes;
    const bool readOk = readWholeFile(output.string(), bytes);
    expect(readOk, "exported .webp file must be readable");
    std::printf("PREMISE exported size=%lld\n", static_cast<long long>(bytes.size()));
    expect(bytes.size() > 12, "exported .webp must not be trivial");
    expect(bytes.size() >= 4 && std::memcmp(bytes.data(), "RIFF", 4) == 0,
           "output must start with the RIFF magic");
    expect(bytes.size() >= 12 && std::memcmp(bytes.data() + 8, "WEBP", 4) == 0,
           "output must be a WEBP container");

    // 8. ICCP 逐字节 == 像素实际落进的空间的剖面（裁决 A 的「不藏」证明）。
    //    期望值与修后代码同源：needSrgbConversion 为真时，取得到 elle sRGB 就写它，
    //    否则写 rgb8(nullptr)（convertToQImage(null,…) 实际落到的那份剖面）。
    const KoColorProfile *expectedProfile = srgbRef ? srgbRef : fallbackProfile;
    std::printf("PREMISE expected ICC profile %s\n",
                describeProfile(expectedProfile).c_str());

    std::vector<char> iccp;
    const bool hasIccp = readWebpIccp(bytes, iccp);
    expect(hasIccp, "output must contain an ICCP chunk");
    if (hasIccp) {
        std::printf("PREMISE iccp size=%lld\n", static_cast<long long>(iccp.size()));
        expect(expectedProfile != nullptr, "expected ICC profile must be non-null");
        if (expectedProfile) {
            expect(bytesEqual(iccp, expectedProfile->rawData()),
                   "ICCP chunk must byte-equal the profile the pixels actually landed in");
        }
    }

    if (srgbRef) {
        std::printf("NOTE: p709SRGBProfile() is non-null in this environment; the "
                    "no-resource fallback branch was not exercised, the elle sRGB "
                    "profile path was (R-61 plan Task 1 Step 6).\n");
    } else {
        std::printf("PREMISE no-resource fallback: ICCP written from rgb8(nullptr) "
                    "profile as designed\n");
    }

    delete doc;

    if (failures == 0) {
        if (srgbRef) {
            std::cout << "KisWebPExport::convert OK with EXTRA_RESOURCE_DIRS set "
                         "(p709SRGBProfile non-null; elle sRGB path exercised)\n";
        } else {
            std::cout << "KisWebPExport::convert survives an unset EXTRA_RESOURCE_DIRS "
                         "with force_srgb\n";
        }
    }
    return failures == 0 ? 0 : 1;
}
