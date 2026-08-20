/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KisResourceCacheDb.h"

#include <PkSqlError.h>
#include <PkSqlQuery.h>
#include <PkSqlDatabase.h>
#include <PkDataStream.h>

#include <PkElapsedTimer.h>
#include <PkMessageLogger.h>
#include <PkSet.h>

#include <KritaVersionWrapper.h>

#include <KisBackup.h>

#include <kis_debug.h>
#include <KisUsageLogger.h>

#include <KisSqlQueryLoader.h>
#include <KisDatabaseTransactionLock.h>
#include "KisResourceThumbnailCodec.h"
#include <KisResourceLocator.h>
#include <KisResourceLoaderRegistry.h>
#include <KisResourceTypes.h>

#include <sqlite3.h>

#include "ResourceDebug.h"
#include <kis_assert.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <new>
#include <optional>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace {

PkString operator+(const char *lhs, const PkString &rhs)
{
    return PkString(lhs) + rhs;
}

struct SchemaVersion
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    static SchemaVersion fromString(const PkString &text)
    {
        SchemaVersion result;
        char trailing = '\0';
        if (std::sscanf(text.PkToUtf8().c_str(), "%d.%d.%d%c",
                        &result.major, &result.minor, &result.patch, &trailing) != 3) {
            return {};
        }
        return result;
    }

    PkString toString() const
    {
        return PkString((std::to_string(major) + "." + std::to_string(minor) + "." +
                         std::to_string(patch)).c_str());
    }

    friend bool operator==(const SchemaVersion &a, const SchemaVersion &b)
    {
        return std::tie(a.major, a.minor, a.patch) == std::tie(b.major, b.minor, b.patch);
    }
    friend bool operator!=(const SchemaVersion &a, const SchemaVersion &b) { return !(a == b); }
    friend bool operator<(const SchemaVersion &a, const SchemaVersion &b)
    {
        return std::tie(a.major, a.minor, a.patch) < std::tie(b.major, b.minor, b.patch);
    }
    friend bool operator>(const SchemaVersion &a, const SchemaVersion &b) { return b < a; }
    friend bool operator<=(const SchemaVersion &a, const SchemaVersion &b) { return !(b < a); }
    friend bool operator>=(const SchemaVersion &a, const SchemaVersion &b) { return !(a < b); }
};

PkString embeddedSql(const char *alias)
{
    return KisSqlQueryLoader::loadEmbeddedScript(PkString(alias));
}

bool endsWithAsciiCaseInsensitive(const PkString &value, const char *suffix)
{
    std::string text = value.PkToUtf8();
    std::string ending = suffix;
    auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    std::transform(text.begin(), text.end(), text.begin(), lower);
    std::transform(ending.begin(), ending.end(), ending.begin(), lower);
    return text.size() >= ending.size() &&
           text.compare(text.size() - ending.size(), ending.size(), ending) == 0;
}

PkString completeBaseName(const PkString &path)
{
    std::string name = std::filesystem::u8path(path.PkToUtf8()).filename().u8string();
    const std::size_t dot = name.rfind('.');
    if (dot != std::string::npos && dot != 0) name.resize(dot);
    std::replace(name.begin(), name.end(), '_', ' ');
    return PkString::PkFromUtf8(name.data(), static_cast<int>(name.size()));
}

PkString fileBaseName(const PkString &path)
{
    const std::string name = std::filesystem::u8path(path.PkToUtf8()).stem().u8string();
    return PkString::PkFromUtf8(name.data(), static_cast<int>(name.size()));
}

const char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Metadata is control-plane state, not a bulk-blob store (resource thumbnails
// have their own BLOB columns).  Bound both retained rows and retained Base64
// text before materializing SQLite values.  A single owner may retain at most
// 8 MiB across at most 1024 rows, and no one row may exceed 1 MiB.  Paged SQL
// returns only row ids and storage classes; SQLite's incremental-blob API
// measures and copies each TEXT/BLOB only after the bounds are known, so an
// over-limit value remains in SQLite without an extra unbounded application
// copy.
constexpr std::uint64_t kMaximumMetadataPayloadBytes = 1024u * 1024u;
constexpr std::uint64_t kMaximumMetadataPayloadBytesPerOwner = 8u * 1024u * 1024u;
constexpr std::uint64_t kMaximumMetadataKeyBytes = 16u * 1024u;
constexpr std::size_t kMaximumMetadataRowsPerOwner = 1024u;
constexpr int kMetadataReadPageRows = 32;

KisResourceCacheDb::MetaDataStorageClass metadataStorageClass(const PkString &name)
{
    if (name == PkString("null")) {
        return KisResourceCacheDb::MetaDataStorageClass::Null;
    }
    if (name == PkString("integer")) {
        return KisResourceCacheDb::MetaDataStorageClass::Integer;
    }
    if (name == PkString("real")) {
        return KisResourceCacheDb::MetaDataStorageClass::Real;
    }
    if (name == PkString("text")) {
        return KisResourceCacheDb::MetaDataStorageClass::Text;
    }
    if (name == PkString("blob")) {
        return KisResourceCacheDb::MetaDataStorageClass::Blob;
    }
    return KisResourceCacheDb::MetaDataStorageClass::Unknown;
}

bool isByteStorageClass(KisResourceCacheDb::MetaDataStorageClass storageClass)
{
    return storageClass == KisResourceCacheDb::MetaDataStorageClass::Text ||
           storageClass == KisResourceCacheDb::MetaDataStorageClass::Blob;
}

bool isValidUtf8(const PkByteArray &bytes)
{
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const std::size_t size = static_cast<std::size_t>(bytes.size());
    std::size_t offset = 0;
    while (offset < size) {
        const unsigned char first = data[offset++];
        if (first <= 0x7fu) {
            continue;
        }

        std::uint32_t codePoint = 0;
        std::size_t continuationCount = 0;
        std::uint32_t minimumCodePoint = 0;
        if (first >= 0xc2u && first <= 0xdfu) {
            codePoint = first & 0x1fu;
            continuationCount = 1;
            minimumCodePoint = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            codePoint = first & 0x0fu;
            continuationCount = 2;
            minimumCodePoint = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            codePoint = first & 0x07u;
            continuationCount = 3;
            minimumCodePoint = 0x10000u;
        } else {
            return false;
        }

        if (continuationCount > size - offset) {
            return false;
        }
        for (std::size_t index = 0; index < continuationCount; ++index) {
            const unsigned char continuation = data[offset++];
            if ((continuation & 0xc0u) != 0x80u) {
                return false;
            }
            codePoint = (codePoint << 6) | (continuation & 0x3fu);
        }
        if (codePoint < minimumCodePoint || codePoint > 0x10ffffu ||
            (codePoint >= 0xd800u && codePoint <= 0xdfffu)) {
            return false;
        }
    }
    return true;
}

struct BoundedMetadataColumn
{
    bool succeeded = false;
    bool complete = false;
    std::uint64_t byteCount = 0;
    PkByteArray bytes;
};

BoundedMetadataColumn readBoundedMetadataColumn(sqlite3 *database,
                                                long long rowId,
                                                const char *column,
                                                std::uint64_t maximumBytes)
{
    BoundedMetadataColumn result;
    sqlite3_blob *blob = nullptr;
    const int openResult = sqlite3_blob_open(database,
                                             "main",
                                             "metadata",
                                             column,
                                             static_cast<sqlite3_int64>(rowId),
                                             0,
                                             &blob);
    if (openResult != SQLITE_OK) {
        return result;
    }

    const int byteCount = sqlite3_blob_bytes(blob);
    result.byteCount = static_cast<std::uint64_t>(byteCount);
    result.succeeded = true;
    if (result.byteCount <= maximumBytes) {
        result.bytes.resize(byteCount);
        if (byteCount == 0 ||
            sqlite3_blob_read(blob, result.bytes.data(), byteCount, 0) == SQLITE_OK) {
            result.complete = true;
        } else {
            result.succeeded = false;
            result.bytes.resize(0);
        }
    }
    if (sqlite3_blob_close(blob) != SQLITE_OK) {
        result.succeeded = false;
        result.complete = false;
        result.bytes.resize(0);
    }
    return result;
}

PkString utf8BytesToPkString(const PkByteArray &bytes)
{
    return PkString::PkFromUtf8(bytes.constData(), bytes.size());
}

PkString base64Encode(const PkByteArray &bytes)
{
    std::string out;
    const std::size_t size = static_cast<std::size_t>(bytes.size());
    out.reserve(((size + 2) / 3) * 4);
    for (std::size_t i = 0; i < size; i += 3) {
        const auto byteAt = [&](std::size_t index) {
            return static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes.constData()[index]));
        };
        const std::uint32_t a = byteAt(i);
        const std::uint32_t b = i + 1 < size ? byteAt(i + 1) : 0;
        const std::uint32_t c = i + 2 < size ? byteAt(i + 2) : 0;
        const std::uint32_t value = (a << 16) | (b << 8) | c;
        out.push_back(kBase64[(value >> 18) & 63]);
        out.push_back(kBase64[(value >> 12) & 63]);
        out.push_back(i + 1 < size ? kBase64[(value >> 6) & 63] : '=');
        out.push_back(i + 2 < size ? kBase64[value & 63] : '=');
    }
    return PkString(out.c_str());
}

enum class Base64DecodeStatus
{
    Ok,
    Invalid,
    LimitExceeded
};

Base64DecodeStatus base64Decode(const PkString &text,
                                std::size_t outputLimit,
                                std::vector<std::uint8_t> &out)
{
    try {
        std::array<int, 256> decode;
        decode.fill(-1);
        for (int i = 0; i < 64; ++i) {
            decode[static_cast<unsigned char>(kBase64[i])] = i;
        }
        const std::string input = text.PkToUtf8();
        out.clear();
        if (input.empty() || input.size() % 4 != 0) return Base64DecodeStatus::Invalid;
        out.reserve(std::min(outputLimit, (input.size() / 4) * 3));

        const auto appendByte = [&](std::uint8_t byte) {
            if (out.size() >= outputLimit) return false;
            out.push_back(byte);
            return true;
        };

        for (std::size_t offset = 0; offset < input.size(); offset += 4) {
            const bool isLastQuartet = offset + 4 == input.size();
            const unsigned char c0 = static_cast<unsigned char>(input[offset]);
            const unsigned char c1 = static_cast<unsigned char>(input[offset + 1]);
            const unsigned char c2 = static_cast<unsigned char>(input[offset + 2]);
            const unsigned char c3 = static_cast<unsigned char>(input[offset + 3]);
            if (decode[c0] < 0 || decode[c1] < 0) return Base64DecodeStatus::Invalid;

            const std::uint32_t v0 = static_cast<std::uint32_t>(decode[c0]);
            const std::uint32_t v1 = static_cast<std::uint32_t>(decode[c1]);
            if (c2 == '=') {
                if (!isLastQuartet || c3 != '=' || (v1 & 0x0fu) != 0) {
                    return Base64DecodeStatus::Invalid;
                }
                return appendByte(static_cast<std::uint8_t>((v0 << 2) | (v1 >> 4)))
                    ? Base64DecodeStatus::Ok : Base64DecodeStatus::LimitExceeded;
            }
            if (decode[c2] < 0) return Base64DecodeStatus::Invalid;

            const std::uint32_t v2 = static_cast<std::uint32_t>(decode[c2]);
            if (!appendByte(static_cast<std::uint8_t>((v0 << 2) | (v1 >> 4))) ||
                !appendByte(static_cast<std::uint8_t>((v1 << 4) | (v2 >> 2)))) {
                return Base64DecodeStatus::LimitExceeded;
            }
            if (c3 == '=') {
                if (!isLastQuartet || (v2 & 0x03u) != 0) {
                    return Base64DecodeStatus::Invalid;
                }
                return Base64DecodeStatus::Ok;
            }
            if (decode[c3] < 0) return Base64DecodeStatus::Invalid;

            const std::uint32_t v3 = static_cast<std::uint32_t>(decode[c3]);
            if (!appendByte(static_cast<std::uint8_t>((v2 << 6) | v3))) {
                return Base64DecodeStatus::LimitExceeded;
            }
        }
        return Base64DecodeStatus::Ok;
    } catch (const std::bad_alloc &) {
        out.clear();
        return Base64DecodeStatus::LimitExceeded;
    } catch (const std::length_error &) {
        out.clear();
        return Base64DecodeStatus::LimitExceeded;
    }
}

class MetadataReadStream final : public PkStream
{
public:
    explicit MetadataReadStream(const std::vector<std::uint8_t> &bytes)
        : m_bytes(bytes)
    {
        open(ReadOnly);
    }

    pk_int64 size() const override
    {
        return static_cast<pk_int64>(m_bytes.size());
    }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const std::size_t offset = static_cast<std::size_t>(pos());
        if (offset >= m_bytes.size()) return 0;
        const std::size_t available = m_bytes.size() - offset;
        const std::size_t requested = static_cast<std::size_t>(maxSize);
        const std::size_t count = std::min(available, requested);
        if (count != 0) std::memcpy(data, m_bytes.data() + offset, count);
        return static_cast<pk_int64>(count);
    }

    pk_int64 writeData(const char *, pk_int64) override
    {
        return -1;
    }

private:
    const std::vector<std::uint8_t> &m_bytes;
};

bool isSupportedBuiltInVariantType(std::uint32_t typeId)
{
    switch (typeId) {
    case PkVariant::Invalid:
    case PkVariant::Bool:
    case PkVariant::Int:
    case PkVariant::UInt:
    case PkVariant::LongLong:
    case PkVariant::ULongLong:
    case PkVariant::Double:
    case PkVariant::Float:
    case PkVariant::String:
    case PkVariant::ByteArray:
    case PkVariant::StringList:
    case PkVariant::List:
    case PkVariant::Map:
    case PkVariant::Hash:
    case PkVariant::Date:
    case PkVariant::Time:
    case PkVariant::DateTime:
    case PkVariant::Rect:
    case PkVariant::RectF:
    case PkVariant::Size:
    case PkVariant::SizeF:
    case PkVariant::Line:
    case PkVariant::LineF:
    case PkVariant::Point:
    case PkVariant::PointF:
        return true;
    default:
        return false;
    }
}

bool isUserTypeId(std::uint32_t typeId)
{
    constexpr std::uint32_t qt5UserType = 1024;
    return typeId == static_cast<std::uint32_t>(PkVariant::UserType) ||
           typeId >= qt5UserType;
}

enum class WireValidationStatus
{
    Valid,
    ReadPastEnd,
    Malformed,
    UnsupportedUserType,
    UnsupportedType,
    TrailingData
};

struct WireValidationResult
{
    WireValidationStatus status = WireValidationStatus::Malformed;
    std::uint32_t topLevelType = 0;
};

class Qt515VariantWireScanner
{
public:
    explicit Qt515VariantWireScanner(const std::vector<std::uint8_t> &bytes)
        : m_bytes(bytes)
    {
    }

    WireValidationResult scan()
    {
        WireValidationResult result;
        result.status = scanVariant(0, &result.topLevelType);
        if (result.status == WireValidationStatus::Valid && m_offset != m_bytes.size()) {
            result.status = WireValidationStatus::TrailingData;
        }
        return result;
    }

private:
    static constexpr std::size_t kMaximumVariantDepth = 64;
    static constexpr std::uint32_t kNullLength = 0xffffffffu;

