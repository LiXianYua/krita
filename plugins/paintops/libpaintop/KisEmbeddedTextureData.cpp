/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisEmbeddedTextureData.h"

#include <PkAuxTypes.h>
#include <kis_properties_configuration.h>
#include <KoResourceLoadResult.h>
#include <KisResourcesInterface.h>

#include <cstring>
#include <filesystem>

namespace {

// PkByteArray 缺 fromHex/toHex/fromBase64/toBase64（pk/variant/PkAuxTypes.h
// 只到 number/data/resize）。本文件只在 md5 摘要与 base64 模式字符串上用，
// 本地补最小实现，语义照 Qt 的字节数组类型：lowercase hex、标准 base64（'=' 结束、
// 非字母字符跳过）。
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

static PkString toHexString(const PkByteArray &ba)
{
    static const char digits[] = "0123456789abcdef";
    const char *d = ba.constData();
    const int n = ba.size();
    std::string out;
    out.reserve(static_cast<size_t>(2 * n));
    for (int i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(d[i]);
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0x0F]);
    }
    return PkString(out.c_str());
}

static int base64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static PkByteArray fromBase64String(const PkString &base64)
{
    const std::string s = base64.PkToUtf8();
    std::string decoded;
    decoded.reserve(s.size() * 3 / 4);
    int val = 0;
    int bits = 0;
    for (char c : s) {
        if (c == '=') break;
        const int v = base64Value(c);
        if (v < 0) continue;
        val = (val << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
            val &= (1 << bits) - 1;
        }
    }
    PkByteArray result;
    result.resize(static_cast<int>(decoded.size()));
    if (!decoded.empty()) {
        std::memcpy(result.data(), decoded.data(), decoded.size());
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

} // namespace


bool KisEmbeddedTextureData::isNull() const
{
    return md5Base64.isEmpty() && md5sum.isEmpty() && fileName.isEmpty() && name.isEmpty();
}

KisEmbeddedTextureData KisEmbeddedTextureData::fromPattern(KoPatternSP pattern)
{
    KisEmbeddedTextureData data;

    data.md5Base64 = toBase64String(fromHexString(pattern->md5Sum()));
    data.md5sum = pattern->md5Sum();
    data.fileName = pattern->filename();
    data.name = pattern->name();

    return data;
}


KoResourceLoadResult KisEmbeddedTextureData::tryFetchPattern(KisResourcesInterfaceSP resourcesInterface) const
{
    auto resourceSourceAdapter = resourcesInterface->source<KoPattern>(ResourceType::Patterns);

    PkString effectiveMd5Sum = md5sum;

    if (effectiveMd5Sum.isEmpty()) {
        const PkByteArray md5 = fromBase64String(md5Base64);
        effectiveMd5Sum = toHexString(md5);
    }

    return resourceSourceAdapter.bestMatchLoadResult(effectiveMd5Sum, fileName, name);
}

KoResourceLoadResult KisEmbeddedTextureData::tryLoadEmbeddedPattern() const
{
    PkString effectiveMd5Sum = md5sum;

    if (effectiveMd5Sum.isEmpty()) {
        const PkByteArray md5 = fromBase64String(md5Base64);
        effectiveMd5Sum = toHexString(md5);
    }

    PkString effectiveName = name;

    if (effectiveName.isEmpty() || effectiveName.PkToUtf8() != std::filesystem::path(effectiveName.PkToUtf8()).filename().string()) {
        effectiveName = PkString(std::filesystem::path(effectiveName.PkToUtf8()).stem().string().c_str());
    }

    KIS_SAFE_ASSERT_RECOVER(!patternBase64.isEmpty()) {
        // return a fail-link pattern
        return KoResourceSignature(ResourceType::Patterns, effectiveMd5Sum, fileName, effectiveName);
    }

    const PkByteArray ba = fromBase64String(patternBase64);
    return KoEmbeddedResource(KoResourceSignature(ResourceType::Patterns, effectiveMd5Sum, fileName, effectiveName), ba);
}


KoResourceLoadResult KisEmbeddedTextureData::loadLinkedPattern(KisResourcesInterfaceSP resourcesInterface) const
{
    KoResourceLoadResult result = tryFetchPattern(resourcesInterface);

    if (result.type() == KoResourceLoadResult::FailedLink && !patternBase64.isEmpty()) {
        result = tryLoadEmbeddedPattern();
    }

    return result;
}

bool KisEmbeddedTextureData::read(const KisPropertiesConfiguration *setting)
{
    md5Base64 = setting->getString("Texture/Pattern/PatternMD5");
    md5sum = setting->getString("Texture/Pattern/PatternMD5Sum");
    fileName = PkString(std::filesystem::path(setting->getString("Texture/Pattern/PatternFileName").PkToUtf8()).filename().string().c_str());
    name = setting->getString("Texture/Pattern/Name");
    patternBase64 = setting->getString("Texture/Pattern/Pattern");

    return true;
}

void KisEmbeddedTextureData::write(KisPropertiesConfiguration *setting) const
{
    setting->setProperty("Texture/Pattern/PatternMD5", md5Base64);
    setting->setProperty("Texture/Pattern/PatternMD5Sum", md5sum);
    setting->setProperty("Texture/Pattern/PatternFileName", fileName);
    setting->setProperty("Texture/Pattern/Name", name);
}
