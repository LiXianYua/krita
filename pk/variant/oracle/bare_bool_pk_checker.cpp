#include "PkDataStream.h"
#include "PkVariant.h"

#include <cstdio>
#include <type_traits>
#include <vector>

class QDataStream;
static_assert(!std::is_same<PkDataStream, QDataStream>::value);

namespace {
void hex(const PkByteArray &bytes) { for (int i = 0; i < bytes.size(); ++i) std::printf("%02x", static_cast<unsigned int>(static_cast<unsigned char>(bytes.constData()[i]))); }
const char *versionName(PkDataStream::Version v) { return v == PkDataStream::Qt_4_6 ? "qt46" : "qt515"; }
const char *orderName(PkDataStream::ByteOrder o) { return o == PkDataStream::BigEndian ? "big" : "little"; }
PkByteArray fromHex(const char *text)
{
    std::vector<unsigned char> bytes;
    for (; *text; text += 2) {
        const auto nibble = [](char c) -> unsigned char { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10; };
        bytes.push_back(static_cast<unsigned char>((nibble(text[0]) << 4) | nibble(text[1])));
    }
    return PkByteArray(bytes);
}
PkByteArray append(PkByteArray first, const PkByteArray &second)
{
    std::vector<unsigned char> bytes;
    for (int i = 0; i < first.size(); ++i) bytes.push_back((unsigned char)first.constData()[i]);
    for (int i = 0; i < second.size(); ++i) bytes.push_back((unsigned char)second.constData()[i]);
    return PkByteArray(bytes);
}
void prefix(const char *kind, PkDataStream::Version v, PkDataStream::ByteOrder o, const char *caseName, const PkByteArray &input)
{
    std::printf("kind=%s version=%s order=%s case=%s input=", kind, versionName(v), orderName(o), caseName); hex(input);
}
void bare(PkDataStream::Version v, PkDataStream::ByteOrder o, const char *caseName, const PkByteArray &input)
{
    bool value = true; PkDataStream stream(input); stream.setVersion(v); stream.setByteOrder(o); stream >> value;
    prefix("bare", v, o, caseName, input); std::printf(" value=%u status=%d\n", static_cast<unsigned int>(value), int(stream.status()));
}
void variant(PkDataStream::Version v, PkDataStream::ByteOrder o, const char *kind, const char *caseName, const PkByteArray &input)
{
    PkVariant value(true); PkDataStream stream(input); stream.setVersion(v); stream.setByteOrder(o); stream >> value;
    prefix(kind, v, o, caseName, input);
    std::printf(" valid=%u null=%u type=%d bool=%u status=%d\n",
                static_cast<unsigned int>(value.isValid()), static_cast<unsigned int>(value.isNull()),
                value.type(), static_cast<unsigned int>(value.toBool()), int(stream.status()));
}
void rect(PkDataStream::Version v, PkDataStream::ByteOrder o, const char *caseName, const PkByteArray &input)
{
    PkVariant value; PkDataStream stream(input); stream.setVersion(v); stream.setByteOrder(o); stream >> value; const PkRect rectangle = value.toRect();
    prefix("rect", v, o, caseName, input);
    std::printf(" valid=%u left=%d top=%d right=%d bottom=%d status=%d\n",
                static_cast<unsigned int>(value.isValid()), rectangle.left(), rectangle.top(),
                rectangle.right(), rectangle.bottom(), int(stream.status()));
}
PkByteArray frame(const char *nullFlag, const char *payload, PkDataStream::ByteOrder o)
{
    PkByteArray bytes = fromHex(o == PkDataStream::BigEndian ? "0000000100" : "0100000000"); bytes.data()[4] = fromHex(nullFlag).constData()[0]; return append(bytes, fromHex(payload));
}
PkByteArray rectFrame(const char *payload, PkDataStream::ByteOrder o)
{
    PkByteArray coordinates = fromHex(payload);
    if (o == PkDataStream::LittleEndian) for (int i = 0; i < coordinates.size(); i += 4) { std::swap(coordinates.data()[i], coordinates.data()[i + 3]); std::swap(coordinates.data()[i + 1], coordinates.data()[i + 2]); }
    return append(fromHex(o == PkDataStream::BigEndian ? "0000001300" : "1300000000"), coordinates);
}
} // namespace

int main()
{
    const char *bytes[] = {"00", "01", "02", "ff"};
    for (PkDataStream::Version version : {PkDataStream::Qt_4_6, PkDataStream::Qt_5_15}) {
        for (PkDataStream::ByteOrder order : {PkDataStream::BigEndian, PkDataStream::LittleEndian}) {
            for (const char *value : bytes) bare(version, order, value, fromHex(value));
            for (const char *value : bytes) variant(version, order, "variant-null", value, frame(value, "01", order));
            for (const char *value : bytes) variant(version, order, "variant-bool", value, frame("00", value, order));
            rect(version, order, "int-min-max", rectFrame("80000000800000007fffffff7fffffff", order));
            rect(version, order, "reversed-extremes", rectFrame("7fffffff7fffffff8000000080000000", order));
        }
    }
}
