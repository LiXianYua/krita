/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// R-59 常驻回归载体：EXTRA_RESOURCE_DIRS 未设（宿主没配资源目录）时，
// KisPngCodec::buildFile 不得对 p709SRGBProfile() 返回的 nullptr 解引用而段错误。
// 修前本用例的第 4 步（buildFile 返回 isOk()）根本走不到——进程当场 SIGSEGV。
// 用例同时把「拿不到参照剖面 ⇒ sRGB=false ⇒ 走既有分支写设备真实剖面的 iCCP」这条
// 设计钉住（见 R-59 计划 §2 裁决 B）。

#include "KisPngCodec.h"

#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <LcmsEnginePlugin.h>
#include <kis_paint_device.h>

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

// 临时 PNG：temp_directory_path() + 自增序号（照 KisPkImageFileDecoderAdapterTest 的
// TemporaryFixture），析构时删除，不在源码里落死路径。
class TemporaryPng
{
public:
    explicit TemporaryPng(const char *stem)
    {
        static int serial = 0;
        m_path = std::filesystem::temp_directory_path()
            / (std::string("kis-png-no-resource-dirs-") + stem + "-"
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

// 读 PNG chunk 类型序列。魔数不对返回 false。
bool readPngChunkTypes(const std::string &path, std::vector<std::string> &types)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
    const unsigned char magic[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (bytes.size() < 8 || std::memcmp(bytes.data(), magic, 8) != 0) {
        return false;
    }
    std::size_t i = 8;
    while (i + 8 <= bytes.size()) {
        const std::uint32_t length = (std::uint8_t(bytes[i]) << 24)
            | (std::uint8_t(bytes[i + 1]) << 16)
            | (std::uint8_t(bytes[i + 2]) << 8)
            | std::uint8_t(bytes[i + 3]);
        const std::string type(bytes.data() + i + 4, 4);
        types.push_back(type);
        i += 12 + length; // 4 length + 4 type + length data + 4 CRC
        if (type == "IEND") {
            break;
        }
    }
    return true;
}

bool hasChunk(const std::vector<std::string> &types, const char *name)
{
    for (const std::string &type : types) {
        if (type == name) {
            return true;
        }
    }
    return false;
}

std::string joinTypes(const std::vector<std::string> &types)
{
    std::string joined;
    for (std::size_t i = 0; i < types.size(); ++i) {
        if (i) {
            joined += ' ';
        }
        joined += types[i];
    }
    return joined;
}

} // namespace

int main()
{
    // 1. 把用例钉死在「无资源」条件：本任务要钉的就是这一刻，不是随手清环境。
    ::unsetenv("EXTRA_RESOURCE_DIRS");

    // 2. 注册 lcms 引擎——不注册则 p709SRGBProfile() 恒为 nullptr，区分不出条件。
    registerLcmsEngine();

    // 3. 打印前提值：判据要的是「可观测的前提」，不是「假定的前提」。
    const char *extra = std::getenv("EXTRA_RESOURCE_DIRS");
    const KoColorProfile *srgbRef = KoColorSpaceRegistry::instance()->p709SRGBProfile();
    std::printf("PREMISE EXTRA_RESOURCE_DIRS=%s\n", extra ? extra : "(unset)");
    std::printf("PREMISE p709SRGBProfile=%s\n", srgbRef ? "non-null" : "nullptr");

    const KoColorSpace *cs =
        KoColorSpaceRegistry::instance()->colorSpace("RGBA", "U8", PkString());
    expect(cs != nullptr, "RGBA/U8 colorspace must still be available without resource dirs");
    if (!cs) {
        return 1;
    }
    const KoColorProfile *deviceProfile = cs->profile();
    std::printf("PREMISE deviceProfile name=%s rawDataSize=%lld\n",
                deviceProfile ? deviceProfile->name().toUtf8().constData() : "(null)",
                deviceProfile ? static_cast<long long>(deviceProfile->rawData().size()) : -1LL);
    expect(deviceProfile != nullptr,
           "device colorspace profile must be non-null even without resource dirs");

    KisPaintDeviceSP device = new KisPaintDevice(cs);
    device->clear();

    TemporaryPng output("rgba8");

    KisPNGOptions options;
    vKisAnnotationSP annotations;
    KisPngCodec codec;
    // 先 flush：若回归又在这里崩，进程异常退出会丢掉未刷出的 stdout 缓冲，
    // 上面那几行前提值就看不见了——它们正是崩溃时要用的东西。
    std::fflush(stdout);
    const KisImportExportErrorCode result =
        codec.buildFile(PkString(output.string().c_str()), PkRect(0, 0, 8, 8), 72.0, 72.0,
                        device, annotations.begin(), annotations.end(), options, nullptr);

    // 4. 核心断言：修前这里根本到不了——buildFile 在无资源时 SIGSEGV。
    expect(result.isOk(), "buildFile must return isOk() with EXTRA_RESOURCE_DIRS unset");

    std::vector<std::string> chunkTypes;
    const bool validPng = readPngChunkTypes(output.string(), chunkTypes);
    expect(validPng, "output must start with the PNG magic");
    std::printf("PREMISE png chunk types: %s\n", joinTypes(chunkTypes).c_str());
    expect(hasChunk(chunkTypes, "IHDR"), "output must contain an IHDR chunk");
    expect(hasChunk(chunkTypes, "IDAT"), "output must contain an IDAT chunk");
    expect(hasChunk(chunkTypes, "IEND"), "output must contain an IEND chunk");

    // 5. 条件断言：拿不到参照剖面 ⇒ sRGB=false ⇒ 走既有的 !sRGB 分支写设备真实剖面的 iCCP。
    //    前提不成立（内核以后真有了兜底 sRGB 剖面）时打印并跳过，而不是 assert 前提——
    //    那会让「内核真的兜住了」变成一次假失败。
    if (!srgbRef) {
        expect(hasChunk(chunkTypes, "iCCP"),
               "with p709SRGBProfile()==nullptr the device's real profile must be written as iCCP");
        std::printf("PREMISE iCCP present as designed\n");
    } else {
        std::printf("NOTE: p709SRGBProfile() is non-null in this environment; the iCCP design "
                    "assertion is skipped (R-59 plan Task 2 Step 2.6).\n");
    }

    if (failures == 0) {
        std::cout << "KisPngCodec::buildFile survives an unset EXTRA_RESOURCE_DIRS\n";
    }
    return failures == 0 ? 0 : 1;
}