    bool readU8(std::uint8_t &value)
    {
        if (!canRead(1)) return false;
        value = m_bytes[m_offset++];
        return true;
    }

    bool readU32(std::uint32_t &value)
    {
        if (!canRead(4)) return false;
        value = (static_cast<std::uint32_t>(m_bytes[m_offset]) << 24) |
                (static_cast<std::uint32_t>(m_bytes[m_offset + 1]) << 16) |
                (static_cast<std::uint32_t>(m_bytes[m_offset + 2]) << 8) |
                static_cast<std::uint32_t>(m_bytes[m_offset + 3]);
        m_offset += 4;
        return true;
    }

    bool canRead(std::size_t count) const
    {
        return m_offset <= m_bytes.size() && count <= m_bytes.size() - m_offset;
    }

    bool skip(std::size_t count)
    {
        if (!canRead(count)) return false;
        m_offset += count;
        return true;
    }

    bool countCanFit(std::uint32_t count, std::size_t minimumBytes) const
    {
        const std::size_t remaining =
            m_offset <= m_bytes.size() ? m_bytes.size() - m_offset : 0;
        return minimumBytes == 0 ||
               static_cast<std::uint64_t>(count) <=
                   static_cast<std::uint64_t>(remaining / minimumBytes);
    }

    WireValidationStatus scanString()
    {
        std::uint32_t byteCount = 0;
        if (!readU32(byteCount)) return WireValidationStatus::ReadPastEnd;
        if (byteCount == kNullLength) return WireValidationStatus::Valid;
        if ((byteCount & 1u) != 0u) return WireValidationStatus::Malformed;
        return skip(byteCount) ? WireValidationStatus::Valid
                               : WireValidationStatus::ReadPastEnd;
    }

    WireValidationStatus scanByteArray()
    {
        std::uint32_t byteCount = 0;
        if (!readU32(byteCount)) return WireValidationStatus::ReadPastEnd;
        if (byteCount == kNullLength) return WireValidationStatus::Valid;
        return skip(byteCount) ? WireValidationStatus::Valid
                               : WireValidationStatus::ReadPastEnd;
    }

    WireValidationStatus scanStringList()
    {
        std::uint32_t count = 0;
        if (!readU32(count)) return WireValidationStatus::ReadPastEnd;
        if (count > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
            return WireValidationStatus::Malformed;
        }
        if (!countCanFit(count, 4)) return WireValidationStatus::ReadPastEnd;
        for (std::uint32_t i = 0; i < count; ++i) {
            const WireValidationStatus status = scanString();
            if (status != WireValidationStatus::Valid) return status;
        }
        return WireValidationStatus::Valid;
    }

    WireValidationStatus scanVariantList(std::size_t depth)
    {
        std::uint32_t count = 0;
        if (!readU32(count)) return WireValidationStatus::ReadPastEnd;
        if (count > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
            return WireValidationStatus::Malformed;
        }
        if (!countCanFit(count, 5)) return WireValidationStatus::ReadPastEnd;
        for (std::uint32_t i = 0; i < count; ++i) {
            const WireValidationStatus status = scanVariant(depth, nullptr);
            if (status != WireValidationStatus::Valid) return status;
        }
        return WireValidationStatus::Valid;
    }

    WireValidationStatus scanVariantMap(std::size_t depth)
    {
        std::uint32_t count = 0;
        if (!readU32(count)) return WireValidationStatus::ReadPastEnd;
        if (count > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
            return WireValidationStatus::Malformed;
        }
        if (!countCanFit(count, 9)) return WireValidationStatus::ReadPastEnd;
        for (std::uint32_t i = 0; i < count; ++i) {
            WireValidationStatus status = scanString();
            if (status != WireValidationStatus::Valid) return status;
            status = scanVariant(depth, nullptr);
            if (status != WireValidationStatus::Valid) return status;
        }
        return WireValidationStatus::Valid;
    }

    WireValidationStatus scanDateTime()
    {
        if (!skip(8 + 4)) return WireValidationStatus::ReadPastEnd;
        std::uint8_t spec = 0;
        if (!readU8(spec)) return WireValidationStatus::ReadPastEnd;
        if (spec > static_cast<std::uint8_t>(PkVariant::DateTimeSpec::TimeZone)) {
            return WireValidationStatus::Malformed;
        }
        if (spec == static_cast<std::uint8_t>(PkVariant::DateTimeSpec::OffsetFromUTC)) {
            return skip(4) ? WireValidationStatus::Valid
                           : WireValidationStatus::ReadPastEnd;
        }
        if (spec == static_cast<std::uint8_t>(PkVariant::DateTimeSpec::TimeZone)) {
            return scanString();
        }
        return WireValidationStatus::Valid;
    }

    WireValidationStatus scanRect()
    {
        // The Qt 5.15 wire format stores rectangles as four canonical int32
        // endpoint coordinates.
        // R32's PkDataStream decoder now constructs PkRect from those endpoints
        // directly, so every four-int32 frame is valid, including descending and
        // extreme endpoints that cannot be represented as x/y/width/height
        // arithmetic without overflow.
        return skip(16) ? WireValidationStatus::Valid
                        : WireValidationStatus::ReadPastEnd;
    }

    WireValidationStatus scanVariant(std::size_t depth, std::uint32_t *typeOut)
    {
        if (depth >= kMaximumVariantDepth) return WireValidationStatus::Malformed;

        std::uint32_t typeId = 0;
        std::uint8_t nullFlag = 0;
        if (!readU32(typeId) || !readU8(nullFlag)) {
            return WireValidationStatus::ReadPastEnd;
        }
        if (typeOut) *typeOut = typeId;
        if (nullFlag > 1u) return WireValidationStatus::Malformed;
        if (isUserTypeId(typeId)) return WireValidationStatus::UnsupportedUserType;
        if (!isSupportedBuiltInVariantType(typeId)) {
            return WireValidationStatus::UnsupportedType;
        }

        switch (typeId) {
        case PkVariant::Invalid:
            return WireValidationStatus::Valid;
        case PkVariant::Bool: {
            std::uint8_t value = 0;
            if (!readU8(value)) return WireValidationStatus::ReadPastEnd;
            return value <= 1u ? WireValidationStatus::Valid
                               : WireValidationStatus::Malformed;
        }
        case PkVariant::Int:
        case PkVariant::UInt:
        case PkVariant::Time:
            return skip(4) ? WireValidationStatus::Valid
                           : WireValidationStatus::ReadPastEnd;
        case PkVariant::LongLong:
        case PkVariant::ULongLong:
        case PkVariant::Double:
        case PkVariant::Float:
        case PkVariant::Date:
            return skip(8) ? WireValidationStatus::Valid
                           : WireValidationStatus::ReadPastEnd;
        case PkVariant::String:
            return scanString();
        case PkVariant::ByteArray:
            return scanByteArray();
        case PkVariant::StringList:
            return scanStringList();
        case PkVariant::List:
            return scanVariantList(depth + 1);
        case PkVariant::Map:
        case PkVariant::Hash:
            return scanVariantMap(depth + 1);
        case PkVariant::DateTime:
            return scanDateTime();
        case PkVariant::Rect:
            return scanRect();
        case PkVariant::RectF:
        case PkVariant::LineF:
            return skip(32) ? WireValidationStatus::Valid
                            : WireValidationStatus::ReadPastEnd;
        case PkVariant::Size:
        case PkVariant::Point:
            return skip(8) ? WireValidationStatus::Valid
                           : WireValidationStatus::ReadPastEnd;
        case PkVariant::SizeF:
        case PkVariant::PointF:
            return skip(16) ? WireValidationStatus::Valid
                            : WireValidationStatus::ReadPastEnd;
        case PkVariant::Line:
            return skip(16) ? WireValidationStatus::Valid
                            : WireValidationStatus::ReadPastEnd;
        default:
            return WireValidationStatus::UnsupportedType;
        }
    }

    const std::vector<std::uint8_t> &m_bytes;
    std::size_t m_offset = 0;
};

KisResourceCacheDb::MetaDataDecodeStatus decodeStatusFromStream(
    PkDataStream::Status status, std::uint32_t typeId)
{
    if (status == PkDataStream::ReadPastEnd) {
        return KisResourceCacheDb::MetaDataDecodeStatus::ReadPastEnd;
    }
    if (status == PkDataStream::ReadCorruptData) {
        if (isUserTypeId(typeId)) {
            return KisResourceCacheDb::MetaDataDecodeStatus::UnsupportedUserType;
        }
        if (!isSupportedBuiltInVariantType(typeId)) {
            return KisResourceCacheDb::MetaDataDecodeStatus::UnsupportedType;
        }
    }
    return KisResourceCacheDb::MetaDataDecodeStatus::ReadCorruptData;
}

struct VariantDecodeResult
{
    bool decoded = false;
    PkVariant value;
    KisResourceCacheDb::MetaDataDecodeStatus status =
        KisResourceCacheDb::MetaDataDecodeStatus::ReadCorruptData;
};

class MetadataSavepoint
{
public:
    MetadataSavepoint()
        : m_name(PkString("krita_metadata_") +
                 PkString(std::to_string(nextId().fetch_add(1)).c_str()))
        , m_database(PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false))
        , m_connectionGuard(m_database)
    {
    }

    ~MetadataSavepoint() noexcept
    {
        try {
            if (m_active && !rollback()) {
                qWarning() << "Failed to rollback metadata savepoint during cleanup" << m_name;
            }
        } catch (...) {
            // The owning connection guard still releases native then outer
            // mutex during member destruction.
        }
    }

    MetadataSavepoint(const MetadataSavepoint &) = delete;
    MetadataSavepoint &operator=(const MetadataSavepoint &) = delete;

    bool begin()
    {
        if (detail::resourceDatabaseConnectionIsPoisoned() ||
            !m_database.isOpen() || !m_database.PkHandle()) {
            qWarning() << detail::resourceDatabaseConnectionPoisonError();
            return false;
        }
        m_outerTransactionActive =
            sqlite3_get_autocommit(m_database.PkHandle()) == 0;
        if (!execControl(PkString("SAVEPOINT ") + m_name, "begin")) {
            return false;
        }
        m_active = true;
        if (!detail::ensureResourceDatabaseAutocommitState(
                m_database, false, m_connectionGuard, "savepoint begin")) {
            m_active = false;
            return false;
        }
        return true;
    }

    bool release()
    {
        if (!m_active) {
            return false;
        }
        if (execControl(PkString("RELEASE SAVEPOINT ") + m_name, "release")) {
            m_active = false;
            return detail::ensureResourceDatabaseAutocommitState(
                m_database,
                !m_outerTransactionActive,
                m_connectionGuard,
                "savepoint release");
        }

        // RELEASE is the commit point for a top-level savepoint.  If it
        // fails, make a best-effort checked rollback instead of allowing the
        // destructor or an outer transaction to commit partial rows.
        (void)rollback();
        return false;
    }

    bool rollback()
    {
        if (!m_active) {
            return true;
        }
        const bool rolledBack =
            execControl(PkString("ROLLBACK TO SAVEPOINT ") + m_name, "rollback-to");
        if (!rolledBack) {
            // Releasing a savepoint whose rollback failed could commit the
            // very partial update this guard exists to contain.
            detail::poisonResourceDatabaseConnection(
                m_database, m_connectionGuard, "savepoint rollback-to");
            m_active = false;
            return false;
        }
        const bool released =
            execControl(PkString("RELEASE SAVEPOINT ") + m_name, "release-after-rollback");
        if (released) {
            m_active = false;
            return detail::ensureResourceDatabaseAutocommitState(
                m_database,
                !m_outerTransactionActive,
                m_connectionGuard,
                "savepoint rollback cleanup");
        }
        detail::poisonResourceDatabaseConnection(
            m_database, m_connectionGuard, "savepoint cleanup release");
        m_active = false;
        return false;
    }

private:
    static std::atomic<std::uint64_t> &nextId()
    {
        static std::atomic<std::uint64_t> value {1};
        return value;
    }

    bool execControl(const PkString &statement, const char *operation)
    {
        PkSqlQuery query;
        if (query.exec(statement)) {
            return true;
        }
        qWarning() << "Could not" << operation << "metadata savepoint" << m_name
                   << query.lastError();
        return false;
    }

    PkString m_name;
    PkSqlDatabase m_database;
    detail::ResourceDatabaseConnectionGuard m_connectionGuard;
    bool m_active = false;
    bool m_outerTransactionActive = false;
};

VariantDecodeResult deserializeVariant(const PkString &encoded)
{
    VariantDecodeResult result;
    PkDataStream defaults;
    const std::size_t allocationLimit = defaults.allocationLimit();
    std::vector<std::uint8_t> bytes;
    const Base64DecodeStatus base64Status = base64Decode(encoded, allocationLimit, bytes);
    if (base64Status == Base64DecodeStatus::Invalid) {
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::InvalidBase64;
        return result;
    }
    if (base64Status == Base64DecodeStatus::LimitExceeded) {
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::ReadCorruptData;
        return result;
    }

    const WireValidationResult framing = Qt515VariantWireScanner(bytes).scan();
    switch (framing.status) {
    case WireValidationStatus::Valid:
        break;
    case WireValidationStatus::ReadPastEnd:
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::ReadPastEnd;
        return result;
    case WireValidationStatus::Malformed:
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::ReadCorruptData;
        return result;
    case WireValidationStatus::UnsupportedUserType:
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::UnsupportedUserType;
        return result;
    case WireValidationStatus::UnsupportedType:
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::UnsupportedType;
        return result;
    case WireValidationStatus::TrailingData:
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::TrailingData;
        return result;
    }

    MetadataReadStream device(bytes);
    PkDataStream stream(&device);
    stream.setVersion(PkDataStream::Qt_5_15);
    stream.setAllocationLimit(allocationLimit);
    stream >> result.value;
    if (stream.status() != PkDataStream::Ok) {
        result.status = decodeStatusFromStream(stream.status(), framing.topLevelType);
        result.value.clear();
        return result;
    }
    if (!device.atEnd()) {
        result.status = KisResourceCacheDb::MetaDataDecodeStatus::TrailingData;
        result.value.clear();
        return result;
    }
    result.decoded = true;
    return result;
}

PkString serializeVariant(const PkVariant &value)
{
    try {
        PkByteArray bytes;
        PkDataStream stream(&bytes, PkStream::WriteOnly);
        stream.setVersion(PkDataStream::Qt_5_15);
        stream << value;
        if (stream.status() != PkDataStream::Ok) return PkString();
        return base64Encode(bytes);
    } catch (const std::bad_alloc &) {
        return PkString();
    } catch (const std::length_error &) {
        return PkString();
    }
}

} // namespace

const PkString dbDriver = "SQLITE";
const PkString METADATA_RESOURCES = "resources";
const PkString METADATA_STORAGES = "storages";

const PkString KisResourceCacheDb::resourceCacheDbFilename { "resourcecache.sqlite" };
const PkString KisResourceCacheDb::databaseVersion { "0.0.18" };
PkStringList KisResourceCacheDb::storageTypes { PkStringList() };
PkStringList KisResourceCacheDb::disabledBundles { PkStringList() << "Krita_3_Default_Resources.bundle" };

