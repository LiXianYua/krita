#include <QByteArray>
#include <QDataStream>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QLine>
#include <QList>
#include <QMap>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QTimeZone>
#include <QVariant>

#include <cstdio>
#include <type_traits>
#include <vector>

class PkVariant;
class PkDataStream;

static_assert(!std::is_same<QVariant, PkVariant>::value, "Qt and pk variants must be distinct");
static_assert(!std::is_same<QDataStream, PkDataStream>::value, "Qt and pk streams must be distinct");

struct WireUserType
{
    int value = 0;
};

QDataStream &operator<<(QDataStream &stream, const WireUserType &value)
{
    return stream << value.value;
}

QDataStream &operator>>(QDataStream &stream, WireUserType &value)
{
    return stream >> value.value;
}

Q_DECLARE_METATYPE(WireUserType)

struct WireCase
{
    const char *name;
    QVariant value;
};

static QByteArray encode(const QVariant &value, QDataStream::Version version,
                         QDataStream::ByteOrder byteOrder)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(version);
    stream.setByteOrder(byteOrder);
    stream << value;
    return bytes;
}

static void printHex(const QByteArray &bytes)
{
    for (unsigned char byte : bytes) {
        std::printf("%02x", static_cast<unsigned int>(byte));
    }
}

static quint32 wireType(const QByteArray &bytes, QDataStream::ByteOrder order)
{
    const auto byte = [&bytes](int i) { return static_cast<quint32>(static_cast<unsigned char>(bytes.at(i))); };
    if (order == QDataStream::BigEndian) {
        return (byte(0) << 24) | (byte(1) << 16) | (byte(2) << 8) | byte(3);
    }
    return byte(0) | (byte(1) << 8) | (byte(2) << 16) | (byte(3) << 24);
}

int main()
{
    qRegisterMetaTypeStreamOperators<WireUserType>("WireUserType");
    QVariantList list{QVariant(7), QVariant(QString::fromUtf8("h\xC3\xA9"))};
    QVariantMap map{{QStringLiteral("a"), QVariant(3)},
                    {QString::fromUtf8("\xCE\xB2"), QVariant(false)}};
    QVariantHash hash{{QStringLiteral("a"), QVariant(3)}};
    QVariantHash multiHash{{QStringLiteral("a"), QVariant(3)},
                           {QStringLiteral("b"), QVariant(QStringLiteral("two"))},
                           {QStringLiteral("c"), QVariant(false)}};
    const ushort isolatedUnits[]{0x0041, 0xd800, 0x0042, 0xdc00};
    const QDateTime localDateTime(QDate(2024, 2, 29), QTime(12, 34, 56, 789), Qt::LocalTime);
    const QDateTime utcDateTime(QDate(2024, 2, 29), QTime(12, 34, 56, 789), Qt::UTC);
    const QDateTime offsetDateTime(QDate(2024, 2, 29), QTime(12, 34, 56, 789),
                                   Qt::OffsetFromUTC, 19800);
    const QDateTime zoneDateTime(QDate(2024, 2, 29), QTime(12, 34, 56, 789),
                                 QTimeZone("Asia/Kolkata"));
    const std::vector<WireCase> cases{
        {"invalid", QVariant()},
        {"bool", QVariant(true)},
        {"int", QVariant(-1234567)},
        {"uint", QVariant(0xf1234567u)},
        {"longlong", QVariant::fromValue<qlonglong>(-0x102030405060708LL)},
        {"ulonglong", QVariant::fromValue<qulonglong>(0xf102030405060708ULL)},
        {"double", QVariant(1234.5)},
        {"float", QVariant(12.25f)},
        {"string", QVariant(QString::fromUtf8("A\xF0\x9F\x8E\xA8"))},
        {"string_isolated_utf16", QVariant(QString::fromUtf16(isolatedUnits, 4))},
        {"string_null", QVariant(QVariant::String)},
        {"bytearray", QVariant(QByteArray("A\0Z", 3))},
        {"bytearray_null", QVariant(QVariant::ByteArray)},
        {"stringlist", QVariant(QStringList{QStringLiteral("a"), QString::fromUtf8("\xCE\xB2")})},
        {"list", QVariant(list)},
        {"map", QVariant(map)},
        {"hash", QVariant(hash)},
        {"hash_multi", QVariant(multiHash)},
        {"date", QVariant(QDate(2024, 2, 29))},
        {"date_null", QVariant(QDate())},
        {"date_typed_null", QVariant(QVariant::Date)},
        {"time", QVariant(QTime(12, 34, 56, 789))},
        {"time_null", QVariant(QTime())},
        {"time_typed_null", QVariant(QVariant::Time)},
        {"datetime", QVariant(localDateTime)},
        {"datetime_null", QVariant(QDateTime())},
        {"datetime_typed_null", QVariant(QVariant::DateTime)},
        {"datetime_utc", QVariant(utcDateTime)},
        {"datetime_offset", QVariant(offsetDateTime)},
        {"datetime_timezone", QVariant(zoneDateTime)},
        {"rect", QVariant(QRect(-2, 3, 4, 5))},
        {"rectf", QVariant(QRectF(-2.5, 3.25, 4.5, 5.75))},
        {"size", QVariant(QSize(-2, 3))},
        {"sizef", QVariant(QSizeF(-2.5, 3.25))},
        {"line", QVariant(QLine(-2, 3, 4, 5))},
        {"linef", QVariant(QLineF(-2.5, 3.25, 4.5, 5.75))},
        {"point", QVariant(QPoint(-2, 3))},
        {"pointf", QVariant(QPointF(-2.5, 3.25))},
        {"usertype", QVariant::fromValue(WireUserType{42})},
    };

    const struct {
        const char *name;
        QDataStream::Version value;
    } versions[]{{"qt_4_6", QDataStream::Qt_4_6},
                 {"qt_5_15", QDataStream::Qt_5_15}};
    const struct {
        const char *name;
        QDataStream::ByteOrder value;
    } orders[]{{"big", QDataStream::BigEndian},
               {"little", QDataStream::LittleEndian}};

    for (const auto &version : versions) {
        for (const auto &order : orders) {
            for (const WireCase &wireCase : cases) {
                const QByteArray bytes = encode(wireCase.value, version.value, order.value);
                std::printf("FIXTURE version=%s order=%s name=%s meta_type=%u wire_type=%u null=%u payload=",
                            version.name, order.name, wireCase.name,
                            static_cast<unsigned int>(wireCase.value.userType()),
                            static_cast<unsigned int>(wireType(bytes, order.value)),
                            wireCase.value.isNull() ? 1u : 0u);
                printHex(bytes.mid(5));
                std::printf(" bytes=");
                printHex(bytes);
                std::printf("\n");
            }
        }
    }
}
