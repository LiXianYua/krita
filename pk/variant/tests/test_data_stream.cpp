#include "data_stream_case.h"

#include "PkDataStream.h"
#include "PkTest.h"

#include "pk_binder_data_stream_case.inc"

#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

static_assert(!std::is_copy_constructible_v<PkDataStream>);
static_assert(!std::is_copy_assignable_v<PkDataStream>);

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

void DataStreamCase::typedNullBuiltinsRoundTrip()
{
    struct NullFixture { PkDataStream::Version version; PkVariant::Type type; const char *hex; };
    const NullFixture fixtures[]{
        {PkDataStream::Qt_4_6, PkVariant::String, "0000000a01ffffffff"},
        {PkDataStream::Qt_4_6, PkVariant::ByteArray, "0000000c01ffffffff"},
        {PkDataStream::Qt_4_6, PkVariant::Date, "0000000e0000000000"},
        {PkDataStream::Qt_4_6, PkVariant::Time, "0000000f00ffffffff"},
        {PkDataStream::Qt_4_6, PkVariant::DateTime, "000000100000000000ffffffffff"},
        {PkDataStream::Qt_5_15, PkVariant::String, "0000000a01ffffffff"},
        {PkDataStream::Qt_5_15, PkVariant::ByteArray, "0000000c01ffffffff"},
        {PkDataStream::Qt_5_15, PkVariant::Date, "0000000e008000000000000000"},
        {PkDataStream::Qt_5_15, PkVariant::Time, "0000000f00ffffffff"},
        {PkDataStream::Qt_5_15, PkVariant::DateTime, "00000010008000000000000000ffffffff00"},
    };
    for (const NullFixture &fixture : fixtures) {
        PkDataStream reader(fromHex(fixture.hex));
        reader.setVersion(fixture.version);
        PkVariant value;
        reader >> value;
        PK_COMPARE(reader.status(), PkDataStream::Ok);
        PK_COMPARE(value.type(), fixture.type);
        PK_VERIFY(value.isValid());
        PK_VERIFY(value.isNull());

        PkByteArray encoded;
        PkDataStream writer(&encoded, PkStream::WriteOnly);
        writer.setVersion(fixture.version);
        writer << value;
        PK_COMPARE(writer.status(), PkDataStream::Ok);
        PK_VERIFY(encoded == fromHex(fixture.hex));
    }
}

void DataStreamCase::dateTimeWireStatesRoundTrip()
{
    struct DateTimeFixture {
        PkDataStream::Version version;
        const char *hex;
        PkVariant::DateTimeSpec spec;
        int offsetSeconds;
        const char *zoneId;
    };
    const DateTimeFixture fixtures[]{
        {PkDataStream::Qt_4_6, "000000100000258ad202b32c95ff", PkVariant::DateTimeSpec::LocalTime, 0, ""},
        {PkDataStream::Qt_4_6, "000000100000258ad202b32c9502", PkVariant::DateTimeSpec::UTC, 0, ""},
        {PkDataStream::Qt_4_6, "000000100000258ad202b32c9503", PkVariant::DateTimeSpec::OffsetFromUTC, 0, ""},
        {PkDataStream::Qt_4_6, "000000100000258ad202b32c9504", PkVariant::DateTimeSpec::TimeZone, 0, ""},
        {PkDataStream::Qt_5_15, "00000010000000000000258ad202b32c9500", PkVariant::DateTimeSpec::LocalTime, 0, ""},
        {PkDataStream::Qt_5_15, "00000010000000000000258ad202b32c9501", PkVariant::DateTimeSpec::UTC, 0, ""},
        {PkDataStream::Qt_5_15, "00000010000000000000258ad202b32c950200004d58", PkVariant::DateTimeSpec::OffsetFromUTC, 19800, ""},
        {PkDataStream::Qt_5_15, "00000010000000000000258ad202b32c9503000000180041007300690061002f004b006f006c006b006100740061", PkVariant::DateTimeSpec::TimeZone, 0, "Asia/Kolkata"},
    };
    for (const DateTimeFixture &fixture : fixtures) {
        PkDataStream reader(fromHex(fixture.hex));
        reader.setVersion(fixture.version);
        PkVariant value;
        reader >> value;
        PK_COMPARE(reader.status(), PkDataStream::Ok);
        PK_COMPARE(value.type(), PkVariant::DateTime);
        PK_COMPARE(value.PkDateTimeSpec(), fixture.spec);
        PK_COMPARE(value.PkDateTimeOffsetSeconds(), fixture.offsetSeconds);
        PK_VERIFY(value.PkDateTimeZoneId() == PkString(fixture.zoneId));

        PkByteArray encoded;
        PkDataStream writer(&encoded, PkStream::WriteOnly);
        writer.setVersion(fixture.version);
        writer << value;
        PK_COMPARE(writer.status(), PkDataStream::Ok);
        PK_VERIFY(encoded == fromHex(fixture.hex));
    }
}

