#include "data_stream_case.h"

#include "PkDataStream.h"
#include "PkTest.h"

#include "pk_binder_data_stream_case.inc"

#include <cstring>
#include <string>
#include <vector>

namespace {

PkByteArray fromHex(const char *hex)
{
    std::vector<unsigned char> bytes;
    for (const char *p = hex; p[0] != '\0'; p += 2) {
        const auto nibble = [](char c) -> unsigned char {
            return c >= '0' && c <= '9' ? static_cast<unsigned char>(c - '0')
                 : c >= 'a' && c <= 'f' ? static_cast<unsigned char>(c - 'a' + 10)
                 : static_cast<unsigned char>(c - 'A' + 10);
        };
        bytes.push_back(static_cast<unsigned char>((nibble(p[0]) << 4) | nibble(p[1])));
    }
    return PkByteArray(bytes);
}

struct Fixture
{
    const char *name;
    const char *hex;
    PkVariant expected;
};

std::vector<Fixture> fixtures46()
{
    PkVariantMap map{{PkString("a"), PkVariant(3)},
                     {PkString::PkFromUtf8("\xCE\xB2", 2), PkVariant(false)}};
    PkVariantHash hash{{PkString("a"), PkVariant(3)}};
    return {
        {"invalid", "0000000001ffffffff", PkVariant()},
        {"bool", "000000010001", PkVariant(true)},
        {"int", "0000000200ffed2979", PkVariant(-1234567)},
        {"uint", "0000000300f1234567", PkVariant(0xf1234567u)},
        {"longlong", "0000000400fefdfcfbfaf9f8f8", PkVariant(-0x102030405060708LL)},
        {"ulonglong", "0000000500f102030405060708", PkVariant(0xf102030405060708ULL)},
        {"double", "000000060040934a0000000000", PkVariant(1234.5)},
        {"float", "00000087004028800000000000", PkVariant(12.25f)},
        {"string", "0000000a00000000060041d83cdfa8", PkVariant(PkString::PkFromUtf8("A\xF0\x9F\x8E\xA8", 5))},
        {"bytearray", "0000000c000000000341005a", PkVariant(PkByteArray("A\0Z", 3))},
        {"stringlist", "0000000b00000000020000000200610000000203b2", PkVariant(PkStringList{PkString("a"), PkString::PkFromUtf8("\xCE\xB2", 2)})},
        {"list", "0000000900000000020000000200000000070000000a0000000004006800e9", PkVariant(PkVariantList{PkVariant(7), PkVariant(PkString::PkFromUtf8("h\xC3\xA9", 3))})},
        {"map", "0000000800000000020000000203b2000000010000000000020061000000020000000003", PkVariant(map)},
        {"hash", "0000001c0000000001000000020061000000020000000003", PkVariant(hash)},
        {"date", "0000000e0000258ad2", PkVariant(PkDate(2024, 2, 29))},
        {"time", "0000000f0002b32c95", PkVariant(PkTime(12, 34, 56, 789))},
        {"datetime", "000000100000258ad202b32c95ff", PkVariant(PkDateTime(PkDate(2024, 2, 29), PkTime(12, 34, 56, 789)))},
        {"rect", "0000001300fffffffe000000030000000100000007", PkVariant(PkRect(-2, 3, 4, 5))},
        {"rectf", "0000001400c004000000000000400a00000000000040120000000000004017000000000000", PkVariant(PkRectF(-2.5, 3.25, 4.5, 5.75))},
        {"size", "0000001500fffffffe00000003", PkVariant(PkSize(-2, 3))},
        {"sizef", "0000001600c004000000000000400a000000000000", PkVariant(PkSizeF(-2.5, 3.25))},
        {"line", "0000001700fffffffe000000030000000400000005", PkVariant(PkLine(-2, 3, 4, 5))},
        {"linef", "0000001800c004000000000000400a00000000000040120000000000004017000000000000", PkVariant(PkLineF(-2.5, 3.25, 4.5, 5.75))},
        {"point", "0000001900fffffffe00000003", PkVariant(PkPoint(-2, 3))},
        {"pointf", "0000001a00c004000000000000400a000000000000", PkVariant(PkPointF(-2.5, 3.25))},
    };
}

std::vector<Fixture> fixtures515()
{
    std::vector<Fixture> fixtures = fixtures46();
    for (Fixture &fixture : fixtures) {
        if (std::strcmp(fixture.name, "invalid") == 0) fixture.hex = "0000000001";
        if (std::strcmp(fixture.name, "float") == 0) fixture.hex = "00000026004028800000000000";
        if (std::strcmp(fixture.name, "date") == 0) fixture.hex = "0000000e000000000000258ad2";
        if (std::strcmp(fixture.name, "datetime") == 0) fixture.hex = "00000010000000000000258ad202b32c9500";
    }
    return fixtures;
}

void verifyReads(const std::vector<Fixture> &fixtures, PkDataStream::Version version)
{
    for (const Fixture &fixture : fixtures) {
        PkDataStream stream(fromHex(fixture.hex));
        stream.setVersion(version);
        PkVariant actual(999);
        stream >> actual;
        PK_COMPARE(stream.status(), PkDataStream::Ok);
        PK_VERIFY2(actual == fixture.expected, fixture.name);
    }
}

void verifyWrites(const std::vector<Fixture> &fixtures, PkDataStream::Version version)
{
    for (const Fixture &fixture : fixtures) {
        PkByteArray actual;
        PkDataStream stream(&actual, PkStream::WriteOnly);
        stream.setVersion(version);
        stream << fixture.expected;
        PK_COMPARE(stream.status(), PkDataStream::Ok);
        PK_VERIFY2(actual == fromHex(fixture.hex), fixture.name);
    }
}

class MemoryStream : public PkStream
{
public:
    explicit MemoryStream(std::string bytes = {}) : m_bytes(std::move(bytes)) {}
    pk_int64 size() const override { return static_cast<pk_int64>(m_bytes.size()); }
    const std::string &bytes() const { return m_bytes; }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 remaining = size() - pos();
        if (remaining <= 0) return 0;
        const pk_int64 count = remaining < maxSize ? remaining : maxSize;
        std::memcpy(data, m_bytes.data() + pos(), static_cast<std::size_t>(count));
        return count;
    }

    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        const std::size_t offset = static_cast<std::size_t>(pos());
        const std::size_t required = offset + static_cast<std::size_t>(maxSize);
        if (m_bytes.size() < required) m_bytes.resize(required);
        std::memcpy(m_bytes.data() + offset, data, static_cast<std::size_t>(maxSize));
        return maxSize;
    }