bool KisResourceCacheDb::s_valid {false};
PkString KisResourceCacheDb::s_lastError {PkString()};

bool KisResourceCacheDb::isValid()
{
    return s_valid && !detail::resourceDatabaseConnectionIsPoisoned();
}

PkString KisResourceCacheDb::lastError()
{
    if (detail::resourceDatabaseConnectionIsPoisoned()) {
        return detail::resourceDatabaseConnectionPoisonError();
    }
    return s_lastError;
}

// use in WHERE PkSqlQuery clauses
// because if the string is null, the query will also have null there
// and every comparison with null is false, so the query won't find anything
// (especially important for storage location where empty string is common)
PkString changeToEmptyIfNull(PkString s)
{
    return s;
}

PkSqlError transactionFailure(const char *operation)
{
    return PkSqlError(PkString("Resource database transaction failed during ") +
                          PkString(operation),
                      PkString("Error executing SQL transaction"),
                      PkSqlError::TransactionError);
}

bool updateSchemaVersion()
{
    PkSqlQuery q;
    if (!q.prepare(embeddedSql("fill_version_information.sql"))) {
        warnDbMigration << "Could not prepare the schema information query" << q.lastError() << q.boundValues();
        return false;
    }
    q.addBindValue(KisResourceCacheDb::databaseVersion);
    q.addBindValue(KritaVersionWrapper::versionString());
    q.addBindValue(static_cast<long long>(PkDateTime::currentDateTimeUtc().toSecsSinceEpoch()));
    if (!q.exec()) {
        warnDbMigration << "Could not insert the current version" << q.lastError() << q.boundValues();
        return false;
    }
    infoDbMigration << "Filled version table";
    return true;
}

PkSqlError runUpdateScriptFile(const PkString &path, const PkString &message)
{
    try {

        KisSqlQueryLoader loader(path);
        loader.exec();

    } catch (const KisSqlQueryLoader::FileException &e) {
        warnDbMigration.noquote() << "ERROR: Could not execute DB update step:" << message;
        warnDbMigration.noquote() << "       error" << e.message;
        warnDbMigration.noquote() << "       file:" << e.filePath;
        warnDbMigration.noquote() << "       file-error:" << e.fileErrorString;
        return PkSqlError(PkString("Could not find SQL file %1").arg(e.filePath),
                          PkString("Error executing SQL"),
                          PkSqlError::StatementError);
    } catch (const KisSqlQueryLoader::SQLException &e) {
        warnDbMigration.noquote() << "ERROR: Could not execute DB update step:" << message;
        warnDbMigration.noquote() << "       error" << e.message;
        warnDbMigration.noquote() << "       file:" << e.filePath;
        warnDbMigration.noquote() << "       statement:" << e.statementIndex;
        warnDbMigration.noquote() << "       sql-error:" << e.sqlError.text();
        return e.sqlError;
    }

    infoDbMigration << "Completed DB update step:" << message;
    return PkSqlError();
}

PkSqlError runUpdateScript(const PkString &script, const PkString &message)
{
    try {

        KisSqlQueryLoader loader("", script);
        loader.exec();

    } catch (const KisSqlQueryLoader::SQLException &e) {
        warnDbMigration.noquote() << "ERROR: Could execute DB update step:" << message;
        warnDbMigration.noquote() << "       error" << e.message;
        warnDbMigration.noquote() << "       sql-error:" << e.sqlError.text();
        return e.sqlError;
    }

    infoDbMigration << "Completed DB update step:" << message;
    return PkSqlError();
}

PkSqlError createDatabase(const PkString &location)
{
    // NOTE: if the id's of Unknown and Memory in the database
    //       will change, and that will break the queries that
    //       remove Unknown and Memory storages on start-up.
    KisResourceCacheDb::storageTypes = PkStringList {
        KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType(1)),
        KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType(2)),
        KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType(3)),
        KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType(4)),
        KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType(5)),
        KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType(6)),
        KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType(7))};

    std::error_code filesystemError;
    std::filesystem::create_directories(std::filesystem::u8path(location.PkToUtf8()), filesystemError);
    if (filesystemError) {
        return PkSqlError(PkString(filesystemError.message().c_str()),
                          PkString("Error opening resource database directory"),
                          PkSqlError::ConnectionError);
    }

    std::optional<PkSqlDatabase> existingDatabase =
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false);

    const bool databaseConnectionExists = !PkSqlDatabase::connectionNames().isEmpty()
        && existingDatabase->isValid() && existingDatabase->isOpen();

    if (databaseConnectionExists && existingDatabase->tables().contains("version_information")) {
        return PkSqlError();
    }

    existingDatabase = std::nullopt;

    PkSqlDatabase db;

    if (!databaseConnectionExists) {
        db = PkSqlDatabase::addDatabase(dbDriver);
        db.setDatabaseName(location + "/" + KisResourceCacheDb::resourceCacheDbFilename);

        if (!db.open()) {
            warnDbMigration << "Could not connect to resource cache database";
            return db.lastError();
        }
    } else {
        db = PkSqlDatabase::database();
    }

    // will be filled correctly later
    SchemaVersion oldSchemaVersionNumber;
    SchemaVersion newSchemaVersionNumber = SchemaVersion::fromString(KisResourceCacheDb::databaseVersion);


    PkStringList tables = PkStringList() << "version_information"
                                       << "storage_types"
                                       << "resource_types"
                                       << "storages"
                                       << "tags"
                                       << "resources"
                                       << "versioned_resources"
                                       << "resource_tags"
                                       << "metadata"
                                       << "tags_storages"
                                       << "tag_translations";

    PkStringList dbTables;
    // Verify whether we should recreate the database
    {
        bool allTablesPresent = true;
        dbTables = db.tables();
        for (const PkString &table : tables) {
            if (!dbTables.contains(table)) {
                allTablesPresent = false;
                break;
            }
        }

        bool schemaIsOutDated = false;
        PkString schemaVersion = "0.0.0";
        PkString kritaVersion = "Unknown";
        int creationDate = 0;

        if (dbTables.contains("version_information")) {
            // Verify the version number

            {
                PkSqlQuery q(
                    "SELECT database_version\n"
                    ",      krita_version\n"
                    ",      creation_date\n"
                    "FROM version_information\n"
                    "ORDER BY id\n"
                    "DESC\n"
                    "LIMIT 1;\n");

                if (!q.exec()) {
                    warnDbMigration << "Could not retrieve version information from the database." << q.lastError();
                    abort();
                }
                q.first();
                schemaVersion = q.value(0).toString();
                kritaVersion = q.value(1).toString();
                creationDate = q.value(2).toInt();
            }

            oldSchemaVersionNumber = SchemaVersion::fromString(schemaVersion);
            newSchemaVersionNumber = SchemaVersion::fromString(KisResourceCacheDb::databaseVersion);

            if (oldSchemaVersionNumber != newSchemaVersionNumber) {

                infoDbMigration << "Old schema:" << schemaVersion << "New schema:" << newSchemaVersionNumber;

                schemaIsOutDated = true;
                KisBackup::numberedBackupFile(location + "/" + KisResourceCacheDb::resourceCacheDbFilename);

                if (newSchemaVersionNumber == SchemaVersion::fromString("0.0.18")
                        && oldSchemaVersionNumber >= SchemaVersion::fromString("0.0.14")
                        && oldSchemaVersionNumber < SchemaVersion::fromString("0.0.18")) {

                    bool from14to15 = oldSchemaVersionNumber == SchemaVersion::fromString("0.0.14");

                    bool from15to16 = oldSchemaVersionNumber == SchemaVersion::fromString("0.0.14")
                            || oldSchemaVersionNumber == SchemaVersion::fromString("0.0.15");

                    bool from16to17 = oldSchemaVersionNumber == SchemaVersion::fromString("0.0.14")
                            || oldSchemaVersionNumber == SchemaVersion::fromString("0.0.15")
                            || oldSchemaVersionNumber == SchemaVersion::fromString("0.0.16");

                    bool from17to18 = oldSchemaVersionNumber == SchemaVersion::fromString("0.0.14")
                            || oldSchemaVersionNumber == SchemaVersion::fromString("0.0.15")
                            || oldSchemaVersionNumber == SchemaVersion::fromString("0.0.16")
                            || oldSchemaVersionNumber == SchemaVersion::fromString("0.0.17");

                    KisDatabaseTransactionLock transactionLock(
                        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
                    if (!transactionLock.transactionStarted()) {
                        return transactionFailure("schema upgrade begin");
                    }

                    bool success = true;
                    if (from14to15) {
                        PkSqlError error = runUpdateScript(
                            "ALTER TABLE  resource_tags\n"
                            "ADD   COLUMN active INTEGER NOT NULL DEFAULT 1", 
                            "Update resource tags table (add \'active\' column)");
                        if (error.type() != PkSqlError::NoError) {
                            success = false;
                        }
                    }
                    if (success && from15to16) {
                        infoDbMigration << "Going to update indices";

                        PkStringList indexes = PkStringList() << "tags" << "resources" << "tag_translations" << "resource_tags";

                        for (const PkString &index : indexes) {
                            PkSqlError error = runUpdateScriptFile(":/create_index_" + index + ".sql",
                                                                  PkString("Create index for %1").arg(index));
                            if (error.type() != PkSqlError::NoError) {
                                success = false;
                            }
                        }
                    }

                    if (success && from16to17) {
                        PkSqlError error = runUpdateScriptFile(":/create_index_resources_signature.sql",
                                                              "Create index for resources_signature");
                        if (error.type() != PkSqlError::NoError) {
                            success = false;
                        }
                    }

                    if (success && from17to18) {
                        {
                            PkSqlError error = runUpdateScriptFile(":/0_0_18_0001_cleanup_metadata_table.sql",
                                                                  "Cleanup and deduplicate metadata table");
                            if (error.type() != PkSqlError::NoError) {
                                success = false;
                            }
                        }
                        if (success) {
                            PkSqlError error = runUpdateScriptFile(":/0_0_18_0002_update_metadata_table_constraints.sql",
                                                                  "Update metadata table constraints");
                            if (error.type() != PkSqlError::NoError) {
                                success = false;
                            }
                        }
                        if (success) {
                            PkSqlError error = runUpdateScriptFile(":/create_index_metadata_key.sql",
                                                                  "Create index for metadata_key");
                            if (error.type() != PkSqlError::NoError) {
                                success = false;
                            }
                        }
                    }

                    if (success && !updateSchemaVersion()) {
                        success = false;
                    }

                    if (success) {
                        if (!transactionLock.commit()) {
                            return transactionFailure("schema upgrade commit");
                        }

                        PkSqlError error = runUpdateScript("VACUUM",
                                                          "Vacuum database after updating schema");
                        if (error.type() != PkSqlError::NoError) {
                            success = false;
                        }
                    } else {
                        transactionLock.rollback();
                    }

                    schemaIsOutDated = !success;

                }

                if (schemaIsOutDated) {
                    qWarning() << "The resource database schema changed; backing up and recreating it";
                    if (oldSchemaVersionNumber > SchemaVersion::fromString("0.0.14")) {
                        KisResourceLocator::instance()->saveTags();
                    }
                    db.close();
                    std::filesystem::remove(
                        std::filesystem::u8path((location + "/" + KisResourceCacheDb::resourceCacheDbFilename).PkToUtf8()),
                        filesystemError);
                    db.open();
                }
            }

        }

        if (allTablesPresent && !schemaIsOutDated) {
            KisUsageLogger::log(PkString("Database is up to date. Version: %1, created by Krita %2, at %3")
                                .arg(schemaVersion)
                                .arg(kritaVersion)
                                .arg(PkString(PkDateTime::fromSecsSinceEpoch(creationDate).toString().c_str())));

            /// initialization is completed, transaction is over,
            /// now enable the foreign_keys constraint if necessary
            KisResourceCacheDb::synchronizeForeignKeysState();

            return PkSqlError();
        }
    }

    KisUsageLogger::log(PkString("Creating database from scratch (%1, %2).")
                        .arg(oldSchemaVersionNumber.toString().isEmpty() ? PkString("database didn't exist") : ("old schema version: " + oldSchemaVersionNumber.toString()))
                        .arg("new schema version: " + newSchemaVersionNumber.toString()));

    KisDatabaseTransactionLock transactionLock(
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
    if (!transactionLock.transactionStarted()) {
        return transactionFailure("schema creation begin");
    }

    // Create tables
    for (const PkString &table : tables) {
        PkSqlError error =
            runUpdateScriptFile(":/create_" + table + ".sql", PkString("Create table %1").arg(table));
        if (error.type() != PkSqlError::NoError) {
            return error;
        }
    }

    {
        // metadata table constraints were updated in version 0.0.18
        PkSqlError error = runUpdateScriptFile(":/0_0_18_0002_update_metadata_table_constraints.sql",
                                              "Update metadata table constraints");

        if (error.type() != PkSqlError::NoError) {
            return error;
        }
    }

    // Create indexes
    PkStringList indexes;

    // these indexes came in version 0.0.16
    indexes << "storages" << "versioned_resources" << "tags" << "resources" << "tag_translations" << "resource_tags";

    // this index came in version 0.0.17
    indexes << "resources_signature";

    // this index came in version 0.0.18
    indexes << "metadata_key";

    for (const PkString &index : indexes) {
        PkSqlError error = runUpdateScriptFile(":/create_index_" + index + ".sql",
                                              PkString("Create index for %1").arg(index));
        if (error.type() != PkSqlError::NoError) {
            return error;
        }
    }

    // Fill lookup tables
    {
        const PkString sql = embeddedSql("fill_storage_types.sql");
        for (const PkString &originType : KisResourceCacheDb::storageTypes) {
            const PkString updateStep = PkString("Register storage type: %1").arg(originType);
            PkSqlQuery q(sql);
            q.addBindValue(originType);
            if (!q.exec()) {
                warnDbMigration << "Could execute DB update step:" << updateStep << q.lastError();
                warnDbMigration << "    faulty statement:" << sql;
                return q.lastError();
            }
            infoDbMigration << "Completed DB update step:" << updateStep;
        }
    }

    {
        const PkString sql = embeddedSql("fill_resource_types.sql");
        for (const PkString &resourceType : KisResourceLoaderRegistry::instance()->resourceTypes()) {
            const PkString updateStep = PkString("Register resource type: %1").arg(resourceType);
            PkSqlQuery q(sql);
            q.addBindValue(resourceType);
            if (!q.exec()) {
                warnDbMigration << "Could execute DB update step:" << updateStep << q.lastError();
                warnDbMigration << "    faulty statement:" << sql;
                return q.lastError();
            }
            infoDbMigration << "Completed DB update step:" << updateStep;
        }
    }

    if (!updateSchemaVersion()) {
       return PkSqlError(PkString("Could not update schema version."),
                         PkString("Error executing SQL"),
                         PkSqlError::StatementError);
    }

    if (!transactionLock.commit()) {
        return transactionFailure("schema creation commit");
    }

    /// initialization is completed, transaction is over,
    /// now enable the foreign_keys constraint if necessary
    KisResourceCacheDb::synchronizeForeignKeysState();

    return PkSqlError();
}

bool KisResourceCacheDb::initialize(const PkString &location)
{
    if (detail::resourceDatabaseConnectionIsPoisoned()) {
        s_valid = false;
        s_lastError = detail::resourceDatabaseConnectionPoisonError();
        return false;
    }
    PkSqlError err;
    try {
        err = createDatabase(location);
    } catch (const KisSqlQueryLoader::FileException &e) {
        warnDbMigration.noquote() << "ERROR: Missing embedded SQL during database initialization";
        warnDbMigration.noquote() << "       file:" << e.filePath;
        warnDbMigration.noquote() << "       file-error:" << e.fileErrorString;
        err = PkSqlError(PkString("Could not find SQL file %1").arg(e.filePath),
                         PkString("Error executing SQL"),
                         PkSqlError::StatementError);
    }

    // Cleanup is part of successful initialization: a failed BEGIN/COMMIT
    // must not be reported as a valid cache database, and no cleanup is run
    // after schema creation itself failed.
    if (err.type() == PkSqlError::NoError && !deleteTemporaryResources()) {
        err = PkSqlError(PkString("Could not clean temporary resource rows."),
                         PkString("Error committing database cleanup"),
                         PkSqlError::TransactionError);
    }

    s_valid = !err.isValid();
    switch (err.type()) {
    case PkSqlError::NoError:
        s_lastError = PkString();
        break;
    case PkSqlError::ConnectionError:
        s_lastError = PkString("Could not initialize the resource cache database. Connection error: %1").arg(err.text());
        break;
    case PkSqlError::StatementError:
        s_lastError = PkString("Could not initialize the resource cache database. Statement error: %1").arg(err.text());
        break;
    case PkSqlError::TransactionError:
        s_lastError = PkString("Could not initialize the resource cache database. Transaction error: %1").arg(err.text());
        break;
    case PkSqlError::UnknownError:
        s_lastError = PkString("Could not initialize the resource cache database. Unknown error: %1").arg(err.text());
        break;
    }

    return s_valid;
}

std::pair<PkVector<int>,PkVector<int>> KisResourceCacheDb::tagsForStorage(const PkString &resourceType, const PkString &storageLocation)
{
    try {
        KisSqlQueryLoader loader(":/sql/storage_tags_ref_count.sql", KisSqlQueryLoader::single_statement_mode);
        loader.query().bindValue(":resource_type", resourceType);
        loader.query().bindValue(":location", changeToEmptyIfNull(storageLocation));
        loader.exec();

        PkVector<int> uniqueTags;
        PkVector<int> sharedTags;

        while (loader.query().next()) {
            if (loader.query().value("ref_count").toInt() > 1) {
                sharedTags << loader.query().value("tag_id").toInt();
            } else {
                uniqueTags << loader.query().value("tag_id").toInt();
            }
        }

        return {uniqueTags, sharedTags};

    } catch (const KisSqlQueryLoader::FileException &e) {
        qWarning().noquote() << "ERROR: deleteStorage:" << e.message;
        qWarning().noquote() << "       file:" << e.filePath;
        qWarning().noquote() << "       error:" << e.fileErrorString;
        return {};
    } catch (const KisSqlQueryLoader::SQLException &e) {
        qWarning().noquote() << "ERROR: deleteStorage:" << e.message;
        qWarning().noquote() << "       file:" << e.filePath;
        qWarning().noquote() << "       statement:" << e.statementIndex;
        qWarning().noquote() << "       error:" << e.sqlError.text();
        return {};
    }

    return {};
}

PkVector<int> KisResourceCacheDb::resourcesForStorage(const PkString &resourceType, const PkString &storageLocation)
{
    PkVector<int> result;

    PkSqlQuery q;

    if (!q.prepare("SELECT resources.id\n"
                   "FROM   resources\n"
                   ",      resource_types\n"
                   ",      storages\n"
                   "WHERE  resources.resource_type_id = resource_types.id\n"
                   "AND    storages.id = resources.storage_id\n"
                   "AND    storages.location = :storage_location\n"
                   "AND    resource_types.name = :resource_type\n")) {

        qWarning() << "Could not read and prepare resourcesForStorage" << q.lastError();
        return result;
    }

    q.bindValue(":resource_type", resourceType);
    q.bindValue(":storage_location", changeToEmptyIfNull(storageLocation));

    if (!q.exec()) {
        qWarning() << "Could not query resourceIdForResource" << q.boundValues() << q.lastError();
        return result;
    }

    while (q.next()) {
        result << q.value(0).toInt();
    }

    return result;
}

int KisResourceCacheDb::resourceIdForResource(const PkString &resourceFileName, const PkString &resourceType, const PkString &storageLocation)
{
    //qDebug() << "resourceIdForResource" << resourceName << resourceFileName << resourceType << storageLocation;

    PkSqlQuery q;

    if (!q.prepare("SELECT resources.id\n"
                   "FROM   resources\n"
                   ",      resource_types\n"
                   ",      storages\n"
                   "WHERE  resources.resource_type_id = resource_types.id\n"
                   "AND    storages.id = resources.storage_id\n"
                   "AND    storages.location = :storage_location\n"
                   "AND    resource_types.name = :resource_type\n"
                   "AND    resources.filename = :filename\n")) {
        qWarning() << "Could not read and prepare resourceIdForResource" << q.lastError();
        return -1;
    }

    q.bindValue(":filename", resourceFileName);
    q.bindValue(":resource_type", resourceType);
    q.bindValue(":storage_location", changeToEmptyIfNull(storageLocation));

    if (!q.exec()) {
        qWarning() << "Could not query resourceIdForResource" << q.boundValues() << q.lastError();
        return -1;
    }

    if (q.first()) {
        return q.value(0).toInt();
    }

    // couldn't be found in the `resources` table, but can still be in versioned_resources

    if (!q.prepare("SELECT versioned_resources.resource_id\n"
                   "FROM   resources\n"
                   ",      resource_types\n"
                   ",      versioned_resources\n"
                   ",      storages\n"
                   "WHERE  resources.resource_type_id = resource_types.id\n"    // join resources and resource_types by resource id
                   "AND    versioned_resources.resource_id = resources.id\n"    // join versioned_resources and resources by resource id
                   "AND    storages.id = versioned_resources.storage_id\n"      // join storages and versioned_resources by storage id
                   "AND    storages.location = :storage_location\n"             // storage location must be the same as asked for
                   "AND    resource_types.name = :resource_type\n"              // resource type must be the same as asked for
                   "AND    versioned_resources.filename = :filename\n")) {      // filename must be the same as asked for
        qWarning() << "Could not read and prepare resourceIdForResource (in versioned resources)" << q.lastError();
        return -1;
    }

    q.bindValue(":filename", resourceFileName);
    q.bindValue(":resource_type", resourceType);
    q.bindValue(":storage_location", changeToEmptyIfNull(storageLocation));

    if (!q.exec()) {
        qWarning() << "Could not query resourceIdForResource (in versioned resources)" << q.boundValues() << q.lastError();
        return -1;
    }

    if (q.first()) {
        return q.value(0).toInt();
    }

    // commenting out, because otherwise it spams the console on every new resource in the local resources folder
    // qWarning() << "Could not find resource" << resourceName << resourceFileName << resourceType << storageLocation;
    return -1;

}

bool KisResourceCacheDb::resourceNeedsUpdating(int resourceId, PkDateTime timestamp)
{
    PkSqlQuery q;
    if (!q.prepare("SELECT timestamp\n"
                   "FROM   versioned_resources\n"
                   "WHERE  resource_id = :resource_id\n"
                   "AND    version = (SELECT MAX(version)\n"
                   "                  FROM   versioned_resources\n"
                   "                  WHERE  resource_id = :resource_id);")) {
        qWarning() << "Could not prepare resourceNeedsUpdating statement" << q.lastError();
        return false;
    }

    q.bindValue(":resource_id", resourceId);

    if (!q.exec()) {
        qWarning() << "Could not query for the most recent timestamp" << q.boundValues() << q.lastError();
        return false;
    }

    if (!q.first()) {
        qWarning() << "Inconsistent database: could not find a version for resource with Id" << resourceId;
        return false;
    }

    PkVariant resourceTimeStamp = q.value(0);

    if (!resourceTimeStamp.isValid()) {
        qWarning() << "Could not retrieve timestamp from versioned_resources" << resourceId;
        return false;
    }

    return (timestamp.toSecsSinceEpoch() > resourceTimeStamp.toInt());
}

bool KisResourceCacheDb::addResourceVersion(int resourceId, PkDateTime timestamp, KisResourceStorageSP storage, KoResourceSP resource)
{
    bool r = false;


    r = addResourceVersionImpl(resourceId, timestamp, storage, resource);

    if (!r) return r;

    r = makeResourceTheCurrentVersion(resourceId, resource);

    return r;
}

bool KisResourceCacheDb::addResourceVersionImpl(int resourceId, PkDateTime timestamp, KisResourceStorageSP storage, KoResourceSP resource)
{
    bool r = false;

    // Create the new version. The resource is expected to have an updated version number, or
    // this will fail on the unique index on resource_id, storage_id and version.
    //
    // This function **only** adds to the versioned_resources table.
    // The resources table should be updated by the caller manually using
    // updateResourceTableForResourceIfNeeded()

    Q_ASSERT(resource->version() >= 0);

    PkSqlQuery q;
    r = q.prepare("INSERT INTO versioned_resources \n"
                  "(resource_id, storage_id, version, filename, timestamp, md5sum)\n"
                  "VALUES\n"
                  "( :resource_id\n"
                  ", (SELECT id \n"
                  "   FROM   storages \n"
                  "   WHERE  location = :storage_location)\n"
                  ", :version\n"
                  ", :filename\n"
                  ", :timestamp\n"
                  ", :md5sum\n"
                  ");");

    if (!r) {
        qWarning() << "Could not prepare addResourceVersion statement" << q.lastError();
        return r;
    }

    q.bindValue(":resource_id", resourceId);
    q.bindValue(":storage_location", changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storage->location())));
    q.bindValue(":version", resource->version());
    q.bindValue(":filename", resource->filename());
    q.bindValue(":timestamp", static_cast<long long>(timestamp.toSecsSinceEpoch()));
    KIS_SAFE_ASSERT_RECOVER_NOOP(!resource->md5Sum().isEmpty());
    q.bindValue(":md5sum", resource->md5Sum());
    r = q.exec();
    if (!r) {

        qWarning() << "Could not execute addResourceVersionImpl statement" << q.lastError() << resourceId << storage->name() << storage->location() << resource->name() << resource->filename() << "version" << resource->version();
        return r;
    }

    return r;
}

