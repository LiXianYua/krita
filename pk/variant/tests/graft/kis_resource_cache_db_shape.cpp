// This is not libs/resources/KisResourceCacheDb.cpp. It is a driver that copies
// the QDataStream call shapes at lines 2444-2447 and 2510-2514 verbatim. The
// production file cannot be compiled in the R-31 lock: QSqlDatabase/QSqlQuery,
// KisResourceLocator, and the full resources target remain behind S-02-b.
// Base64 is deliberately a test-only adapter because R-31 owns the wire codec,
// not the QByteArray codec API.

#include "PkDataStream.h"
#include "PkVariant.h"

#include <cstdio>
#include <string>
#include <vector>

class GraftByteArray : public PkByteArray
{
public:
    using PkByteArray::PkByteArray;
    GraftByteArray() = default;
    GraftByteArray(const PkByteArray &other) : PkByteArray(other) {}

    static GraftByteArray fromBase64(const GraftByteArray &encoded)
    {
        static const std::string alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<std::uint8_t> output;
        unsigned int bits = 0;
        int bitCount = 0;
        for (int i = 0; i < encoded.size(); ++i) {
            const char c = encoded.constData()[i];
            if (c == '=') break;
            const std::size_t value = alphabet.find(c);
            if (value == std::string::npos) continue;
            bits = (bits << 6) | static_cast<unsigned int>(value);
            bitCount += 6;
            if (bitCount >= 8) {
                bitCount -= 8;
                output.push_back(static_cast<std::uint8_t>(bits >> bitCount));
                bits &= (1u << bitCount) - 1u;
            }
        }
        return GraftByteArray(PkByteArray(output));
    }

    GraftByteArray toBase64() const
    {
        static const char alphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        unsigned int bits = 0;
        int bitCount = 0;
        for (int i = 0; i < size(); ++i) {
            bits = (bits << 8) | static_cast<unsigned char>(constData()[i]);
            bitCount += 8;
            while (bitCount >= 6) {
                bitCount -= 6;
                output.push_back(alphabet[(bits >> bitCount) & 0x3f]);
                bits &= (1u << bitCount) - 1u;
            }
        }
        if (bitCount) output.push_back(alphabet[(bits << (6 - bitCount)) & 0x3f]);
        while ((output.size() & 3u) != 0u) output.push_back('=');
        return GraftByteArray(output.data(), static_cast<int>(output.size()));
    }
};

#define QByteArray GraftByteArray
#define QDataStream PkDataStream
#define QIODevice PkStream
#define QVariant PkVariant

int main()
{
    // KisResourceCacheDb.cpp:2442-2447, with only the SQL-provided input made literal.
    QByteArray ba("AAAAAgAAAAAq", 12);
    if (!ba.isEmpty()) {
        QDataStream ds(QByteArray::fromBase64(ba));
        QVariant value;
        ds.setVersion(QDataStream::Qt_5_15); // so Qt6 can read metatypes written by Qt5
        ds >> value;
        if (ds.status() != QDataStream::Ok || value.toInt() != 42) return 1;
    }

    // KisResourceCacheDb.cpp:2508-2515, with only the map iterator value made literal.
    QVariant v(42);
    if (!v.isNull() && v.isValid()) {
        QByteArray ba;
        QDataStream ds(&ba, QIODevice::WriteOnly);
        ds.setVersion(QDataStream::Qt_5_15); // so Qt6 can write metatypes readable by Qt5
        ds << v;
        ba = ba.toBase64();
        if (ds.status() != QDataStream::Ok ||
            std::string(ba.constData(), static_cast<std::size_t>(ba.size())) != "AAAAAgAAAAAq") return 2;
    }

    std::puts("GRAFT PASS KisResourceCacheDb QDataStream shape");
    return 0;
}
