#include <QByteArray>
#include <QDataStream>
#include <QRect>
#include <QVariant>

#include <cstdio>
#include <type_traits>

class PkDataStream;
static_assert(!std::is_same<QDataStream, PkDataStream>::value);

namespace {
void hex(const QByteArray &bytes) { for (unsigned char byte : bytes) std::printf("%02x", byte); }
const char *versionName(QDataStream::Version v) { return v == QDataStream::Qt_4_6 ? "qt46" : "qt515"; }
const char *orderName(QDataStream::ByteOrder o) { return o == QDataStream::BigEndian ? "big" : "little"; }

void prefix(const char *kind, QDataStream::Version v, QDataStream::ByteOrder o,
            const char *caseName, const QByteArray &input)
{
    std::printf("kind=%s version=%s order=%s case=%s input=", kind, versionName(v), orderName(o), caseName);
    hex(input);
}

void bare(QDataStream::Version v, QDataStream::ByteOrder o, const char *caseName, const QByteArray &input)
{
    bool value = true;
    QDataStream stream(input);
    stream.setVersion(v); stream.setByteOrder(o); stream >> value;
    prefix("bare", v, o, caseName, input);
    std::printf(" value=%u status=%d\n", value, int(stream.status()));
}

void variant(QDataStream::Version v, QDataStream::ByteOrder o, const char *kind,
             const char *caseName, const QByteArray &input)
{
    QVariant value;
    QDataStream stream(input);
    stream.setVersion(v); stream.setByteOrder(o);
    stream >> value;
    prefix(kind, v, o, caseName, input);
    std::printf(" valid=%u null=%u type=%d bool=%u status=%d\n",
                value.isValid(), value.isNull(), int(value.type()), value.toBool(), int(stream.status()));
}

void rect(QDataStream::Version v, QDataStream::ByteOrder o, const char *caseName, const QByteArray &input)
{
    QVariant value;
    QDataStream stream(input);
    stream.setVersion(v); stream.setByteOrder(o); stream >> value;
    const QRect rectangle = value.toRect();
    prefix("rect", v, o, caseName, input);
    std::printf(" valid=%u x=%d y=%d w=%d h=%d status=%d\n", value.isValid(), rectangle.x(), rectangle.y(),
                rectangle.width(), rectangle.height(), int(stream.status()));
}

QByteArray frame(const char *nullFlag, const char *payload, QDataStream::ByteOrder o)
{
    QByteArray bytes = QByteArray::fromHex(o == QDataStream::BigEndian ? "0000000100" : "0100000000");
    bytes[4] = QByteArray::fromHex(nullFlag)[0];
    bytes += QByteArray::fromHex(payload);
    return bytes;
}

QByteArray rectFrame(const char *payload, QDataStream::ByteOrder o)
{
    QByteArray bytes = QByteArray::fromHex(o == QDataStream::BigEndian ? "0000001300" : "1300000000");
    QByteArray coordinates = QByteArray::fromHex(payload);
    if (o == QDataStream::LittleEndian) {
        for (int i = 0; i < coordinates.size(); i += 4) {
            const char first = coordinates[i];
            coordinates[i] = coordinates[i + 3];
            coordinates[i + 3] = first;
            const char second = coordinates[i + 1];
            coordinates[i + 1] = coordinates[i + 2];
            coordinates[i + 2] = second;
        }
    }
    bytes += coordinates;
    return bytes;
}
} // namespace

int main()
{
    const char *bytes[] = {"00", "01", "02", "ff"};
    for (QDataStream::Version version : {QDataStream::Qt_4_6, QDataStream::Qt_5_15}) {
        for (QDataStream::ByteOrder order : {QDataStream::BigEndian, QDataStream::LittleEndian}) {
            for (const char *value : bytes) bare(version, order, value, QByteArray::fromHex(value));
            for (const char *value : bytes) variant(version, order, "variant-null", value, frame(value, "01", order));
            for (const char *value : bytes) variant(version, order, "variant-bool", value, frame("00", value, order));
            rect(version, order, "int-min-max", rectFrame("80000000800000007fffffff7fffffff", order));
            rect(version, order, "reversed-extremes", rectFrame("7fffffff7fffffff8000000080000000", order));
        }
    }
}