bool KisResourceCacheDb::removeResourceVersionImpl(int resourceId, int version, KisResourceStorageSP storage)
{
    bool r = false;

    // Remove a version of the resource. This function **only** removes data from
    // the versioned_resources table. The resources table should be updated by
    // the caller manually using updateResourceTableForResourceIfNeeded()

    PkSqlQuery q;
    r = q.prepare("DELETE FROM versioned_resources \n"
                  "WHERE resource_id = :resource_id\n"
                  "AND version = :version\n"
                  "AND storage_id = (SELECT id \n"
                  "                  FROM   storages \n"
                  "                  WHERE  location = :storage_location);");

    if (!r) {
        qWarning() << "Could not prepare removeResourceVersionImpl statement" << q.lastError();
        return r;
    }

    q.bindValue(":resource_id", resourceId);
    q.bindValue(":storage_location", changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storage->location())));
    q.bindValue(":version", version);
    r = q.exec();
    if (!r) {

        qWarning() << "Could not execute removeResourceVersionImpl statement" << q.lastError() << resourceId << storage->name() << storage->location() << "version" << version;
        return r;
    }

    return r;
}

bool KisResourceCacheDb::updateResourceTableForResourceIfNeeded(int resourceId, const PkString &resourceType, KisResourceStorageSP storage)
{
    bool r = false;

    int maxVersion = -1;
    {
        PkSqlQuery q;
        r = q.prepare("SELECT MAX(version)\n"
                      "FROM   versioned_resources\n"
                      "WHERE  resource_id = :resource_id;");
        if (!r) {
            qWarning() << "Could not prepare findMaxVersion statement" << q.lastError();
            return r;
        }

        q.bindValue(":resource_id", resourceId);

        r = q.exec();
        if (!r) {
            qWarning() << "Could not execute findMaxVersion query" << q.boundValues() << q.lastError();
            return r;
        }

        r = q.first();
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(r, false);

        maxVersion = q.value(0).toInt();
    }

    PkString maxVersionFilename;
    {
        PkSqlQuery q;
        r = q.prepare("SELECT filename\n"
                      "FROM   versioned_resources\n"
                      "WHERE  resource_id = :resource_id\n"
                      "AND    version = :version;");
        if (!r) {
            qWarning() << "Could not prepare findMaxVersionFilename statement" << q.lastError();
            return r;
        }

        q.bindValue(":resource_id", resourceId);
        q.bindValue(":version", maxVersion);

        r = q.exec();
        if (!r) {
            qWarning() << "Could not execute findMaxVersionFilename query" << q.boundValues() << q.lastError();
            return r;
        }

        if (!q.first()) {
            return removeResourceCompletely(resourceId);
        } else {
            maxVersionFilename = q.value(0).toString();
        }
    }

    PkString currentFilename;
    {
        PkSqlQuery q;
        r = q.prepare("SELECT filename\n"
                      "FROM   resources\n"
                      "WHERE  id = :resource_id;");
        if (!r) {
            qWarning() << "Could not prepare findMaxVersion statement" << q.lastError();
            return r;
        }

        q.bindValue(":resource_id", resourceId);

        r = q.exec();
        if (!r) {
            qWarning() << "Could not execute findMaxVersion query" << q.boundValues() << q.lastError();
            return r;
        }

        r = q.first();
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(r, false);

        currentFilename = q.value(0).toString();
    }

    if (currentFilename != maxVersionFilename) {
        const PkString url = resourceType + "/" + maxVersionFilename;
        KoResourceSP resource = storage->resource(url);
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(resource, false);
        resource->setVersion(maxVersion);
        resource->setMD5Sum(storage->resourceMd5(url));
        resource->setStorageLocation(storage->location());
        r = makeResourceTheCurrentVersion(resourceId, resource);
    }

    return r;
}

bool KisResourceCacheDb::makeResourceTheCurrentVersion(int resourceId, KoResourceSP resource)
{
    bool r = false;

    PkSqlQuery q;
    r = q.prepare("UPDATE resources\n"
                  "SET name    = :name\n"
                  ", filename  = :filename\n"
                  ", tooltip   = :tooltip\n"
                  ", thumbnail = :thumbnail\n"
                  ", status    = 1\n"
                  ", md5sum    = :md5sum\n"
                  "WHERE id    = :id");
    if (!r) {
        qWarning() << "Could not prepare updateResource statement" << q.lastError();
        return r;
    }

    q.bindValue(":name", resource->name());
    q.bindValue(":filename", resource->filename());
    q.bindValue(":tooltip", resource->name());
    q.bindValue(":md5sum", resource->md5Sum());

    q.bindValue(":thumbnail", KisResourceThumbnailCodec::encodePng(resource->thumbnail()));
    q.bindValue(":id", resourceId);

    r = q.exec();
    if (!r) {
        qWarning() << "Could not update resource" << q.boundValues() << q.lastError();
    }

    if (!resource->metadata().isEmpty()) {
        return updateMetaDataForId(resource->metadata(), resourceId, METADATA_RESOURCES);
    }

    return r;
}

