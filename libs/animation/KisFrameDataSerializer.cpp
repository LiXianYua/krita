/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisFrameDataSerializer.h"

#include <PkDataStream.h>
#include <PkElapsedTimer.h>
#include <PkFileStream.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "tiles3/swap/kis_lzf_compression.h"

namespace {

namespace fs = std::filesystem;

class TemporaryFrameDirectory
{
public:
    explicit TemporaryFrameDirectory(const PkString &requestedRoot)
    {
        std::error_code error;
        const fs::path requested = fs::u8path(requestedRoot.PkToUtf8());
        if (!requestedRoot.isEmpty() && fs::is_directory(requested, error)) {
            m_path = createUniqueDirectory(requested);
        }

        if (m_path.empty()) {
            error.clear();
            m_path = createUniqueDirectory(fs::temp_directory_path(error));
        }
    }

    ~TemporaryFrameDirectory()
    {
        if (!m_path.empty()) {
            std::error_code error;
            fs::remove_all(m_path, error);
        }
    }

    TemporaryFrameDirectory(const TemporaryFrameDirectory &) = delete;
    TemporaryFrameDirectory &operator=(const TemporaryFrameDirectory &) = delete;

    const fs::path &path() const { return m_path; }

private:
    static fs::path createUniqueDirectory(const fs::path &root)
    {
        if (root.empty()) return {};

        static std::atomic<std::uint64_t> sequence {0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        constexpr char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

        for (int attempt = 0; attempt < 100; ++attempt) {
            std::uint64_t uniqueValue = static_cast<std::uint64_t>(timestamp) ^ sequence.fetch_add(1);
            std::string suffix(6, '0');
            for (char &character : suffix) {
                character = alphabet[uniqueValue % (sizeof(alphabet) - 1)];
                uniqueValue /= sizeof(alphabet) - 1;
            }
            const fs::path candidate = root / ("KritaFrameCache" + suffix);
            std::error_code error;
            if (fs::create_directory(candidate, error)) {
                const fs::path absolutePath = fs::absolute(candidate, error);
                return error ? candidate : absolutePath;
            }
        }
        return {};
    }

    fs::path m_path;
};

PkString pathString(const fs::path &path)
{
    const std::string utf8 = path.u8string();
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

} // namespace

struct KRITAANIMATION_NO_EXPORT KisFrameDataSerializer::Private
{
    explicit Private(const PkString &frameCachePath)
        : framesDir(frameCachePath)
    {
    }

    std::string subfolderNameForFrame(int frameId) const
    {
        return std::to_string(frameId & 0xff00);
    }

    std::string fileNameForFrame(int frameId) const
    {
        return "frame_" + std::to_string(frameId);
    }

    fs::path filePathForFrame(int frameId) const
    {
        if (framesDir.path().empty()) return {};
        return framesDir.path() / subfolderNameForFrame(frameId) / fileNameForFrame(frameId);
    }

    int generateFrameId()
    {
        // TODO: handle wrapping and range compression
        return nextFrameId++;
    }

    std::uint8_t *getCompressionBuffer(int size)
    {
        if (static_cast<int>(compressionBuffer.size()) < size) {
            compressionBuffer.resize(static_cast<std::size_t>(size));
        }
        return compressionBuffer.data();
    }

