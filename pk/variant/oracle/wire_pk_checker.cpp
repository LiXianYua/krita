#include "PkDataStream.h"
#include "PkVariant.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

PkByteArray fromHex(const std::string &hex)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    const auto nibble = [](char c) -> std::uint8_t {
        return c >= '0' && c <= '9' ? static_cast<std::uint8_t>(c - '0')
             : c >= 'a' && c <= 'f' ? static_cast<std::uint8_t>(c - 'a' + 10)
             : static_cast<std::uint8_t>(c - 'A' + 10);
    };
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return PkByteArray(bytes);
}

std::string toHex(const PkByteArray &bytes)
{
    static const char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(static_cast<std::size_t>(bytes.size()) * 2);
    for (int i = 0; i < bytes.size(); ++i) {
        const auto byte = static_cast<unsigned char>(bytes.constData()[i]);
        hex.push_back(digits[byte >> 4]);
        hex.push_back(digits[byte & 0x0f]);
    }
    return hex;
}

std::map<std::string, PkVariant> values()
{
    PkVariantMap map{{PkString("a"), PkVariant(3)},
                     {PkString::PkFromUtf8("\xCE\xB2", 2), PkVariant(false)}};
    PkVariantHash hash{{PkString("a"), PkVariant(3)}};
    PkVariantHash multiHash{{PkString("a"), PkVariant(3)},
                            {PkString("b"), PkVariant(PkString("two"))},
                            {PkString("c"), PkVariant(false)}};
    const PkDateTime dateTime(PkDate(2024, 2, 29), PkTime(12, 34, 56, 789));
    return {
        {"invalid", PkVariant()},
        {"bool", PkVariant(true)},
        {"int", PkVariant(-1234567)},
        {"uint", PkVariant(0xf1234567u)},
        {"longlong", PkVariant(-0x102030405060708LL)},
        {"ulonglong", PkVariant(0xf102030405060708ULL)},
        {"double", PkVariant(1234.5)},
        {"float", PkVariant(12.25f)},
        {"string", PkVariant(PkString::PkFromUtf8("A\xF0\x9F\x8E\xA8", 5))},
        {"string_isolated_utf16", PkVariant::PkFromStringCodeUnits({u'A', 0xd800, u'B', 0xdc00})},
        {"string_null", PkVariant::PkTypedNull(PkVariant::String)},
        {"bytearray", PkVariant(PkByteArray("A\0Z", 3))},
        {"bytearray_null", PkVariant::PkTypedNull(PkVariant::ByteArray)},
        {"stringlist", PkVariant(PkStringList{PkString("a"), PkString::PkFromUtf8("\xCE\xB2", 2)})},
        {"list", PkVariant(PkVariantList{PkVariant(7), PkVariant(PkString::PkFromUtf8("h\xC3\xA9", 3))})},
        {"map", PkVariant(map)},
        {"hash", PkVariant(hash)},
        {"hash_multi", PkVariant(multiHash)},
        {"date", PkVariant(PkDate(2024, 2, 29))},
        {"date_null", PkVariant(PkDate())},
        {"date_typed_null", PkVariant::PkTypedNull(PkVariant::Date)},
        {"time", PkVariant(PkTime(12, 34, 56, 789))},
        {"time_null", PkVariant(PkTime())},
        {"time_typed_null", PkVariant::PkTypedNull(PkVariant::Time)},
        {"datetime", PkVariant::PkFromDateTime(dateTime, PkVariant::DateTimeSpec::LocalTime)},
        {"datetime_null", PkVariant(PkDateTime())},
        {"datetime_typed_null", PkVariant::PkTypedNull(PkVariant::DateTime)},
        {"datetime_utc", PkVariant::PkFromDateTime(dateTime, PkVariant::DateTimeSpec::UTC)},
        {"datetime_offset", PkVariant::PkFromDateTime(dateTime, PkVariant::DateTimeSpec::OffsetFromUTC, 19800)},
        {"datetime_timezone", PkVariant::PkFromDateTime(dateTime, PkVariant::DateTimeSpec::TimeZone, 0, PkString("Asia/Kolkata"))},
        {"rect", PkVariant(PkRect(-2, 3, 4, 5))},
        {"rectf", PkVariant(PkRectF(-2.5, 3.25, 4.5, 5.75))},
        {"size", PkVariant(PkSize(-2, 3))},
        {"sizef", PkVariant(PkSizeF(-2.5, 3.25))},
        {"line", PkVariant(PkLine(-2, 3, 4, 5))},
        {"linef", PkVariant(PkLineF(-2.5, 3.25, 4.5, 5.75))},
        {"point", PkVariant(PkPoint(-2, 3))},
        {"pointf", PkVariant(PkPointF(-2.5, 3.25))},
    };
}