bool KisResourceCacheDb::removeResourceCompletely(int resourceId)
{
    bool r = false;

    {
        PkSqlQuery q;
        r = q.prepare("DELETE FROM versioned_resources \n"
                      "WHERE resource_id = :resource_id;");

        if (!r) {
            qWarning() << "Could not prepare removeResourceCompletely1 statement" << q.lastError();
            return r;
        }

        q.bindValue(":resource_id", resourceId);
        r = q.exec();
        if (!r) {
            qWarning() << "Could not execute removeResourceCompletely1 statement" << q.lastError() << resourceId;
            return r;
        }
    }

    {
        PkSqlQuery q;
        r = q.prepare("DELETE FROM resources \n"
                      "WHERE id = :resource_id;");

        if (!r) {
            qWarning() << "Could not prepare removeResourceCompletely2 statement" << q.lastError();
            return r;
        }

        q.bindValue(":resource_id", resourceId);
        r = q.exec();
        if (!r) {
            qWarning() << "Could not execute removeResourceCompletely2 statement" << q.lastError() << resourceId;
            return r;
        }
    }

    {
        PkSqlQuery q;
        r = q.prepare("DELETE FROM resource_tags \n"
                      "WHERE resource_id = :resource_id;");

        if (!r) {
            qWarning() << "Could not prepare removeResourceCompletely3 statement" << q.lastError();
            return r;
        }

        q.bindValue(":resource_id", resourceId);
        r = q.exec();
        if (!r) {
            qWarning() << "Could not execute removeResourceCompletely3 statement" << q.lastError() << resourceId;
            return r;
        }
    }

    {
        PkSqlQuery q;
        r = q.prepare("DELETE FROM metadata \n"
                      "WHERE foreign_id = :resource_id\n"
                      "AND    table_name = :table;");

        if (!r) {
            qWarning() << "Could not prepare removeResourceCompletely4 statement" << q.lastError();
            return r;
        }

        q.bindValue(":resource_id", resourceId);
        q.bindValue(":table", METADATA_RESOURCES);
        r = q.exec();
        if (!r) {
            qWarning() << "Could not execute removeResourceCompletely4 statement" << q.lastError() << resourceId;
            return r;
        }
    }

    return r;
}

bool KisResourceCacheDb::getResourceIdFromFilename(PkString filename, PkString resourceType, PkString storageLocation, int &outResourceId)
{
    PkSqlQuery q;

    bool r = q.prepare("SELECT resources.id FROM resources\n"
                       ", resource_types\n"
                       ", storages\n"
                       "WHERE resources.filename = :filename\n" // bind to filename
                       "AND resource_types.id = resources.resource_type_id\n"  // join resources_types + resources
                       "AND resource_types.name = :resourceType\n" // bind to resource type
                       "AND resources.storage_id = storages.id\n" // join resources + storages
                       "AND storages.location = :storageLocation"); // bind to storage location

    if (!r) {
        qWarning() << "Could not prepare getResourceIdFromFilename statement" << q.lastError() << q.executedQuery();
        return r;
    }

    q.bindValue(":filename", filename);
    q.bindValue(":resourceType", resourceType);
    q.bindValue(":storageLocation",  changeToEmptyIfNull(storageLocation));

    r = q.exec();
    if (!r) {
        qWarning() << "Could not execute getResourceIdFromFilename statement" << q.lastError() << filename << resourceType;
        return r;
    }

    r = q.first();
    if (r) {
        outResourceId = q.value("resources.id").toInt();
    }

    return r;
}

bool KisResourceCacheDb::getResourceIdFromVersionedFilename(PkString filename, PkString resourceType, PkString storageLocation, int &outResourceId)
{
    PkSqlQuery q;

    bool r = q.prepare("SELECT resource_id FROM versioned_resources\n"
                       ", resources\n"
                       ", resource_types\n"
                       ", storages\n"
                       "WHERE versioned_resources.filename = :filename\n" // bind to filename
                       "AND resources.id = versioned_resources.resource_id\n" // join resources + versioned_resources
                       "AND resource_types.id = resources.resource_type_id\n"  // join resources_types + resources
                       "AND resource_types.name = :resourceType\n" // bind to resource type
                       "AND resources.storage_id = storages.id\n" // join resources + storages
                       "AND storages.location = :storageLocation"); // bind to storage location

    if (!r) {
        qWarning() << "Could not prepare getResourceIdFromVersionedFilename statement" << q.lastError() << q.executedQuery();
        return r;
    }


    q.bindValue(":filename", filename);
    q.bindValue(":resourceType", resourceType);
    q.bindValue(":storageLocation",  changeToEmptyIfNull(storageLocation));

    r = q.exec();
    if (!r) {
        qWarning() << "Could not execute getResourceIdFromVersionedFilename statement" << q.lastError() << filename << resourceType;
        return r;
    }

    r = q.first();
    if (r) {
        outResourceId = q.value("resource_id").toInt();
    }

    return r;
}

bool KisResourceCacheDb::getAllVersionsLocations(int resourceId, PkStringList &outVersionsLocationsList)
{
    PkSqlQuery q;
    bool r = q.prepare("SELECT filename FROM versioned_resources \n"
                  "WHERE resource_id = :resource_id;");

    if (!r) {
        qWarning() << "Could not prepare getAllVersionsLocations statement" << q.lastError();
        return r;
    }

    q.bindValue(":resource_id", resourceId);
    r = q.exec();
    if (!r) {
        qWarning() << "Could not execute getAllVersionsLocations statement" << q.lastError() << resourceId;
        return r;
    }

    outVersionsLocationsList = PkStringList();
    while (q.next()) {
        outVersionsLocationsList << q.value("filename").toString();
    }

    return r;

}

bool KisResourceCacheDb::addResource(KisResourceStorageSP storage, PkDateTime timestamp, KoResourceSP resource, const PkString &resourceType)
{
    bool r = false;

    if (!isValid()) {
        qWarning() << "KisResourceCacheDb::addResource: The database is not valid";
        return false;
    }

    if (!resource || !resource->valid()) {
        qWarning() << "KisResourceCacheDb::addResource: The resource is not valid:" << resource->filename();
        // We don't care about invalid resources and will just ignore them.
        return true;
    }
    bool temporary = (storage->type() == KisResourceStorage::StorageType::Memory);

    // Check whether it already exists
    int resourceId = resourceIdForResource(resource->filename(), resourceType, KisResourceLocator::instance()->makeStorageLocationRelative(storage->location()));
    if (resourceId > -1) {
        return true;
    }

    PkSqlQuery q;
    r = q.prepare("INSERT INTO resources \n"
                  "(storage_id, resource_type_id, name, filename, tooltip, thumbnail, status, temporary, md5sum) \n"
                  "VALUES \n"
                  "((SELECT  id "
                  "  FROM    storages "
                  "  WHERE   location = :storage_location)\n"
                  ", (SELECT id\n"
                  "   FROM   resource_types\n"
                  "   WHERE  name = :resource_type)\n"
                  ", :name\n"
                  ", :filename\n"
                  ", :tooltip\n"
                  ", :thumbnail\n"
                  ", :status\n"
                  ", :temporary\n"
                  ", :md5sum)");

    if (!r) {
        qWarning() << "Could not prepare addResource statement" << q.lastError();
        return r;
    }

    q.bindValue(":resource_type", resourceType);
    q.bindValue(":storage_location", changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storage->location())));
    q.bindValue(":name", resource->name());
    q.bindValue(":filename", resource->filename());

    PkString translationContext;
    if (storage->type() == KisResourceStorage::StorageType::Bundle) {
        translationContext = "./krita/data/bundles/" + KisResourceLocator::instance()->makeStorageLocationRelative(storage->location())
                + ":" + resourceType + "/" + resource->filename();
    } else if (storage->location() == "memory") {
        translationContext = "memory/" + resourceType + "/" + resource->filename();
    }
    else if (endsWithAsciiCaseInsensitive(resource->filename(), ".myb")) {
        translationContext = "./plugins/paintops/mypaint/brushes/" + resource->filename();
    } else {
        translationContext = "./krita/data/" + resourceType + "/" + resource->filename();
    }

    {
        (void)translationContext;
        q.bindValue(":tooltip", resource->name().isEmpty()
                                    ? completeBaseName(resource->filename())
                                    : resource->name());
    }

    q.bindValue(":thumbnail", KisResourceThumbnailCodec::encodePng(resource->image()));

    q.bindValue(":status", resource->active());
    q.bindValue(":temporary", (temporary ? 1 : 0));
    q.bindValue(":md5sum", resource->md5Sum());

    r = q.exec();
    if (!r) {
        qWarning() << "Could not execute addResource statement" << q.lastError() << q.boundValues();
        return r;
    }
    resourceId = resourceIdForResource(resource->filename(), resourceType, KisResourceLocator::instance()->makeStorageLocationRelative(storage->location()));

    if (resourceId < 0) {

        qWarning() << "Adding to database failed, resource id after adding is " << resourceId << "! (Probable reason: the resource has the same filename, storage, resource type as an existing resource). Resource is: "
                   << resource->name()
                   << resource->filename()
                   << resourceType
                   << KisResourceLocator::instance()->makeStorageLocationRelative(storage->location());
        return false;
    }

    resource->setResourceId(resourceId);

    if (!addResourceVersionImpl(resourceId, timestamp, storage, resource)) {
        qWarning() << "Could not add resource version" << resource;
        return false;
    }

    if (!resource->metadata().isEmpty()) {
        return updateMetaDataForId(resource->metadata(), resource->resourceId(), METADATA_RESOURCES);
    }

    return true;


}

bool KisResourceCacheDb::addResources(KisResourceStorageSP storage, PkString resourceType)
{
    KisDatabaseTransactionLock transaction(
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
    if (!transaction.transactionStarted()) {
        return false;
    }
    PkSharedPointer<KisResourceStorage::ResourceIterator> iter = storage->resources(resourceType);
    while (iter->hasNext()) {
        iter->next();

        PkSharedPointer<KisResourceStorage::ResourceIterator> verIt =
            iter->versions();

        int resourceId = -1;

        while (verIt->hasNext()) {
            verIt->next();

            KoResourceSP resource = verIt->resource();
            if (resource && resource->valid()) {
                resource->setVersion(verIt->guessedVersion());
                resource->setMD5Sum(storage->resourceMd5(verIt->url()));

                if (resourceId < 0) {
                    if (addResource(storage, iter->lastModified(), resource, iter->type())) {
                        resourceId = resource->resourceId();
                    } else {
                        qWarning() << "Could not add resource" << resource->filename() << "to the database";
                    }
                } else {
                    if (!addResourceVersion(resourceId, iter->lastModified(), storage, resource)) {
                        qWarning() << "Could not add resource version" << resource->filename() << "to the database";
                    }
                }
            }
        }
    }
    return transaction.commit();
}

bool KisResourceCacheDb::setResourceActive(int resourceId, bool active)
{
    if (resourceId < 0) {
        qWarning() << "Invalid resource id; cannot remove resource";
        return false;
    }
    PkSqlQuery q;
    bool r = q.prepare("UPDATE resources\n"
                       "SET    status = :status\n"
                       "WHERE  id = :resource_id");
    if (!r) {
        qWarning() << "Could not prepare removeResource query" << q.lastError();
    }
    q.bindValue(":status", active);
    q.bindValue(":resource_id", resourceId);
    if (!q.exec()) {
        qWarning() << "Could not update resource" << resourceId << "to  inactive" << q.lastError();
        return false;
    }

    return true;
}

bool KisResourceCacheDb::tagResource(const PkString &resourceFileName, KisTagSP tag, const PkString &resourceType)
{
    // Get tag id
    int tagId {-1};
    {
        PkSqlQuery q;
        if (!q.prepare(embeddedSql("select_tag.sql"))) {
            qWarning() << "Could not read and prepare select_tag.sql" << q.lastError();
            return false;
        }
        q.bindValue(":url", tag->url());
        q.bindValue(":resource_type", resourceType);

        if (!q.exec()) {
            qWarning() << "Could not query tags" << q.boundValues() << q.lastError();
            return false;
        }

        if (!q.first()) {
            qWarning() << "Could not find tag" << q.boundValues() << q.lastError();
            return false;
        }

        tagId = q.value(0).toInt();
    }


    // Get resource id
    PkSqlQuery q;
    bool r = q.prepare("SELECT resources.id\n"
                       "FROM   resources\n"
                       ",      resource_types\n"
                       "WHERE  resources.resource_type_id = resource_types.id\n"
                       "AND    resource_types.name = :resource_type\n"
                       "AND    resources.filename = :resource_filename\n");
    if (!r) {
        qWarning() << "Could not prepare tagResource query" << q.lastError();
        return false;
    }

    q.bindValue(":resource_type", resourceType);
    q.bindValue(":resource_filename", resourceFileName);

    if (!q.exec()) {
        qWarning() << "Could not execute tagResource statement" << q.boundValues() << q.lastError();
        return false;
    }


    while (q.next()) {

        int resourceId = q.value(0).toInt();

        if (resourceId < 0) {
            qWarning() << "Could not find resource to tag" << resourceFileName << resourceType;
            continue;
        }

        {
            PkSqlQuery q;
            if (!q.prepare("SELECT COUNT(*)\n"
                           "FROM   resource_tags\n"
                           "WHERE  resource_id = :resource_id\n"
                           "AND    tag_id = :tag_id")) {
                qWarning() << "Could not prepare tagResource query 2" << q.lastError();
                continue;
            }
            q.bindValue(":resource_id", resourceId);
            q.bindValue(":tag_id", tagId);

            if (!q.exec()) {
                qWarning() << "Could not execute tagResource query 2" << q.lastError() << q.boundValues();
                continue;
            }

            q.first();
            int count = q.value(0).toInt();
            if (count > 0) {
                continue;
            }
        }

        {
            PkSqlQuery q;
            if (!q.prepare("INSERT INTO resource_tags\n"
                           "(resource_id, tag_id)\n"
                           "VALUES\n"
                           "(:resource_id, :tag_id);")) {
                qWarning() << "Could not prepare tagResource insert statement" << q.lastError();
                continue;
            }

            q.bindValue(":resource_id", resourceId);
            q.bindValue(":tag_id", tagId);

            if (!q.exec()) {
                qWarning() << "Could not execute tagResource stagement" << q.boundValues() << q.lastError();
                continue;
            }
        }
    }
    return true;
}

bool KisResourceCacheDb::hasTag(const PkString &url, const PkString &resourceType)
{
    PkSqlQuery q;
    if (!q.prepare(embeddedSql("select_tag.sql"))) {
        qWarning() << "Could not read and prepare select_tag.sql" << q.lastError();
        return false;
    }
    q.bindValue(":url", url);
    q.bindValue(":resource_type", resourceType);
    if (!q.exec()) qWarning() << "Could not query tags" << q.boundValues() << q.lastError();
    return q.first();
}

bool KisResourceCacheDb::linkTagToStorage(const PkString &url, const PkString &resourceType, const PkString &storageLocation)
{
    PkSqlQuery q;
    if (!q.prepare("INSERT INTO tags_storages\n"
                   "(tag_id, storage_id)\n"
                   "VALUES\n"
                   "(\n"
                   " ( SELECT id\n"
                   "   FROM  tags\n"
                   "   WHERE url = :url\n"
                   "   AND   resource_type_id = (SELECT id \n"
                   "                              FROM   resource_types\n"
                   "                              WHERE  name = :resource_type)"
                   " )\n"
                   ",( SELECT id\n"
                   "   FROM   storages\n"
                   "   WHERE  location = :storage_location\n"
                   " )\n"
                   ");")) {
        qWarning() << "Could not prepare add tag/storage statement" << q.lastError();
        return false;
    }

    q.bindValue(":url", url);
    q.bindValue(":resource_type", resourceType);
    q.bindValue(":storage_location", changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storageLocation)));

    if (!q.exec()) {
        qWarning() << "Could not insert tag/storage link" << q.boundValues() << q.lastError();
        return false;
    }
    return true;
}