private:
    std::string m_bytes;
};

} // namespace

void DataStreamCase::defaultsAndStatus()
{
    PkDataStream stream;
    PK_COMPARE(stream.version(), PkDataStream::Qt_5_15);
    PK_COMPARE(stream.byteOrder(), PkDataStream::BigEndian);
    PK_COMPARE(stream.floatingPointPrecision(), PkDataStream::DoublePrecision);
    PK_COMPARE(stream.status(), PkDataStream::Ok);
    stream.setStatus(PkDataStream::ReadCorruptData);
    PK_COMPARE(stream.status(), PkDataStream::ReadCorruptData);
    stream.resetStatus();
    PK_COMPARE(stream.status(), PkDataStream::Ok);
}

void DataStreamCase::readsQt46BigEndianFixtures()
{
    verifyReads(fixtures46(), PkDataStream::Qt_4_6);
}

void DataStreamCase::readsQt515BigEndianFixtures()
{
    verifyReads(fixtures515(), PkDataStream::Qt_5_15);
}

void DataStreamCase::writesQt46BigEndianFixtures()
{
    verifyWrites(fixtures46(), PkDataStream::Qt_4_6);
}

void DataStreamCase::writesQt515BigEndianFixtures()
{
    verifyWrites(fixtures515(), PkDataStream::Qt_5_15);
}

void DataStreamCase::littleEndianScalarRoundTrip()
{
    PkByteArray bytes;
    PkDataStream out(&bytes, PkStream::WriteOnly);
    out.setByteOrder(PkDataStream::LittleEndian);
    const long long signedWide = 0x0102030405060708LL;
    const unsigned long long unsignedWide = 0xf102030405060708ULL;
    out << static_cast<std::uint32_t>(0x01020304u) << 1.0 << signedWide << unsignedWide;
    PK_VERIFY(bytes == fromHex("04030201000000000000f03f080706050403020108070605040302f1"));

    PkDataStream in(bytes);
    in.setByteOrder(PkDataStream::LittleEndian);
    std::uint32_t integer = 0;
    double real = 0.0;
    long long readSignedWide = 0;
    unsigned long long readUnsignedWide = 0;
    in >> integer >> real >> readSignedWide >> readUnsignedWide;
    PK_COMPARE(integer, 0x01020304u);
    PK_COMPARE(real, 1.0);
    PK_COMPARE(readSignedWide, signedWide);
    PK_COMPARE(readUnsignedWide, unsignedWide);
    PK_COMPARE(in.status(), PkDataStream::Ok);
}

void DataStreamCase::shortReadIsStickyAndZeroesTarget()
{
    PkDataStream stream(fromHex("0102"));
    std::uint32_t first = 99;
    std::uint16_t second = 88;
    stream >> first;
    PK_COMPARE(first, 0u);
    PK_COMPARE(stream.status(), PkDataStream::ReadPastEnd);
    stream >> second;
    PK_COMPARE(second, static_cast<std::uint16_t>(0));
    PK_COMPARE(stream.status(), PkDataStream::ReadPastEnd);
}

void DataStreamCase::rejectsUnknownAndUserTypes()
{
    {
        PkDataStream stream(fromHex("000000070000"));
        PkVariant value(42);
        stream >> value;
        PK_COMPARE(stream.status(), PkDataStream::ReadCorruptData);
        PK_VERIFY(!value.isValid());
    }
    {
        PkDataStream stream(fromHex("00000400000000000d576972655573657254797065000000002a"));
        PkVariant value(42);
        stream >> value;
        PK_COMPARE(stream.status(), PkDataStream::ReadCorruptData);
        PK_VERIFY(!value.isValid());
    }
    {
        PkVariant user = PkVariant::fromValue(std::vector<int>{1, 2});
        PkByteArray bytes;
        PkDataStream stream(&bytes, PkStream::WriteOnly);
        stream << user;
        PK_COMPARE(stream.status(), PkDataStream::WriteFailed);
        PK_VERIFY(bytes.isEmpty());
    }
}

void DataStreamCase::readsAndWritesThroughPkStream()
{
    MemoryStream device;
    device.open(PkStream::ReadWrite);
    PkDataStream writer(&device);
    writer << PkVariant(42);
    PK_COMPARE(writer.status(), PkDataStream::Ok);
    PK_VERIFY(device.bytes() == std::string("\0\0\0\2\0\0\0\0*", 9));

    device.seek(0);
    PkDataStream reader(&device);
    PkVariant value;
    reader >> value;
    PK_COMPARE(reader.status(), PkDataStream::Ok);
    PK_COMPARE(value.toInt(), 42);
}

PK_TEST_MAIN(DataStreamCase)
