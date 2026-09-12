/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// R-61 常驻回归载体（(c) 项）：KisPngCodec::buildFile 的 forceSRGB 分支在无资源环境
// （EXTRA_RESOURCE_DIRS 未设）下的**降级语义**。
//
// 这**不是崩溃**：(c) 处（KisPngCodec.cpp:934 `dstProfile = p709SRGBProfile()`）对返回值
// **不解引用**，而是交给
// KoColorSpaceRegistry::instance()->colorSpace(dstModel, dstDepth, dstProfile)。
// 实测该入口（KoColorSpaceRegistry.cpp:533-538）在 `!profile` 时**退到 `colorSpace1(csID)`
// 的默认 RGBA/U8 空间**（lcms 内建 sRGB，rawData 588 字节），**非返空**。于是 device 被
// convertTo 到该默认空间，随后写出的 iCCP 就是这份内建 sRGB。
//
// 按 R-59 裁决 C：本线**不免除**宿主配资源目录 / 注册色彩引擎的义务，只保证违约时
// **不崩、有定义输出**。故本用例**不改产品代码**，只把这条差异**测出来并打印**：
//   · 降级时 iCCP 内容 == rgb8(nullptr)->profile()->rawData()（证明「退了默认剖面」）；
//   · 资源齐备时走 elle V2 sRGB 路径（iCCP 是否写出见实测打印）。
//
// 注意：PNG 的 iCCP chunk 载荷是 **zlib 压缩**的（PNG 规范 §11.3.3.2：剖面名 + 压缩方法
// + deflate 流）。故比对前先用 zlib 解压，再与 rawData() 逐字节比。载荷的原始字节另打印
// 长度与哈希，便于诊断。
//
// 命令行开关：默认（无参数）强制 EXTRA_RESOURCE_DIRS 缺席，保证条件是封闭的；手工传
// `--keep-resource-dirs` 才保留调用者设的 EXTRA_RESOURCE_DIRS（环境对照组用）。
// 形态照 R-61 T1 的 KisWebPExportNoResourceDirsTest 与 T2 的
// PkP709SRgbProfileNoResourceDirsDriver。

#include "KisPngCodec.h"

#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <LcmsEnginePlugin.h>
#include <PkByteArray.h>
#include <kis_paint_device.h>

#include <zlib.h>

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

// 临时 PNG：temp_directory_path() + 自增序号（照 KisPngCodecNoResourceDirsTest 的
// TemporaryPng），析构时删除，不在源码里落死路径。
class TemporaryPng
{
public:
    explicit TemporaryPng(const char *stem)
    {
        static int serial = 0;
        m_path = std::filesystem::temp_directory_path()
            / (std::string("kis-png-force-srgb-no-resource-dirs-") + stem + "-"
               + std::to_string(++serial) + ".png");
    }

    ~TemporaryPng()
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    std::string string() const { return m_path.string(); }

private:
    std::filesystem::path m_path;
};

struct PngChunk
{
    std::string type;
    std::vector<unsigned char> data;
};