bool KisResourceCacheDb::addTag(const PkString &resourceType, const PkString storageLocation, KisTagSP tag)
{
    if (hasTag(tag->url(), resourceType)) {
        // Check whether this storage is already registered for this tag
        PkSqlQuery q;
        if (!q.prepare("SELECT storages.location\n"
                       "FROM   tags_storages\n"
                       ",      tags\n"
                       ",      storages\n"
                       "WHERE  tags.id = tags_storages.tag_id\n"
                       "AND    storages.id = tags_storages.storage_id\n"
                       "AND    tags.resource_type_id = (SELECT id\n"
                       "                                FROM   resource_types\n"
                       "                                WHERE  name = :resource_type)\n"
                       "AND    storages.location = :storage_location\n"
                       "AND    tags.url = :url"))
        {
            qWarning() << "Could not prepare select tags from tags_storages query" << q.lastError();
        }

        q.bindValue(":url", tag->url());
        q.bindValue(":resource_type", resourceType);
        q.bindValue(":storage_location", changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storageLocation)));

        if (!q.exec()) {
            qWarning() << "Could not execute tags_storages query" << q.boundValues() << q.lastError();
        }

        // If this tag is not yet linked to the storage, link it
        if (!q.first()) {
            return linkTagToStorage(tag->url(), resourceType, storageLocation);
        }

        return true;
    }

    int tagId;

    // Insert the tag
    {
        PkSqlQuery q;
        if (!q.prepare("INSERT INTO tags\n"
                       "(url, name, comment, resource_type_id, active, filename)\n"
                       "VALUES\n"
                       "( :url\n"
                       ", :name\n"
                       ", :comment\n"
                       ", (SELECT id\n"
                       "   FROM   resource_types\n"
                       "   WHERE  name = :resource_type)\n"
                       ", 1\n"
                       ", :filename\n"
                       ");")) {
            qWarning() << "Could not prepare insert tag statement" << q.lastError();
            return false;
        }

        q.bindValue(":url", tag->url());
        q.bindValue(":name", tag->name(false));
        q.bindValue(":comment", tag->comment(false));
        q.bindValue(":resource_type", resourceType);
        q.bindValue(":filename", tag->filename());

        if (!q.exec()) {
            qWarning() << "Could not insert tag" << q.boundValues() << q.lastError();
        }

        tagId = q.lastInsertId().toInt();
    }

    {
        for (const PkString language : tag->names().keys()) {

            PkString name = tag->names()[language];
            PkString comment = name;
            if (tag->comments().contains(language)) {
                comment = tag->comments()[language];
            }

            PkSqlQuery q;
            if (!q.prepare("INSERT INTO tag_translations\n"
                           "( tag_id\n"
                           ", language\n"
                           ", name\n"
                           ", comment\n"
                           ")\n"
                           "VALUES\n"
                           "( :id\n"
                           ", :language\n"
                           ", :name\n"
                           ", :comment\n"
                           ");")) {
                qWarning() << "Could not prepare insert tag_translation query" << q.lastError();
            }

            q.bindValue(":id", tagId);
            q.bindValue(":language", language);
            q.bindValue(":name", name);
            q.bindValue(":comment", comment);

            if (!q.exec()) {
                qWarning() << "Could not execute insert tag_translation query" << q.lastError() << q.boundValues();
            }
        }

    }


    linkTagToStorage(tag->url(), resourceType, storageLocation);

    return true;
}

bool KisResourceCacheDb::addTags(KisResourceStorageSP storage, PkString resourceType)
{
    KisDatabaseTransactionLock transaction(
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
    if (!transaction.transactionStarted()) {
        return false;
    }
    PkSharedPointer<KisResourceStorage::TagIterator> iter = storage->tags(resourceType);
    while(iter->hasNext()) {
        iter->next();
        KisTagSP tag = iter->tag();
        if (tag && tag->valid()) {
            if (!addTag(resourceType, storage->location(), tag)) {
                qWarning() << "Could not add tag" << tag << "to the database";
                continue;
            }
            if (!tag->defaultResources().isEmpty()) {
                for (const PkString &resourceFileName : tag->defaultResources()) {
                    if (!tagResource(resourceFileName, tag, resourceType)) {
                        qWarning() << "Could not tag resource" << fileBaseName(resourceFileName) << "from" << storage->name() << "filename" << resourceFileName << "with tag" << iter->tag();
                    }
                }
            }
        }
    }
    return transaction.commit();
}

bool KisResourceCacheDb::registerStorageType(const KisResourceStorage::StorageType storageType)
{
    // Check whether the type already exists
    const PkString name = KisResourceStorage::storageTypeToUntranslatedString(storageType);

    {
        PkSqlQuery q;
        if (!q.prepare("SELECT count(*)\n"
                       "FROM   storage_types\n"
                       "WHERE  name = :storage_type\n")) {
            qWarning() << "Could not prepare select from storage_types query" << q.lastError();
            return false;
        }
        q.bindValue(":storage_type", name);
        if (!q.exec()) {
            qWarning() << "Could not execute select from storage_types query" << q.lastError();
            return false;
        }
        q.first();
        int rowCount = q.value(0).toInt();
        if (rowCount > 0) {
            return true;
        }
    }
    // if not, add it
    PkSqlQuery q(embeddedSql("fill_storage_types.sql"));
    q.addBindValue(name);
    if (!q.exec()) {
        qWarning() << "Could not insert" << name << q.lastError();
        return false;
    }
    return true;
}

bool KisResourceCacheDb::addStorage(KisResourceStorageSP storage, bool preinstalled)
{
    bool r = true;

    if (!isValid()) {
        qWarning() << "The database is not valid";
        return false;
    }

    {
        PkSqlQuery q;
        r = q.prepare("SELECT * FROM storages WHERE location = :location");
        q.bindValue(":location", changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storage->location())));
        r = q.exec();
        if (!r) {
            qWarning() << "Could not select from storages";
            return r;
        }
        if (q.first()) {
            debugResource << "Storage already exists" << storage;
            return true;
        }
    }

    // Insert the storage;
    {
        PkSqlQuery q;

        r = q.prepare("INSERT INTO storages\n "
                      "(storage_type_id, location, timestamp, pre_installed, active, thumbnail)\n"
                      "VALUES\n"
                      "(:storage_type_id, :location, :timestamp, :pre_installed, :active, :thumbnail);");

        if (!r) {
            qWarning() << "Could not prepare query" << q.lastError();
            return r;
        }

        const PkString sanitizedStorageLocation =
            changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storage->location()));

        q.bindValue(":storage_type_id", static_cast<int>(storage->type()));
        q.bindValue(":location", sanitizedStorageLocation);
        q.bindValue(":timestamp", static_cast<long long>(storage->timestamp().toSecsSinceEpoch()));
        q.bindValue(":pre_installed", preinstalled ? 1 : 0);
        q.bindValue(":active", !disabledBundles.contains(storage->name()));

        q.bindValue(":thumbnail", KisResourceThumbnailCodec::encodePng(storage->thumbnail()));

        r = q.exec();

        if (!r) qWarning() << "Could not execute query" << q.lastError();

        if (!q.prepare("SELECT id\n"
                       "FROM   storages\n"
                       "WHERE  location = :location\n")) {
            qWarning() << "Could not prepare storage id statement" << q.lastError();
        }

        q.bindValue(":location", sanitizedStorageLocation);
        if (!q.exec()) {
            qWarning() << "Could not execute storage id statement" << q.boundValues() << q.lastError();
        }

        if (!q.first()) {
            qWarning() << "Could not find id for the newly added storage" << q.lastError();
        } else {
            storage->setStorageId(q.value("id").toInt());
        }
    }

    // Insert the metadata
    {
        PkStringList keys = storage->metaDataKeys();
        if (keys.size() > 0 && storage->storageId() >= 0) {

            PkMap<PkString, PkVariant> metadata;

            for (const PkString &key : storage->metaDataKeys()) {
                metadata[key] = storage->metaData(key);
            }

            updateMetaDataForId(metadata, storage->storageId(), METADATA_STORAGES);
        }
    }

    for (const PkString &resourceType : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        if (!KisResourceCacheDb::addResources(storage, resourceType)) {
            qWarning() << "Failed to add all resources for storage" << storage;
            r = false;
        }
    }

    return r;
}

bool KisResourceCacheDb::addStorageTags(KisResourceStorageSP storage)
{

    bool r = true;
    for (const PkString &resourceType : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        if (!KisResourceCacheDb::addTags(storage, resourceType)) {
            qWarning() << "Failed to add all tags for storage" << storage;
            r = false;
        }
    }
    return r;
}

bool KisResourceCacheDb::deleteStorage(PkString location)
{
    // location is already relative

    try {
        KisDatabaseTransactionLock transactionLock(
            PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
        if (!transactionLock.transactionStarted()) {
            return false;
        }

        {
            KisSqlQueryLoader loader(":/sql/delete_versioned_resources_for_storage_indirect.sql",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":location", changeToEmptyIfNull(location));
            loader.exec();
        }

        {
            KisSqlQueryLoader loader(":/sql/delete_resource_tags_for_storage_indirect.sql",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":location", changeToEmptyIfNull(location));
            loader.exec();
        }

        {
            KisSqlQueryLoader loader(":/sql/delete_versioned_resources_for_storage_direct.sql",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":location", changeToEmptyIfNull(location));
            loader.exec();
            if (loader.query().numRowsAffected() > 0) {
                qWarning() << "WARNING: deleteStorage: versioned_resurces table contained resource versions not being "
                              "present in the main table. Deleted: "
                           << loader.query().numRowsAffected();
            }
        }

        {
            KisSqlQueryLoader loader(":/sql/delete_resource_metadata_for_storage.sql",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":location", changeToEmptyIfNull(location));
            loader.query().bindValue(":table", METADATA_RESOURCES);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_current_resources_for_storage",
                                     "DELETE FROM resources\n"
                                     "WHERE storage_id = (SELECT storages.id\n"
                                     "                    FROM   storages\n"
                                     "                    WHERE storages.location = :location)\n",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":location", changeToEmptyIfNull(location));
            loader.exec();
        }

        /**
         * Remove only the storage-unique tags
         *
         * We should first get the list of storage-unique tags, and then remove
         * **all** the links between this storage and (any) tags, including the
         * shared ones. The unique tags will be removed, but shared will only
         * decrease their reference counter.
         *
         * NOTE: we cannot use temporary tables here, since our models may
         * have queries open at the moment, which means we will not be able
         * to remove these temporary tables
         */
        PkVariantList uniqueTagIdsToDelete;
        {
            auto [unique, shared] = tagsForStorage(ResourceType::PaintOpPresets, location);
            std::copy(unique.begin(), unique.end(), std::back_inserter(uniqueTagIdsToDelete));
        }

        {
                KisSqlQueryLoader loader("inline://delete_tags_storage_links_for_storage",
                                         "WITH storage_id_query AS (\n"
                                         "    SELECT storages.id\n"
                                         "    FROM storages\n"
                                         "    WHERE storages.location = :location)\n"
                                         "DELETE FROM tags_storages\n"
                                         "WHERE storage_id IN storage_id_query\n",
                                         KisSqlQueryLoader::single_statement_mode);
                loader.query().bindValue(":location", changeToEmptyIfNull(location));
                loader.exec();
        }

        if (!uniqueTagIdsToDelete.empty()) {
            {
                KisSqlQueryLoader loader("inline://delete_tags_translations_for_storage",
                                         "DELETE FROM tag_translations WHERE tag_id = ?",
                                         KisSqlQueryLoader::single_statement_mode);
                loader.query().addBindValue(uniqueTagIdsToDelete);
                loader.execBatch();
            }

            {
                KisSqlQueryLoader loader("inline://delete_resource_tags_for_storage",
                                         "DELETE FROM resource_tags WHERE tag_id = ?",
                                         KisSqlQueryLoader::single_statement_mode);
                loader.query().addBindValue(uniqueTagIdsToDelete);
                loader.execBatch();
            }

            {
                KisSqlQueryLoader loader("inline://delete_tags_for_storage",
                                         "DELETE FROM tags WHERE id = ?",
                                         KisSqlQueryLoader::single_statement_mode);
                loader.query().addBindValue(uniqueTagIdsToDelete);
                loader.execBatch();
            }
        }

        {
            KisSqlQueryLoader loader("inline://delete_starage_metadata_for_storage",
                                     "DELETE FROM metadata\n"
                                     "WHERE foreign_id = (SELECT storages.id\n"
                                     "                    FROM   storages\n"
                                     "                    WHERE  storages.location = :location)"
                                     "AND table_name = :table;",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":location", changeToEmptyIfNull(location));
            loader.query().bindValue(":table", METADATA_STORAGES);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_storage",
                                     "DELETE FROM storages\n"
                                     "WHERE location = :location;",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":location", changeToEmptyIfNull(location));
            loader.exec();
        }

        if (!transactionLock.commit()) {
            return false;
        }

    } catch (const KisSqlQueryLoader::FileException &e) {
        qWarning().noquote() << "ERROR: deleteStorage:" << e.message;
        qWarning().noquote() << "       file:" << e.filePath;
        qWarning().noquote() << "       error:" << e.fileErrorString;
        return false;
    } catch (const KisSqlQueryLoader::SQLException &e) {
        qWarning().noquote() << "ERROR: deleteStorage:" << e.message;
        qWarning().noquote() << "       file:" << e.filePath;
        qWarning().noquote() << "       statement:" << e.statementIndex;
        qWarning().noquote() << "       error:" << e.sqlError.text();
        return false;
    }

    return true;
}

bool KisResourceCacheDb::deleteStorage(KisResourceStorageSP storage)
{
    return deleteStorage(KisResourceLocator::instance()->makeStorageLocationRelative(storage->location()));
}

namespace {
struct ResourceVersion : public boost::less_than_comparable<ResourceVersion>
{
    int resourceId = -1;
    int version = -1;
    PkDateTime timestamp;
    PkString url;

