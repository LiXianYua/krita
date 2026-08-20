#pragma once

#include "PkAuxTypes.h"
#include "PkVariant.h"
#include "../port/PkStream.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>

class PkDataStream
{
public:
    enum ByteOrder { BigEndian, LittleEndian };
    enum Status { Ok, ReadPastEnd, ReadCorruptData, WriteFailed };
    enum Version { Qt_4_6 = 12, Qt_5_15 = 19 };
    enum FloatingPointPrecision { SinglePrecision, DoublePrecision };

    PkDataStream();
    explicit PkDataStream(const PkByteArray &bytes);
    PkDataStream(PkByteArray *bytes, PkStream::OpenMode mode);
    explicit PkDataStream(PkStream *device);

    ByteOrder byteOrder() const;
    void setByteOrder(ByteOrder order);
    Version version() const;
    void setVersion(Version version);
    FloatingPointPrecision floatingPointPrecision() const;
    void setFloatingPointPrecision(FloatingPointPrecision precision);
    Status status() const;
    void setStatus(Status status);
    void resetStatus();

    PkDataStream &operator<<(std::int8_t value);
    PkDataStream &operator<<(std::uint8_t value);
    PkDataStream &operator<<(std::int16_t value);
    PkDataStream &operator<<(std::uint16_t value);
    PkDataStream &operator<<(std::int32_t value);
    PkDataStream &operator<<(std::uint32_t value);
    PkDataStream &operator<<(std::int64_t value);
    PkDataStream &operator<<(std::uint64_t value);
    template<typename T, std::enable_if_t<
        (std::is_same_v<T, long long> && !std::is_same_v<long long, std::int64_t>) ||
        (std::is_same_v<T, unsigned long long> && !std::is_same_v<unsigned long long, std::uint64_t>), int> = 0>
    PkDataStream &operator<<(T value)
    {
        if constexpr (std::is_signed_v<T>) return *this << static_cast<std::int64_t>(value);
        else return *this << static_cast<std::uint64_t>(value);
    }
    PkDataStream &operator<<(float value);
    PkDataStream &operator<<(double value);
    PkDataStream &operator<<(const PkString &value);
    PkDataStream &operator<<(const PkByteArray &value);
    PkDataStream &operator<<(const PkVariant &value);

    PkDataStream &operator>>(std::int8_t &value);
    PkDataStream &operator>>(std::uint8_t &value);
    PkDataStream &operator>>(std::int16_t &value);
    PkDataStream &operator>>(std::uint16_t &value);
    PkDataStream &operator>>(std::int32_t &value);
    PkDataStream &operator>>(std::uint32_t &value);
    PkDataStream &operator>>(std::int64_t &value);
    PkDataStream &operator>>(std::uint64_t &value);
    template<typename T, std::enable_if_t<
        (std::is_same_v<T, long long> && !std::is_same_v<long long, std::int64_t>) ||
        (std::is_same_v<T, unsigned long long> && !std::is_same_v<unsigned long long, std::uint64_t>), int> = 0>
    PkDataStream &operator>>(T &value)
    {
        if constexpr (std::is_signed_v<T>) {
            std::int64_t fixed = 0;
            *this >> fixed;
            value = static_cast<T>(fixed);
        } else {
            std::uint64_t fixed = 0;
            *this >> fixed;
            value = static_cast<T>(fixed);
        }
        return *this;
    }
    PkDataStream &operator>>(float &value);
    PkDataStream &operator>>(double &value);
    PkDataStream &operator>>(PkString &value);
    PkDataStream &operator>>(PkByteArray &value);
    PkDataStream &operator>>(PkVariant &value);

private:
    template<typename T> PkDataStream &writeInteger(T value);
    template<typename T> PkDataStream &readInteger(T &value);

    bool writeRaw(const char *data, std::size_t size);
    bool readRaw(char *data, std::size_t size);
    bool writeVariantPayload(const PkVariant &value);
    bool readVariantPayload(std::uint32_t typeId, PkVariant &value);
    bool writeVariantList(const PkVariantList &values);
    bool readVariantList(PkVariantList &values);
    bool writeStringList(const PkStringList &values);
    bool readStringList(PkStringList &values);
    bool writeVariantMap(const PkVariantMap &values);
    bool readVariantMap(PkVariantMap &values);
    bool writeVariantHash(const PkVariantHash &values);
    bool readVariantHash(PkVariantHash &values);

    const PkByteArray &byteArray() const;
    PkByteArray *writableByteArray();

    PkStream *m_device;
    PkByteArray *m_externalBytes;
    PkByteArray m_bytes;
    std::size_t m_position;
    PkStream::OpenMode m_mode;
    ByteOrder m_byteOrder;
    Version m_version;
    FloatingPointPrecision m_precision;
    Status m_status;
};