// 读 PNG chunk 序列（类型 + 载荷）。魔数不对返回 false。照 R-59 载体
// KisPngCodecNoResourceDirsTest.cpp 的 readPngChunkTypes 形态，扩展成能取 chunk 内容。
bool readPngChunks(const std::string &path, std::vector<PngChunk> &chunks)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    const std::vector<char> raw((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
    const unsigned char magic[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (raw.size() < 8 || std::memcmp(raw.data(), magic, 8) != 0) {
        return false;
    }
    std::size_t i = 8;
    while (i + 8 <= raw.size()) {
        const std::uint32_t length = (std::uint8_t(raw[i]) << 24)
            | (std::uint8_t(raw[i + 1]) << 16)
            | (std::uint8_t(raw[i + 2]) << 8)
            | std::uint8_t(raw[i + 3]);
        const std::size_t dataStart = i + 8;
        if (dataStart + length > raw.size()) {
            return false;
        }
        PngChunk chunk;
        chunk.type.assign(raw.data() + i + 4, 4);
        chunk.data.assign(raw.begin() + dataStart, raw.begin() + dataStart + length);
        chunks.push_back(chunk);
        i = dataStart + length + 4; // 4 length + 4 type + length data + 4 CRC
        if (chunk.type == "IEND") {
            break;
        }
    }
    return true;
}

const PngChunk *findChunk(const std::vector<PngChunk> &chunks, const char *name)
{
    for (const PngChunk &chunk : chunks) {
        if (chunk.type == name) {
            return &chunk;
        }
    }
    return nullptr;
}

std::string joinTypes(const std::vector<PngChunk> &chunks)
{
    std::string joined;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        if (i) {
            joined += ' ';
        }
        joined += chunks[i].type;
    }
    return joined;
}

// 从 iCCP chunk 载荷里解出 ICC 剖面字节：载荷 = 剖面名(Latin-1, NUL 结尾)
// + 压缩方法(1 字节, 0=deflate) + zlib 压缩流。
bool inflateIccp(const std::vector<unsigned char> &payload, std::vector<unsigned char> &icc)
{
    std::size_t nameEnd = 0;
    while (nameEnd < payload.size() && payload[nameEnd] != 0) {
        ++nameEnd;
    }
    if (nameEnd + 2 > payload.size()) {
        return false; // 需要 NUL 终止符 + 压缩方法字节
    }
    if (payload[nameEnd + 1] != 0) {
        return false; // 只支持 deflate
    }
    const unsigned char *src = payload.data() + nameEnd + 2;
    const std::size_t srcLen = payload.size() - nameEnd - 2;

    std::size_t capacity = 4096;
    for (int attempt = 0; attempt < 16; ++attempt) {
        icc.resize(capacity);
        uLongf destLen = static_cast<uLongf>(capacity);
        const int rc = ::uncompress(icc.data(), &destLen, src, static_cast<uLong>(srcLen));
        if (rc == Z_OK) {
            icc.resize(destLen);
            return true;
        }
        if (rc != Z_BUF_ERROR) {
            return false; // 不是「缓冲区太小」的错误 ⇒ 载荷本身有问题
        }
        capacity *= 4;
    }
    return false;
}

// FNV-1a 64 位：给「内容哈希」用，稳定、无外部依赖。
std::uint64_t fnv1a(const unsigned char *data, std::size_t size)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t fnv1a(const std::vector<unsigned char> &bytes)
{
    return fnv1a(bytes.data(), bytes.size());
}

std::uint64_t fnv1a(const PkByteArray &bytes)
{
    return fnv1a(reinterpret_cast<const unsigned char *>(bytes.constData()),
                 static_cast<std::size_t>(bytes.size()));
}

std::string hexHash(std::uint64_t hash)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%016llx",
                  static_cast<unsigned long long>(hash));
    return buffer;
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

bool bytesEqual(const std::vector<unsigned char> &a, const PkByteArray &b)
{
    if (a.size() != static_cast<std::size_t>(b.size())) {
        return false;
    }
    return a.empty() || std::memcmp(a.data(), b.constData(), a.size()) == 0;
}

} // namespace