PkVariant expectedValue(const std::map<std::string, PkVariant> &expectedValues,
                        const std::string &name, PkDataStream::Version version)
{
    if (version == PkDataStream::Qt_4_6 && name == "datetime_offset") {
        return PkVariant::PkFromDateTime(
            PkDateTime(PkDate(2024, 2, 29), PkTime(12, 34, 56, 789)),
            PkVariant::DateTimeSpec::OffsetFromUTC);
    }
    if (version == PkDataStream::Qt_4_6 && name == "datetime_timezone") {
        return PkVariant::PkFromDateTime(
            PkDateTime(PkDate(2024, 2, 29), PkTime(12, 34, 56, 789)),
            PkVariant::DateTimeSpec::TimeZone);
    }
    return expectedValues.at(name);
}

std::string field(const std::string &line, const std::string &name)
{
    const std::string prefix = name + '=';
    const std::size_t begin = line.find(prefix);
    if (begin == std::string::npos) return {};
    const std::size_t valueBegin = begin + prefix.size();
    const std::size_t end = line.find(' ', valueBegin);
    return line.substr(valueBegin, end == std::string::npos ? end : end - valueBegin);
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 2;

    const auto expectedValues = values();
    std::map<std::string, int> tags;
    int total = 0;
    int mismatch = 0;
    std::string line;
    while (std::getline(input, line)) {
        const std::string versionName = field(line, "version");
        const std::string orderName = field(line, "order");
        const std::string name = field(line, "name");
        const std::string bytesHex = field(line, "bytes");
        const auto version = versionName == "qt_4_6" ? PkDataStream::Qt_4_6 : PkDataStream::Qt_5_15;
        const auto order = orderName == "little" ? PkDataStream::LittleEndian : PkDataStream::BigEndian;

        PkDataStream reader(fromHex(bytesHex));
        reader.setVersion(version);
        reader.setByteOrder(order);
        PkVariant decoded;
        reader >> decoded;
        bool readOk = false;
        if (name == "usertype") {
            readOk = reader.status() == PkDataStream::ReadCorruptData && !decoded.isValid();
        } else {
            const auto it = expectedValues.find(name);
            readOk = it != expectedValues.end() && reader.status() == PkDataStream::Ok
                && decoded == expectedValue(expectedValues, name, version);
        }
        ++total;
        if (!readOk) {
            ++mismatch;
            ++tags["read-" + name + '-' + versionName + '-' + orderName];
        }

        PkByteArray encoded;
        PkDataStream writer(&encoded, PkStream::WriteOnly);
        writer.setVersion(version);
        writer.setByteOrder(order);
        bool writeOk = false;
        if (name == "usertype") {
            writer << PkVariant::fromValue(std::vector<int>{42});
            writeOk = writer.status() == PkDataStream::WriteFailed && encoded.isEmpty();
        } else {
            writer << expectedValue(expectedValues, name, version);
            if (name == "hash_multi") {
                writeOk = writer.status() == PkDataStream::Ok;
                std::printf("PKHASH version=%s order=%s bytes=%s\n",
                            versionName.c_str(), orderName.c_str(), toHex(encoded).c_str());
            } else {
                writeOk = writer.status() == PkDataStream::Ok && toHex(encoded) == bytesHex;
            }
        }
        ++total;
        if (!writeOk) {
            ++mismatch;
            ++tags["write-" + name + '-' + versionName + '-' + orderName];
        }
    }

    std::printf("DIFF total=%d mismatch=%d\n", total, mismatch);
    for (const auto &tag : tags) std::printf("DIFFTAG wire %s %d\n", tag.first.c_str(), tag.second);
    return 0;
}