    TemporaryFrameDirectory framesDir;
    int nextFrameId = 0;
    std::vector<std::uint8_t> compressionBuffer;
};

KisFrameDataSerializer::KisFrameDataSerializer()
    : KisFrameDataSerializer(PkString())
{
}

KisFrameDataSerializer::KisFrameDataSerializer(const PkString &frameCachePath)
    : m_d(new Private(frameCachePath))
{
}

KisFrameDataSerializer::~KisFrameDataSerializer()
{
}

int KisFrameDataSerializer::saveFrame(const KisFrameDataSerializer::Frame &frame)
{
    KisLzfCompression compression;

    const int frameId = m_d->generateFrameId();

    const fs::path frameFilePath = m_d->filePathForFrame(frameId);
    if (frameFilePath.empty()) return frameId;
    std::error_code error;
    fs::create_directories(frameFilePath.parent_path(), error);
    if (error) return frameId;

    if (fs::exists(frameFilePath, error)) {
        std::cerr << "WARNING: overwriting existing frame file! " << frameFilePath << '\n';
        forgetFrame(frameId);
    }

    PkFileStream file(pathString(frameFilePath));
    if (!file.open(PkStream::WriteOnly | PkStream::Truncate)) {
        return frameId;
    }

    PkDataStream stream(&file);
    stream.setByteOrder(PkDataStream::BigEndian);
    stream.setVersion(PkDataStream::Qt_5_15);
    stream << static_cast<std::int32_t>(frameId);
    stream << static_cast<std::int32_t>(frame.pixelSize);
    stream << static_cast<std::int32_t>(frame.frameTiles.size());

    for (int i = 0; i < int(frame.frameTiles.size()); i++) {
        const FrameTile &tile = frame.frameTiles[i];

        stream << static_cast<std::int32_t>(tile.col);
        stream << static_cast<std::int32_t>(tile.row);
        stream << static_cast<std::int32_t>(tile.rect.left());
        stream << static_cast<std::int32_t>(tile.rect.top());
        stream << static_cast<std::int32_t>(tile.rect.right());
        stream << static_cast<std::int32_t>(tile.rect.bottom());

        const int frameByteSize = frame.pixelSize * tile.rect.width() * tile.rect.height();
        const int maxBufferSize = compression.outputBufferSize(frameByteSize);
        std::uint8_t *buffer = m_d->getCompressionBuffer(maxBufferSize);

        const int compressedSize =
            compression.compress(tile.data.data(), frameByteSize, buffer, maxBufferSize);

        //ENTER_FUNCTION() << ppVar(compressedSize) << ppVar(frameByteSize);

        const bool isCompressed = compressedSize < frameByteSize;
        stream << isCompressed;

        if (isCompressed) {
            stream << static_cast<std::int32_t>(compressedSize);
            if (stream.status() == PkDataStream::Ok &&
                file.write(reinterpret_cast<const char *>(buffer), compressedSize) != compressedSize) {
                stream.setStatus(PkDataStream::WriteFailed);
            }
        } else {
            stream << static_cast<std::int32_t>(frameByteSize);
            if (stream.status() == PkDataStream::Ok &&
                file.write(reinterpret_cast<const char *>(tile.data.data()), frameByteSize) != frameByteSize) {
                stream.setStatus(PkDataStream::WriteFailed);
            }
        }

        if (stream.status() != PkDataStream::Ok) break;
    }

    file.close();

    return frameId;
}

KisFrameDataSerializer::Frame KisFrameDataSerializer::loadFrame(int frameId, KisTextureTileInfoPoolSP pool)
{
    KisLzfCompression compression;

    PkElapsedTimer loadingTime;
    loadingTime.start();

    int loadedFrameId = -1;
    KisFrameDataSerializer::Frame frame;

    std::int64_t compressionTime = 0;

    const fs::path framePath = m_d->filePathForFrame(frameId);

    KIS_SAFE_ASSERT_RECOVER_NOOP(fs::exists(framePath));
    PkFileStream file(pathString(framePath));
    if (!file.open(PkStream::ReadOnly)) return frame;

    PkDataStream stream(&file);
    stream.setByteOrder(PkDataStream::BigEndian);
    stream.setVersion(PkDataStream::Qt_5_15);

    std::int32_t loadedFrameIdWire = -1;
    std::int32_t pixelSizeWire = 0;
    std::int32_t numTiles = 0;

    stream >> loadedFrameIdWire;
    stream >> pixelSizeWire;
    stream >> numTiles;
    if (stream.status() != PkDataStream::Ok) return KisFrameDataSerializer::Frame();
    loadedFrameId = loadedFrameIdWire;
    frame.pixelSize = pixelSizeWire;
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(loadedFrameId == frameId, KisFrameDataSerializer::Frame());



    for (int i = 0; i < numTiles; i++) {
        FrameTile tile(pool);
        std::int32_t col = 0;
        std::int32_t row = 0;
        std::int32_t left = 0;
        std::int32_t top = 0;
        std::int32_t right = -1;
        std::int32_t bottom = -1;
        stream >> col >> row >> left >> top >> right >> bottom;
        if (stream.status() != PkDataStream::Ok) return KisFrameDataSerializer::Frame();
        tile.col = col;
        tile.row = row;
        tile.rect = PkRect(left, top, right - left + 1, bottom - top + 1);

        const int frameByteSize = frame.pixelSize * tile.rect.width() * tile.rect.height();
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(frameByteSize <= pool->chunkSize(frame.pixelSize),
                                             KisFrameDataSerializer::Frame());

        bool isCompressed = false;
        std::int32_t inputSize = -1;

        stream >> isCompressed;
        stream >> inputSize;
        if (stream.status() != PkDataStream::Ok) return KisFrameDataSerializer::Frame();

        if (isCompressed) {
            const int maxBufferSize = compression.outputBufferSize(inputSize);
            std::uint8_t *buffer = m_d->getCompressionBuffer(maxBufferSize);
            if (file.read(reinterpret_cast<char *>(buffer), inputSize) != inputSize) {
                return KisFrameDataSerializer::Frame();
            }

            tile.data.allocate(frame.pixelSize);

            PkElapsedTimer compTime;
            compTime.start();

            const int decompressedSize =
                compression.decompress(buffer, inputSize, tile.data.data(), frameByteSize);

            compressionTime += compTime.nsecsElapsed();

            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(frameByteSize == decompressedSize,
                                                 KisFrameDataSerializer::Frame());

        } else {
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(frameByteSize == inputSize,
                                                 KisFrameDataSerializer::Frame());

            tile.data.allocate(frame.pixelSize);
            if (file.read(reinterpret_cast<char *>(tile.data.data()), inputSize) != inputSize) {
                return KisFrameDataSerializer::Frame();
            }
        }

        frame.frameTiles.push_back(std::move(tile));
    }

    (void)compressionTime;

    file.close();

    return frame;
}

void KisFrameDataSerializer::moveFrame(int srcFrameId, int dstFrameId)
{
    const fs::path srcFramePath = m_d->filePathForFrame(srcFrameId);
    const fs::path dstFramePath = m_d->filePathForFrame(dstFrameId);
    KIS_SAFE_ASSERT_RECOVER_RETURN(fs::exists(srcFramePath));

    std::error_code error;
    fs::create_directories(dstFramePath.parent_path(), error);
    KIS_SAFE_ASSERT_RECOVER(!fs::exists(dstFramePath)) {
        fs::remove(dstFramePath, error);
    }

    error.clear();
    fs::rename(srcFramePath, dstFramePath, error);
}

bool KisFrameDataSerializer::hasFrame(int frameId) const
{
    return fs::exists(m_d->filePathForFrame(frameId));
}

void KisFrameDataSerializer::forgetFrame(int frameId)
{
    std::error_code error;
    fs::remove(m_d->filePathForFrame(frameId), error);
}

boost::optional<double> KisFrameDataSerializer::estimateFrameUniqueness(const KisFrameDataSerializer::Frame &lhs, const KisFrameDataSerializer::Frame &rhs, double portion)
{
    if (lhs.pixelSize != rhs.pixelSize) return boost::none;
    if (lhs.frameTiles.size() != rhs.frameTiles.size()) return boost::none;

    const int pixelSize = lhs.pixelSize;
    int numSampledPixels = 0;
    int numUniquePixels = 0;
    const int sampleStep = portion > 0.0 ? std::max(1, static_cast<int>(std::lround(1.0 / portion))) : 0;

    for (int i = 0; i < int(lhs.frameTiles.size()); i++) {
        const FrameTile &lhsTile = lhs.frameTiles[i];
        const FrameTile &rhsTile = rhs.frameTiles[i];

        if (lhsTile.col != rhsTile.col ||
            lhsTile.row != rhsTile.row ||
            lhsTile.rect != rhsTile.rect) {

            return boost::none;
        }

        if (sampleStep > 0) {
            const int numPixels = lhsTile.rect.width() * lhsTile.rect.height();

            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(lhsTile.data.data(), boost::none);
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(rhsTile.data.data(), boost::none);

            for (int j = 0; j < numPixels; j += sampleStep) {
                std::uint8_t *lhsDataPtr = lhsTile.data.data() + j * pixelSize;
                std::uint8_t *rhsDataPtr = rhsTile.data.data() + j * pixelSize;

                if (std::memcmp(lhsDataPtr, rhsDataPtr, pixelSize) != 0) {
                    numUniquePixels++;
                }
                numSampledPixels++;
            }
        }
    }

    return numSampledPixels > 0 ? double(numUniquePixels) / numSampledPixels : 1.0;
}

template <template <typename U> class OpPolicy, typename T>
bool processData(T *dst, const T *src, int numUnits)
{
    OpPolicy<T> op;

    bool unitsAreSame = true;

    for (int j = 0; j < numUnits; j++) {
        *dst = op(*dst, *src);

        if (*dst != 0) {
            unitsAreSame = false;
        }

        src++;
        dst++;
    }
    return unitsAreSame;
}


template<template <typename U> class OpPolicy>
bool KisFrameDataSerializer::processFrames(KisFrameDataSerializer::Frame &dst, const KisFrameDataSerializer::Frame &src)
{
    bool framesAreSame = true;

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(estimateFrameUniqueness(src, dst, 0.0), false);

    for (int i = 0; i < int(src.frameTiles.size()); i++) {
        const FrameTile &srcTile = src.frameTiles[i];
        FrameTile &dstTile = dst.frameTiles[i];

        const int numBytes = srcTile.rect.width() * srcTile.rect.height() * src.pixelSize;
        const int numQWords = numBytes / 8;

        const std::uint64_t *srcDataPtr = reinterpret_cast<const std::uint64_t*>(srcTile.data.data());
        std::uint64_t *dstDataPtr = reinterpret_cast<std::uint64_t*>(dstTile.data.data());

        framesAreSame &= processData<OpPolicy>(dstDataPtr, srcDataPtr, numQWords);


        const int tailBytes = numBytes % 8;
        const std::uint8_t *srcTailDataPtr = srcTile.data.data() + numBytes - tailBytes;
        std::uint8_t *dstTailDataPtr = dstTile.data.data() + numBytes - tailBytes;

        framesAreSame &= processData<OpPolicy>(dstTailDataPtr, srcTailDataPtr, tailBytes);
    }

    return framesAreSame;
}

bool KisFrameDataSerializer::subtractFrames(KisFrameDataSerializer::Frame &dst, const KisFrameDataSerializer::Frame &src)
{
    return processFrames<std::minus>(dst, src);
}

void KisFrameDataSerializer::addFrames(KisFrameDataSerializer::Frame &dst, const KisFrameDataSerializer::Frame &src)
{
    // TODO: don't spend time on calculation of "framesAreSame" in this case
    (void) processFrames<std::plus>(dst, src);
}