void DataStreamCase::isolatedUtf16CodeUnitsRoundTrip()
{
    const char *hex = "0000000a00000000080041d8000042dc00";
    PkDataStream reader(fromHex(hex));
    PkVariant value;
    reader >> value;
    PK_COMPARE(reader.status(), PkDataStream::Ok);
    PK_VERIFY(value.PkStringCodeUnits() == std::u16string({u'A', 0xd800, u'B', 0xdc00}));
    PK_VERIFY(value.toString().PkToU16() == std::u16string({u'A', 0xd800, u'B', 0xdc00}));

    PkDataStream directReader(fromHex("000000080041d8000042dc00"));
    PkString direct;
    directReader >> direct;
    PK_COMPARE(directReader.status(), PkDataStream::Ok);
    PK_VERIFY(direct.PkToU16() == std::u16string({u'A', 0xd800, u'B', 0xdc00}));

    PkByteArray encoded;
    PkDataStream writer(&encoded, PkStream::WriteOnly);
    writer << value;
    PK_COMPARE(writer.status(), PkDataStream::Ok);
    PK_VERIFY(encoded == fromHex(hex));
}

void DataStreamCase::mutatedStringDataIsAuthoritative()
{
    PkVariant value(PkString("a"));
    *static_cast<PkString *>(value.data()) = PkString("b");

    PK_VERIFY(value == PkVariant(PkString("b")));
    PK_VERIFY(value != PkVariant(PkString("a")));
    PK_VERIFY(value.PkStringCodeUnits() == std::u16string({u'b'}));

    PkByteArray encoded;
    PkDataStream writer(&encoded, PkStream::WriteOnly);
    writer << value;
    PK_COMPARE(writer.status(), PkDataStream::Ok);
    PK_VERIFY(encoded == fromHex("0000000a00000000020062"));
}

void DataStreamCase::multiElementHashRoundTripIsSemantic()
{
    PkVariantHash hash{{PkString("a"), PkVariant(3)},
                       {PkString("b"), PkVariant(PkString("two"))},
                       {PkString("c"), PkVariant(false)}};
    PkByteArray bytes;
    PkDataStream writer(&bytes, PkStream::WriteOnly);
    writer << PkVariant(hash);
    PK_COMPARE(writer.status(), PkDataStream::Ok);

    PkDataStream reader(bytes);
    PkVariant decoded;
    reader >> decoded;
    PK_COMPARE(reader.status(), PkDataStream::Ok);
    PK_VERIFY(decoded.toHash() == hash);
}

void DataStreamCase::userTypeFailureConsumesTypeName()
{
    MemoryStream device(std::string("\0\0\4\0\0\0\0\0\rWireUserType\0\0\0\0*", 26));
    device.open(PkStream::ReadOnly);
    PkDataStream stream(&device);
    PkVariant value(42);
    stream >> value;
    PK_COMPARE(stream.status(), PkDataStream::ReadCorruptData);
    PK_COMPARE(device.pos(), 22LL);

    stream.resetStatus();
    std::int32_t payload = 0;
    stream >> payload;
    PK_COMPARE(payload, 42);
    PK_COMPARE(stream.status(), PkDataStream::Ok);
}