    bool operator<(const ResourceVersion &rhs) const {
        return resourceId < rhs.resourceId ||
                (resourceId == rhs.resourceId && version < rhs.version);
    }

    struct CompareByResourceId {
        bool operator() (const ResourceVersion &lhs, const ResourceVersion &rhs) const {
            return lhs.resourceId < rhs.resourceId;
        }
    };


};

[[maybe_unused]]
PkDebug operator<<(PkDebug dbg, const ResourceVersion &ver)
{
    dbg.nospace() << "ResourceVersion("
                  << ver.resourceId << ", "
                  << ver.version << ", "
                  << ver.timestamp << ", "
                  << ver.url << ")";

    return dbg.space();
}
}

bool KisResourceCacheDb::synchronizeStorage(KisResourceStorageSP storage)
{
    PkElapsedTimer t;
    t.start();

    if (!isValid()) {
        qWarning() << "KisResourceCacheDb::addResource: The database is not valid";
        return false;
    }

    bool success = true;

    // Find the storage in the database
    PkSqlQuery q;
    if (!q.prepare("SELECT id\n"
                   ",      timestamp\n"
                   ",      pre_installed\n"
                   "FROM   storages\n"
                   "WHERE  location = :location\n")) {
        qWarning() << "Could not prepare storage timestamp statement" << q.lastError();
    }

    q.bindValue(":location", changeToEmptyIfNull(KisResourceLocator::instance()->makeStorageLocationRelative(storage->location())));
    if (!q.exec()) {
        qWarning() << "Could not execute storage timestamp statement" << q.boundValues() << q.lastError();
    }

    if (!q.first()) {
        // This is a new storage, the user must have dropped it in the path before restarting Krita, so add it.
        debugResource << "Adding storage to the database:" << storage;
        if (!addStorage(storage, false)) {
            qWarning() << "Could not add new storage" << storage->name() << "to the database";
            success = false;
        }
        return success;
    }

    storage->setStorageId(q.value("id").toInt());

    /// Start the transaction that will add all the resources
    KisDatabaseTransactionLock transaction(
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
    if (!transaction.transactionStarted()) {
        return false;
    }

    /// We compare resource versions one-by-one because the storage may have multiple
    /// versions of them

    for (const PkString &resourceType : KisResourceLoaderRegistry::instance()->resourceTypes()) {

        /// Firstly, fetch information about the existing resources
        /// in the storage

        PkVector<ResourceVersion> resourcesInStorage;

        /// A fake resourceId to group resources which are not yet present
        /// in the database. This value is always negative, therefore it
        /// cannot overlap with normal ids.

        int nextInexistentResourceId = std::numeric_limits<int>::min();

        PkSharedPointer<KisResourceStorage::ResourceIterator> iter = storage->resources(resourceType);
        while (iter->hasNext()) {
            iter->next();

            const int firstResourceVersionPosition = resourcesInStorage.size();

            int detectedResourceId = nextInexistentResourceId;
            PkSharedPointer<KisResourceStorage::ResourceIterator> verIt =
                    iter->versions();

            while (verIt->hasNext()) {
                verIt->next();

                // verIt->url() contains paths like "brushes/ink.png" or "brushes/subfolder/splash.png".
                // we need to cut off the first part and get "ink.png" in the first case,
                // but "subfolder/splash.png" in the second case in order for subfolders to work
                // so it cannot just use a basename helper here.
                std::string normalizedPath = verIt->url().PkToUtf8();
                std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
                const std::size_t folderEndIdx = normalizedPath.find('/');
                const PkString properFilenameWithSubfolders(
                    folderEndIdx == std::string::npos
                        ? normalizedPath.c_str()
                        : normalizedPath.substr(folderEndIdx + 1).c_str());
                int id = resourceIdForResource(properFilenameWithSubfolders,
                                               verIt->type(),
                                               KisResourceLocator::instance()->makeStorageLocationRelative(storage->location()));

                ResourceVersion item;
                item.url = verIt->url();
                item.version = verIt->guessedVersion();

                // we use lower precision than the normal PkDateTime
                item.timestamp = PkDateTime::fromSecsSinceEpoch(verIt->lastModified().toSecsSinceEpoch());

                item.resourceId = id;

                if (detectedResourceId < 0 && id >= 0) {
                    detectedResourceId = id;
                }

                resourcesInStorage.append(item);
            }

            /// Assign the detected resource id to all the versions of
            /// this resource (if they are not present in the database).
            /// If no id has been detected, then a fake one will be assigned.

            for (int i = firstResourceVersionPosition; i < resourcesInStorage.size(); i++) {
                if (resourcesInStorage[i].resourceId < 0) {
                    resourcesInStorage[i].resourceId = detectedResourceId;
                }
            }

            nextInexistentResourceId++;
        }


        /// Secondly, fetch the resources present in the database

        PkVector<ResourceVersion> resourcesInDatabase;

        PkSqlQuery q;
        q.setForwardOnly(true);
        if (!q.prepare("SELECT versioned_resources.resource_id, versioned_resources.filename, versioned_resources.version, versioned_resources.timestamp\n"
                       "FROM   versioned_resources\n"
                       ",      resource_types\n"
                       ",      resources\n"
                       "WHERE  resources.resource_type_id = resource_types.id\n"
                       "AND    resources.id = versioned_resources.resource_id\n"
                       "AND    resource_types.name = :resource_type\n"
                       "AND    versioned_resources.storage_id == :storage_id")) {
            qWarning() << "Could not prepare resource by type query" << q.lastError();
            success = false;
            continue;
        }

        q.bindValue(":resource_type", resourceType);
        q.bindValue(":storage_id", int(storage->storageId()));

        if (!q.exec()) {
            qWarning() << "Could not exec resource by type query" << q.boundValues() << q.lastError();
            success = false;
            continue;
        }

        while (q.next()) {
            ResourceVersion item;
            item.url = resourceType + "/" + q.value(1).toString();
            item.version = q.value(2).toInt();
            item.timestamp = PkDateTime::fromSecsSinceEpoch(q.value(3).toInt());
            item.resourceId = q.value(0).toInt();

            resourcesInDatabase.append(item);
        }

        PkSet<int> resourceIdForUpdate;

        std::sort(resourcesInStorage.begin(), resourcesInStorage.end());
        std::sort(resourcesInDatabase.begin(), resourcesInDatabase.end());

        auto itA = resourcesInStorage.begin();
        auto endA = resourcesInStorage.end();

        auto itB = resourcesInDatabase.begin();
        auto endB = resourcesInDatabase.end();

        /// The head of itA array contains some resources with fake
        /// (negative) resourceId. These resources are obviously new
        /// resources and should be added to the cache database.

        while (itA != endA) {
            if (itA->resourceId >= 0) break;

            KoResourceSP res = storage->resource(itA->url);

            if (!res) {
                KisUsageLogger::log("Could not load resource " + itA->url);
                ++itA;
                continue;
            }

            res->setVersion(itA->version);
            res->setMD5Sum(storage->resourceMd5(itA->url));
            if (!res->valid()) {
                KisUsageLogger::log("Could not retrieve md5 for resource " + itA->url);
                ++itA;
                continue;
            }

            const bool retval = addResource(storage, itA->timestamp, res, resourceType);
            if (!retval) {
                KisUsageLogger::log("Could not add resource " + itA->url);
                ++itA;
                continue;
            }

            const int resourceId = res->resourceId();
            KIS_SAFE_ASSERT_RECOVER(resourceId >= 0) {
                KisUsageLogger::log("Could not get id for resource " + itA->url);
                ++itA;
                continue;
            }

            auto nextResource = std::upper_bound(itA, endA, *itA, ResourceVersion::CompareByResourceId());
            for (auto it = std::next(itA); it != nextResource; ++it) {
                KoResourceSP res = storage->resource(it->url);
                res->setVersion(it->version);
                res->setMD5Sum(storage->resourceMd5(it->url));
                if (!res->valid()) {
                    continue;
                }

                const bool retval = addResourceVersion(resourceId, it->timestamp, storage, res);
                KIS_SAFE_ASSERT_RECOVER(retval) {
                    KisUsageLogger::log("Could not add version for resource " + itA->url);
                    continue;
                }
            }

            itA = nextResource;
        }

        /// Now both arrays are sorted in resourceId/version/timestamp
        /// order. It lets us easily find the resources that are unique
        /// to the storage or database. If *itA < *itB, then the resource
        /// is present in the storage only and should be added to the
        /// database. If *itA > *itB, then the resource is present in
        /// the database only and should be removed (because it has been
        /// removed from the storage);

        while (itA != endA || itB != endB) {
            if ((itA != endA && itB != endB && *itA < *itB) ||
                    itB == endB) {

                // add a version to the database

                KoResourceSP res = storage->resource(itA->url);
                if (res) {
                    res->setVersion(itA->version);
                    res->setMD5Sum(storage->resourceMd5(itA->url));

                    const bool result = addResourceVersionImpl(itA->resourceId, itA->timestamp, storage, res);
                    KIS_SAFE_ASSERT_RECOVER_NOOP(result);

                    resourceIdForUpdate.insert(itA->resourceId);
                }
                ++itA;

            } else if ((itA != endA && itB != endB && *itA > *itB) ||
                       itA == endA) {

                // remove a version from the database
                const bool result = removeResourceVersionImpl(itB->resourceId, itB->version, storage);
                KIS_SAFE_ASSERT_RECOVER_NOOP(result);
                resourceIdForUpdate.insert(itB->resourceId);
                ++itB;

            } else {
                // resources are equal, just skip them
                ++itA;
                ++itB;
            }
        }


        /// In the main loop we modified the versioned_resource table only,
        /// now we should update the head resources table with the latest
        /// version of the resource (and upload the thumbnail as well)

        for (auto it = resourceIdForUpdate.begin(); it != resourceIdForUpdate.end(); ++it) {
            updateResourceTableForResourceIfNeeded(*it, resourceType, storage);
        }
    }

    if (!transaction.commit()) {
        success = false;
    }
    debugResource << "Synchronizing the storages took" << t.elapsed() << "milliseconds for" << storage->location();

    return success;
}

bool KisResourceCacheDb::deleteTemporaryResources()
{
    try {
        KisDatabaseTransactionLock transactionLock(
            PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
        if (!transactionLock.transactionStarted()) {
            return false;
        }

        /**
         * Remove all temporary resources
         */
        {
            KisSqlQueryLoader loader(
                "inline://delete_metadata_for_resources_in_memory_storages",
                "DELETE FROM metadata\n"
                "WHERE foreign_id IN (SELECT id\n"
                "                     FROM resources\n"
                "                     WHERE storage_id in (SELECT id\n"
                "                                          FROM storages\n"
                "                                          WHERE  storage_type_id == :storage_type))\n"
                "AND   table_name = :table",
                KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":table", METADATA_RESOURCES);
            loader.query().bindValue(":storage_type", (int)KisResourceStorage::StorageType::Memory);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_metadata_for_temporary_resources",
                                     "DELETE FROM metadata\n"
                                     "WHERE foreign_id IN (SELECT id\n"
                                     "                     FROM   resources\n"
                                     "                     WHERE temporary = 1)\n"
                                     "AND   table_name = :table",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":table", METADATA_RESOURCES);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_versions_of_resources_in_temporary_storages",
                                     "DELETE FROM versioned_resources\n"
                                     "WHERE  storage_id in (SELECT id\n"
                                     "                      FROM   storages\n"
                                     "                      WHERE  storage_type_id == :storage_type)",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":storage_type", (int)KisResourceStorage::StorageType::Memory);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_versions_of_temporary_resources",
                                     "DELETE FROM versioned_resources\n"
                                     "WHERE resource_id IN (SELECT id FROM resources\n"
                                     "                      WHERE  temporary = 1)",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_current_resources_in_temporary_storages",
                                     "DELETE FROM resources\n"
                                     "WHERE  storage_id in (SELECT id\n"
                                     "                      FROM   storages\n"
                                     "                      WHERE  storage_type_id  == :storage_type)",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":storage_type", (int)KisResourceStorage::StorageType::Memory);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_current_temporary_resources",
                                     "DELETE FROM resources\n"
                                     "WHERE  temporary = 1",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.exec();
        }

        /**
         * Remove all temporary storages
         */

        {
            KisSqlQueryLoader loader("inline://delete_metadata_for_temporary_storages",
                                     "DELETE FROM metadata\n"
                                     "WHERE foreign_id IN (SELECT id\n"
                                     "                     FROM   storages\n"
                                     "                     WHERE  storage_type_id  == :storage_type)\n"
                                     "AND   table_name = :table;",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":storage_type", (int)KisResourceStorage::StorageType::Memory);
            loader.query().bindValue(":table", METADATA_STORAGES);
            loader.exec();
        }

        {
            KisSqlQueryLoader loader("inline://delete_temporary_storages",
                                     "DELETE FROM storages\n"
                                     "WHERE  storage_type_id  == :storage_type\n",
                                     KisSqlQueryLoader::single_statement_mode);
            loader.query().bindValue(":storage_type", (int)KisResourceStorage::StorageType::Memory);
            loader.exec();
        }

        if (!transactionLock.commit()) {
            return false;
        }
    } catch (const KisSqlQueryLoader::SQLException &e) {
        qWarning().noquote() << "ERROR: failed to execute query:" << e.message;
        qWarning().noquote() << "       file:" << e.filePath;
        qWarning().noquote() << "       statement:" << e.statementIndex;
        qWarning().noquote() << "       error:" << e.sqlError.text();
        return false;
    }
    return true;
}

void KisResourceCacheDb::performHouseKeepingOnExit()
{
    PkSqlQuery q;

    if (!q.prepare("PRAGMA optimize;")) {
        qWarning() << "Could not prepare query" << q.lastQuery() << q.lastError();
    }

    if (!q.exec()) {
        qWarning() << "Could not execute query" << q.lastQuery() << q.lastError();
    }
}

void KisResourceCacheDb::setForeignKeysStateImpl(bool isEnabled)
{
    KisSqlQueryLoader loader("inline://set_foreign_keys_state",
                             PkString("PRAGMA foreign_keys = %1").arg(isEnabled ? "ON" : "OFF"));
    loader.exec();
}

bool KisResourceCacheDb::getForeignKeysStateImpl()
{
    KisSqlQueryLoader loader("inline://get_foreign_keys_state",
                             "PRAGMA foreign_keys");

    loader.exec();

    if (loader.query().first()) {
        return loader.query().value(0).toInt();
    }

    return false;
}