int main(int argc, char **argv)
{
    // 1. 钉死「无资源」条件（默认）。`--keep-resource-dirs` 是环境对照组用的显式开关：
    //    只有手工传它时才保留调用者设的 EXTRA_RESOURCE_DIRS，默认路径下是 no-op。
    bool keepResourceDirs = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--keep-resource-dirs") == 0) {
            keepResourceDirs = true;
        }
    }
    if (!keepResourceDirs) {
        ::unsetenv("EXTRA_RESOURCE_DIRS");
    }

    // 2. 注册 lcms 引擎——elle 剖面（p709SRGBProfile() 的来源）由它从资源目录载入；
    //    不注册则 p709SRGBProfile() 取不到（**读码所得、未经探针实测**：本用例与所有探针
    //    都先注册引擎，从未喂过「未注册引擎」这一条件），区分不出本用例要区分的条件。
    registerLcmsEngine();

    // 3. 打印前提值：判据要的是「可观测的前提」，不是「假定的前提」。
    const char *extra = std::getenv("EXTRA_RESOURCE_DIRS");
    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    const KoColorProfile *srgbRef = registry->p709SRGBProfile();
    const KoColorSpace *fallbackCs = registry->rgb8(nullptr);
    const KoColorProfile *fallbackProfile = fallbackCs ? fallbackCs->profile() : nullptr;

    std::printf("PREMISE EXTRA_RESOURCE_DIRS=%s\n", extra ? extra : "(unset)");
    std::printf("PREMISE p709SRGBProfile=%s\n", srgbRef ? "non-null" : "nullptr");
    if (srgbRef) {
        std::printf("PREMISE p709SRGBProfile %s\n", describeProfile(srgbRef).c_str());
    }
    expect(fallbackProfile != nullptr, "rgb8(nullptr)->profile() must be non-null");
    std::printf("PREMISE rgb8(nullptr).profile %s\n", describeProfile(fallbackProfile).c_str());
    if (fallbackProfile) {
        std::printf("PREMISE rgb8(nullptr).profile rawDataHash=%s\n",
                    hexHash(fnv1a(fallbackProfile->rawData())).c_str());
    }

    const KoColorSpace *cs = registry->colorSpace("RGBA", "U8", PkString());
    expect(cs != nullptr, "RGBA/U8 colorspace must exist without resource dirs");
    if (!cs) {
        std::fflush(stdout);
        return 1;
    }
    std::printf("PREMISE device cs profile %s\n", describeProfile(cs->profile()).c_str());

    KisPaintDeviceSP device = new KisPaintDevice(cs);
    device->clear();

    TemporaryPng output("rgba8");

    KisPNGOptions options;
    options.forceSRGB = true;
    options.saveAsHDR = false;
    vKisAnnotationSP annotations;
    KisPngCodec codec;
    // 先 flush：若回归又在这里崩，进程异常退出会丢掉未刷出的 stdout 缓冲。
    std::fflush(stdout);
    const KisImportExportErrorCode result =
        codec.buildFile(PkString(output.string().c_str()), PkRect(0, 0, 8, 8), 72.0, 72.0,
                        device, annotations.begin(), annotations.end(), options, nullptr);

    // 4. 核心断言（(c) 是「不崩」的语义差异）：forceSRGB 分支本身不走 :1094 的裸解引用，
    //    实测应当直接过。若这里崩，那是新发现，停下来报。
    expect(result.isOk(),
           "buildFile with forceSRGB=true must return isOk() with EXTRA_RESOURCE_DIRS unset");

    // 5. 把差异测出来并打印（这是 (c)「要有人认」的实体）。
    std::vector<PngChunk> chunks;
    const bool validPng = readPngChunks(output.string(), chunks);
    expect(validPng, "output must start with the PNG magic");
    std::printf("PREMISE png chunk types: %s\n", joinTypes(chunks).c_str());
    expect(findChunk(chunks, "IHDR") != nullptr, "output must contain an IHDR chunk");
    expect(findChunk(chunks, "IDAT") != nullptr, "output must contain an IDAT chunk");
    expect(findChunk(chunks, "IEND") != nullptr, "output must contain an IEND chunk");

    const PngChunk *iccp = findChunk(chunks, "iCCP");
    std::printf("PREMISE iCCP present=%s\n", iccp ? "yes" : "no");
    std::vector<unsigned char> icc;
    bool inflated = false;
    if (iccp) {
        std::printf("PREMISE iCCP payload size=%lld hash=%s\n",
                    static_cast<long long>(iccp->data.size()),
                    hexHash(fnv1a(iccp->data)).c_str());
        inflated = inflateIccp(iccp->data, icc);
        expect(inflated, "iCCP payload must be a well-formed (name + deflate) ICC profile");
        if (inflated) {
            std::printf("PREMISE iCCP profile(ICC) size=%lld hash=%s\n",
                        static_cast<long long>(icc.size()),
                        hexHash(fnv1a(icc)).c_str());
        }
    }

    // 6. 条件断言：p709SRGBProfile() 为空 ⇒ forceSRGB 把 dstProfile 置空 ⇒ 退到默认
    //    RGBA/U8 空间（lcms 内建 sRGB）⇒ iCCP 必须逐字节等于该默认空间的剖面 rawData()。
    //    前提不成立（内核以后真有兜底）时打印并跳过，不 assert 前提——那会让「内核真的
    //    兜住了」变成一次假失败。
    if (!srgbRef) {
        expect(iccp != nullptr,
               "with p709SRGBProfile()==nullptr the degraded forceSRGB output must still "
               "carry an iCCP chunk");
        if (iccp && inflated && fallbackProfile) {
            expect(bytesEqual(icc, fallbackProfile->rawData()),
                   "degraded iCCP must byte-equal rgb8(nullptr)->profile()->rawData() "
                   "(the forceSRGB branch fell back to the default profile)");
        }
        std::printf("PREMISE degraded forceSRGB: pixels landed in the default RGBA/U8 space, "
                    "iCCP carries that space's profile (built-in lcms sRGB), not elle V2 sRGB\n");
    } else {
        std::printf("NOTE: p709SRGBProfile() is non-null in this environment; the degraded "
                    "forceSRGB path was not exercised, the elle V2 sRGB path was. The iCCP "
                    "presence printed above is the normal-path result (saveSRGBProfile "
                    "defaults to false). R-61 plan Task 3 Step 4 / 裁决 C.\n");
    }

    if (failures == 0) {
        std::cout << "KisPngCodec::buildFile forceSRGB survives with EXTRA_RESOURCE_DIRS "
                  << (extra ? "set" : "unset") << "\n";
    }
    return failures == 0 ? 0 : 1;
}