void DataStreamCase::hostileLengthsAreRejectedBeforeAllocation()
{
    MemoryStream stringDevice(std::string("\x7f\xff\xff\xfe", 4));
    stringDevice.open(PkStream::ReadOnly);
    PkDataStream stringStream(&stringDevice);
    stringStream.setAllocationLimit(1024);
    PkString string;
    stringStream >> string;
    PK_COMPARE(stringStream.status(), PkDataStream::ReadCorruptData);
    PK_VERIFY(string.isEmpty());

    PkVariant list;
    // QVariant List framing, then hostile element count.
    MemoryStream variantDevice(std::string("\0\0\0\t\0\x7f\xff\xff\xff", 9));
    variantDevice.open(PkStream::ReadOnly);
    PkDataStream variantStream(&variantDevice);
    variantStream.setAllocationLimit(1024);
    variantStream >> list;
    PK_COMPARE(variantStream.status(), PkDataStream::ReadCorruptData);
    PK_VERIFY(!list.isValid());
}

void DataStreamCase::containerDecodedStorageIsBounded()
{
    constexpr std::size_t objectAllowance = sizeof(PkVariant) + 2u * sizeof(void *);
    constexpr std::size_t oneCodeUnitKeyPayload = 2u + 2u * sizeof(void *);
    const auto encode = [](const PkVariant &value) {
        PkByteArray bytes;
        PkDataStream writer(&bytes, PkStream::WriteOnly);
        writer << value;
        return bytes;
    };
    const auto verifyBoundary = [&](const PkVariant &twoElements,
                                    const PkVariant &threeElements,
                                    std::size_t limit) {
        PkDataStream accepted(encode(twoElements));
        accepted.setAllocationLimit(limit);
        PkVariant acceptedValue;
        accepted >> acceptedValue;
        PK_COMPARE(accepted.status(), PkDataStream::Ok);
        PK_VERIFY(acceptedValue == twoElements);

        PkDataStream rejected(encode(threeElements));
        rejected.setAllocationLimit(limit);
        PkVariant rejectedValue;
        rejected >> rejectedValue;
        PK_COMPARE(rejected.status(), PkDataStream::ReadCorruptData);
        PK_VERIFY(!rejectedValue.isValid());
    };

    verifyBoundary(PkVariant(PkVariantList{PkVariant(), PkVariant()}),
                   PkVariant(PkVariantList{PkVariant(), PkVariant(), PkVariant()}),
                   objectAllowance + 2u * sizeof(PkVariant));
    verifyBoundary(PkVariant(PkStringList{PkString(), PkString()}),
                   PkVariant(PkStringList{PkString(), PkString(), PkString()}),
                   objectAllowance + 2u * sizeof(PkString));
    verifyBoundary(PkVariant(PkVariantMap{{PkString("a"), PkVariant()},
                                          {PkString("b"), PkVariant()}}),
                   PkVariant(PkVariantMap{{PkString("a"), PkVariant()},
                                          {PkString("b"), PkVariant()},
                                          {PkString("c"), PkVariant()}}),
                   objectAllowance
                       + 2u * (sizeof(PkVariantMap::value_type) + 4u * sizeof(void *))
                       + 2u * oneCodeUnitKeyPayload);
    verifyBoundary(PkVariant(PkVariantHash{{PkString("a"), PkVariant()},
                                           {PkString("b"), PkVariant()}}),
                   PkVariant(PkVariantHash{{PkString("a"), PkVariant()},
                                           {PkString("b"), PkVariant()},
                                           {PkString("c"), PkVariant()}}),
                   objectAllowance
                       + 2u * (sizeof(PkVariantHash::value_type) + 4u * sizeof(void *))
                       + 2u * oneCodeUnitKeyPayload);
}