void KisResourceCacheDb::synchronizeForeignKeysState()
{
#ifdef KRITA_STABLE
    bool useForeignKeys = false;
    KisUsageLogger::log("INFO: detected stable build of Krita, foreign_keys constraint will be disabled");
#else
    bool useForeignKeys = true;
    KisUsageLogger::log("INFO: detected unstable build of Krita, foreign_keys constraint will be enabled");
#endif

    if (const char *overrideValue = std::getenv("KRITA_OVERRIDE_USE_FOREIGN_KEYS")) {
        useForeignKeys = std::strtol(overrideValue, nullptr, 10) > 0;
        KisUsageLogger::log(PkString("INFO: foreign_keys constraint was overridden by KRITA_OVERRIDE_USE_FOREIGN_KEYS: %1")
                                .arg(useForeignKeys ? 1 : 0));
    }

    try {
        const bool oldForeignKeysState = KisResourceCacheDb::getForeignKeysStateImpl();

        if (oldForeignKeysState != useForeignKeys) {
            KisUsageLogger::log(
                PkString("INFO: switch foreign_keys state: %1 -> %2")
                    .arg(oldForeignKeysState ? 1 : 0)
                    .arg(useForeignKeys ? 1 : 0));

            KisResourceCacheDb::setForeignKeysStateImpl(useForeignKeys);
        }

    } catch (const KisSqlQueryLoader::SQLException &e) {
        qWarning().noquote() << "ERROR: failed to execute query:" << e.message;
        qWarning().noquote() << "       file:" << e.filePath;
        qWarning().noquote() << "       statement:" << e.statementIndex;
        qWarning().noquote() << "       error:" << e.sqlError.text();
    }

}

bool KisResourceCacheDb::registerResourceType(const PkString &resourceType)
{
    // Check whether the type already exists
    {
        PkSqlQuery q;
        if (!q.prepare("SELECT count(*)\n"
                       "FROM   resource_types\n"
                       "WHERE  name = :resource_type\n")) {
            qWarning() << "Could not prepare select from resource_types query" << q.lastError();
            return false;
        }
        q.bindValue(":resource_type", resourceType);
        if (!q.exec()) {
            qWarning() << "Could not execute select from resource_types query" << q.lastError();
            return false;
        }
        q.first();
        int rowCount = q.value(0).toInt();
        if (rowCount > 0) {
            return true;
        }
    }
    // if not, add it
    PkSqlQuery q(embeddedSql("fill_resource_types.sql"));
    q.addBindValue(resourceType);
    if (!q.exec()) {
        qWarning() << "Could not insert" << resourceType << q.lastError();
        return false;
    }
    return true;
}

KisResourceCacheDb::MetaDataReadResult KisResourceCacheDb::metaDataReadResultForId(
    int id, const PkString &tableName)
{
    MetaDataReadResult result;
    if (detail::resourceDatabaseConnectionIsPoisoned()) {
        qWarning() << detail::resourceDatabaseConnectionPoisonError();
        return result;
    }
    PkSqlDatabase database =
        PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false);
    if (!database.isOpen()) {
        qWarning() << "Could not read metadata from a closed database connection";
        return result;
    }
    detail::ResourceDatabaseConnectionGuard connectionGuard(database);
    sqlite3 *nativeDatabase = database.PkHandle();
    if (!nativeDatabase) {
        return result;
    }

    result.rows.reserve(kMaximumMetadataRowsPerOwner);
    std::size_t rowCount = 0;
    std::uint64_t retainedPayloadBytes = 0;
    long long lastRowId = 0;
    bool firstPage = true;

    for (;;) {
        PkSqlQuery page;
        page.setForwardOnly(true);
        const PkString sql = firstPage
            ? PkString("SELECT rowid,typeof(key),typeof(value) "
                       "FROM metadata "
                       "WHERE +foreign_id=:id AND +table_name=:table "
                       "ORDER BY rowid LIMIT :page_limit")
            : PkString("SELECT rowid,typeof(key),typeof(value) "
                       "FROM metadata "
                       "WHERE +foreign_id=:id AND +table_name=:table "
                       "AND rowid>:last_rowid "
                       "ORDER BY rowid LIMIT :page_limit");
        if (!page.prepare(sql)) {
            qWarning() << "Could not prepare metadata page query" << page.lastError();
            result.querySucceeded = false;
            return result;
        }
        page.bindValue(":id", id);
        page.bindValue(":table", tableName);
        page.bindValue(":page_limit", kMetadataReadPageRows);
        if (!firstPage) {
            page.bindValue(":last_rowid", lastRowId);
        }
        if (!page.exec()) {
            qWarning() << "Could not execute metadata page query" << page.lastError();
            result.querySucceeded = false;
            return result;
        }
        result.querySucceeded = true;

        int rowsInPage = 0;
        while (page.next()) {
            ++rowsInPage;
            lastRowId = page.value(0).toLongLong();
            if (rowCount >= kMaximumMetadataRowsPerOwner) {
                result.resourceLimitExceeded = true;
                return result;
            }
            ++rowCount;

            MetaDataReadRow row;
            row.rowId = lastRowId;
            row.keyStorageClass = metadataStorageClass(page.value(1).toString());
            row.valueStorageClass = metadataStorageClass(page.value(2).toString());

            std::optional<MetaDataDecodeStatus> keyIssue;
            if (row.keyStorageClass == MetaDataStorageClass::Null) {
                row.rawKeyAvailable = true;
                keyIssue = MetaDataDecodeStatus::UnsupportedStorageClass;
            } else if (isByteStorageClass(row.keyStorageClass)) {
                const BoundedMetadataColumn keyColumn = readBoundedMetadataColumn(
                    nativeDatabase, row.rowId, "key", kMaximumMetadataKeyBytes);
                if (!keyColumn.succeeded) {
                    qWarning() << "Could not incrementally read metadata key" << row.rowId;
                    result.querySucceeded = false;
                    return result;
                }
                row.rawKey = keyColumn.bytes;
                row.rawKeyAvailable = keyColumn.complete;
                if (!keyColumn.complete) {
                    row.status = MetaDataDecodeStatus::PayloadLimitExceeded;
                    result.rows.push_back(row);
                    result.resourceLimitExceeded = true;
                    return result;
                }
                if (row.keyStorageClass != MetaDataStorageClass::Text) {
                    keyIssue = MetaDataDecodeStatus::UnsupportedStorageClass;
                } else if (!isValidUtf8(row.rawKey)) {
                    keyIssue = MetaDataDecodeStatus::InvalidUtf8;
                } else {
                    row.key = utf8BytesToPkString(row.rawKey);
                    row.keyAvailable = true;
                }
            } else {
                keyIssue = MetaDataDecodeStatus::UnsupportedStorageClass;
            }

            std::optional<MetaDataDecodeStatus> valueIssue;
            VariantDecodeResult decoded;
            if (row.valueStorageClass == MetaDataStorageClass::Null) {
                row.rawPayloadAvailable = true;
                valueIssue = MetaDataDecodeStatus::UnsupportedStorageClass;
            } else if (isByteStorageClass(row.valueStorageClass)) {
                const std::uint64_t ownerBytesRemaining =
                    kMaximumMetadataPayloadBytesPerOwner - retainedPayloadBytes;
                const std::uint64_t retainLimit =
                    std::min(kMaximumMetadataPayloadBytes, ownerBytesRemaining);
                const BoundedMetadataColumn payloadColumn = readBoundedMetadataColumn(
                    nativeDatabase, row.rowId, "value", retainLimit);
                if (!payloadColumn.succeeded) {
                    qWarning() << "Could not incrementally read metadata value" << row.rowId;
                    result.querySucceeded = false;
                    return result;
                }
                row.rawPayload = payloadColumn.bytes;
                row.rawPayloadAvailable = payloadColumn.complete;
                if (!payloadColumn.complete) {
                    valueIssue = MetaDataDecodeStatus::PayloadLimitExceeded;
                } else {
                    retainedPayloadBytes += payloadColumn.byteCount;
                    if (row.valueStorageClass != MetaDataStorageClass::Text) {
                        valueIssue = MetaDataDecodeStatus::UnsupportedStorageClass;
                    } else if (!isValidUtf8(row.rawPayload)) {
                        valueIssue = MetaDataDecodeStatus::InvalidUtf8;
                    } else {
                        decoded = deserializeVariant(utf8BytesToPkString(row.rawPayload));
                        if (!decoded.decoded) {
                            valueIssue = decoded.status;
                        }
                    }
                }
            } else {
                valueIssue = MetaDataDecodeStatus::UnsupportedStorageClass;
            }

            if (keyIssue) {
                row.status = *keyIssue;
            } else if (valueIssue) {
                row.status = *valueIssue;
            } else {
                row.decoded = true;
                row.value = decoded.value;
            }

            result.rows.push_back(row);
            const MetaDataReadRow &storedRow = result.rows.back();
            if (!storedRow.keyAvailable) {
                continue;
            }
            if (storedRow.decoded) {
                if (!result.values.contains(storedRow.key)) {
                    result.values.insert(storedRow.key, storedRow.value);
                }
                continue;
            }

            MetaDataDecodeIssue issue;
            issue.status = storedRow.status;
            issue.rawPayloadAvailable =
                storedRow.valueStorageClass == MetaDataStorageClass::Text &&
                storedRow.rawPayloadAvailable && isValidUtf8(storedRow.rawPayload);
            if (issue.rawPayloadAvailable) {
                issue.rawPayload = utf8BytesToPkString(storedRow.rawPayload);
            }
            if (!result.undecodable.contains(storedRow.key)) {
                result.undecodable.insert(storedRow.key, issue);
            }
        }

        if (rowsInPage < kMetadataReadPageRows) {
            return result;
        }
        firstPage = false;
    }
}

PkMap<PkString, PkVariant> KisResourceCacheDb::metaDataForId(int id,
                                                             const PkString &tableName)
{
    const MetaDataReadResult result = metaDataReadResultForId(id, tableName);
    for (auto iter = result.undecodable.cbegin(); iter != result.undecodable.cend(); ++iter) {
        qWarning() << "Could not decode metadata value for key" << iter.key()
                   << "status" << static_cast<int>(iter.value().status);
    }
    return result.values;
}

bool KisResourceCacheDb::updateMetaDataForId(const PkMap<PkString, PkVariant> map, int id, const PkString &tableName)
{
    PkMap<PkString, PkString> serializedUpdates;
    for (auto iter = map.cbegin(); iter != map.cend(); ++iter) {
        const PkVariant value = iter.value();
        // An invalid PkVariant is the replace-all API's "no value" marker.
        // A valid, typed null is persisted data and has a canonical R31 wire
        // representation; conflating the two silently deletes typed nulls.
        if (!value.isValid()) continue;
        const PkString encoded = serializeVariant(value);
        if (encoded.isEmpty()) {
            qWarning() << "Unsupported metadata value type for key" << iter.key()
                       << value.typeName();
            return false;
        }
        serializedUpdates.insert(iter.key(), encoded);
    }

    MetadataSavepoint transaction;
    if (!transaction.begin()) {
        return false;
    }

    const auto rollbackAndFail = [&transaction]() {
        if (!transaction.rollback()) {
            qWarning() << "Could not fully rollback metadata update savepoint";
        }
        return false;
    };

    const MetaDataReadResult current = metaDataReadResultForId(id, tableName);
    if (!current.querySucceeded || current.resourceLimitExceeded) {
        return rollbackAndFail();
    }

    std::vector<long long> rowIdsToDelete;
    rowIdsToDelete.reserve(current.rows.size());
    for (const MetaDataReadRow &row : current.rows) {
        if (row.decoded) {
            if (row.value.isValid() && row.value.isNull() &&
                !serializedUpdates.contains(row.key)) {
                // Callers using the compatibility map can omit a typed null
                // when changing an unrelated key. Preserve it unless this
                // update has an explicit replacement for that TEXT key.
                continue;
            }
            rowIdsToDelete.push_back(row.rowId);
        } else if (row.keyAvailable && serializedUpdates.contains(row.key)) {
            // Only a valid UTF-8 TEXT key is addressable through the PkString
            // update API. Delete by physical row id so a BLOB key with the
            // same bytes, a NULL key, or invalid UTF-8 remains byte-identical.
            rowIdsToDelete.push_back(row.rowId);
        }
    }

    {
        PkSqlQuery q;
        if (!q.prepare("DELETE FROM metadata\n"
                       "WHERE  rowid = :row_id\n"
                       "AND    foreign_id = :id\n"
                       "AND    table_name = :table\n")) {
            qWarning() << "Could not prepare delete metadata query" << q.lastError();
            return rollbackAndFail();
        }

        for (long long rowId : rowIdsToDelete) {
            q.bindValue(":row_id", rowId);
            q.bindValue(":id", id);
            q.bindValue(":table", tableName);
            if (!q.exec()) {
                qWarning() << "Could not execute delete metadata query" << q.lastError();
                return rollbackAndFail();
            }
        }
    }

    const bool added = addMetaDataForId(map, id, tableName);
    if (!added) {
        return rollbackAndFail();
    }
    return transaction.release();
}

bool KisResourceCacheDb::addMetaDataForId(const PkMap<PkString, PkVariant> map, int id, const PkString &tableName)
{

    PkSqlQuery q;
    if (!q.prepare("INSERT INTO metadata\n"
                   "(foreign_id, table_name, key, value)\n"
                   "VALUES\n"
                   "(:id, :table, :key, :value)")) {
        qWarning() << "Could not create insert metadata query" << q.lastError();
        return false;
    }

    PkMap<PkString, PkVariant>::const_iterator iter = map.cbegin();
    while (iter != map.cend()) {
        q.bindValue(":id", id);
        q.bindValue(":table", tableName);
        q.bindValue(":key", iter.key());

        PkVariant v = iter.value();
        if (v.isValid()) {
            const PkString encoded = serializeVariant(v);
            if (encoded.isEmpty()) {
                qWarning() << "Unsupported metadata value type for key" << iter.key()
                           << v.typeName();
                return false;
            }
            q.bindValue(":value", encoded);

            if (!q.exec()) {
                qWarning() << "Could not insert metadata" << q.lastError();
                return false;
            }
        }
        ++iter;
    }
    return true;
}

bool KisResourceCacheDb::removeOrphanedMetaData()
{
    auto deleteMetadataForType = [] (const PkString &tableName) {
        KisSqlQueryLoader loader("inline://delete_orphaned_records (" + tableName + ")",
                                 PkString("DELETE FROM metadata\n"
                                         "WHERE  foreign_id NOT IN (SELECT id FROM %1)\n"
                                         "AND    table_name = \"%1\"\n")
                                         .arg(tableName));
        loader.exec();
        return loader.query().numRowsAffected();
    };

    try {
        KisDatabaseTransactionLock transactionLock(
            PkSqlDatabase::database(PkSqlDatabase::defaultConnection, false));
        if (!transactionLock.transactionStarted()) {
            return false;
        }

        const int removedResources = deleteMetadataForType(METADATA_RESOURCES);
        const int removedStorages = deleteMetadataForType(METADATA_STORAGES);

        if (!transactionLock.commit()) {
            return false;
        }

        const auto reportRemoved = [](const PkString &tableName, int removed) {
            if (removed > 0) {
                qWarning().noquote().nospace()
                    << "WARNING: orphaned metadata records were found for "
                    << tableName << "!";
                qWarning().noquote().nospace()
                    << "         Num records removed: " << removed;
            }
        };
        reportRemoved(METADATA_RESOURCES, removedResources);
        reportRemoved(METADATA_STORAGES, removedStorages);

    } catch (const KisSqlQueryLoader::SQLException &e) {
        qWarning().noquote() << "ERROR: failed to execute query:" << e.message;
        qWarning().noquote() << "       file:" << e.filePath;
        qWarning().noquote() << "       statement:" << e.statementIndex;
        qWarning().noquote() << "       error:" << e.sqlError.text();

        return false;
    }

    return true;
}