void DataStreamCase::recursiveDecodeBudgetIncludesAssociativeOverheadAndPayload()
{
    const std::string payload(300, 'x');
    const PkVariantList nested{
        PkVariant(PkByteArray(payload.data(), static_cast<int>(payload.size()))),
        PkVariant(PkByteArray(payload.data(), static_cast<int>(payload.size())))
    };
    const PkVariant values[]{
        PkVariant(PkVariantMap{{PkString("unique-map-key"), PkVariant(nested)}}),
        PkVariant(PkVariantHash{{PkString("unique-hash-key"), PkVariant(nested)}})
    };

    for (const PkVariant &expected : values) {
        PkByteArray bytes;
        PkDataStream writer(&bytes, PkStream::WriteOnly);
        writer << expected;
        PK_COMPARE(writer.status(), PkDataStream::Ok);

        // Every individual node and payload is below 512 bytes. Only a single
        // recursive budget that accumulates container/node/bucket/payload
        // ownership rejects the aggregate.
        PkDataStream rejected(bytes);
        rejected.setAllocationLimit(512);
        PkVariant rejectedValue;
        rejected >> rejectedValue;
        PK_COMPARE(rejected.status(), PkDataStream::ReadCorruptData);
        PK_VERIFY(!rejectedValue.isValid());

        PkDataStream accepted(bytes);
        accepted.setAllocationLimit(2048);
        PkVariant acceptedValue;
        accepted >> acceptedValue;
        PK_COMPARE(accepted.status(), PkDataStream::Ok);
        PK_VERIFY(acceptedValue == expected);
    }
}

void DataStreamCase::variantObjectStorageIsBoundedRecursively()
{
    const auto encode = [](const PkVariant &value) {
        PkByteArray bytes;
        PkDataStream writer(&bytes, PkStream::WriteOnly);
        writer << value;
        return bytes;
    };
    const auto verifyBoundary = [&](const PkVariant &expected, std::size_t exactLimit) {
        PkDataStream rejected(encode(expected));
        rejected.setAllocationLimit(exactLimit - 1u);
        PkVariant rejectedValue;
        rejected >> rejectedValue;
        PK_COMPARE(rejected.status(), PkDataStream::ReadCorruptData);
        PK_VERIFY(!rejectedValue.isValid());

        PkDataStream accepted(encode(expected));
        accepted.setAllocationLimit(exactLimit);
        PkVariant acceptedValue;
        accepted >> acceptedValue;
        PK_COMPARE(accepted.status(), PkDataStream::Ok);
        PK_VERIFY(acceptedValue == expected);
    };

    // PkVariant's closed A1 non-POD set is stored out of line by std::any.
    // The codec policy reserves one PkVariant-sized object plus a two-pointer
    // allocator allowance for every such destination object.
    constexpr std::size_t objectAllowance = sizeof(PkVariant) + 2u * sizeof(void *);

    verifyBoundary(PkVariant(PkRectF(1.0, 2.0, 3.0, 4.0)), objectAllowance);
    verifyBoundary(PkVariant(PkVariantList{}), objectAllowance);
    verifyBoundary(PkVariant(PkVariantList{PkVariant(PkVariantList{})}),
                   2u * objectAllowance + sizeof(PkVariant));
}

void DataStreamCase::copyAssignedStringMutationSerializesDestination()
{
    PkVariant source(PkString("source"));
    PkVariant destination(42);
    destination = source;
    *static_cast<PkString *>(destination.data()) = PkString("destination");

    PK_VERIFY(source == PkVariant(PkString("source")));
    PK_VERIFY(destination == PkVariant(PkString("destination")));

    PkByteArray encoded;
    PkDataStream writer(&encoded, PkStream::WriteOnly);
    writer << destination;
    PK_COMPARE(writer.status(), PkDataStream::Ok);
    PK_VERIFY(encoded == fromHex("0000000a000000001600640065007300740069006e006100740069006f006e"));
}

PK_TEST_MAIN(DataStreamCase)
