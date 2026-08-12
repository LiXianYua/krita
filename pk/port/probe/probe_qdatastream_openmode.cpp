// R-12 probe: QDataStream defaults + QIODevice::OpenMode bit values.
// Measured against real Qt 5.15.13.
//
// Build:
//   QT=/mnt/ssd-disk/liyang/projects/kde-deps/usr
//   g++ -fPIC -std=c++17 probe_qdatastream_openmode.cpp -o probe_qdatastream_openmode \
//     -I$QT/include/x86_64-linux-gnu/qt5 -I$QT/include/x86_64-linux-gnu/qt5/QtCore \
//     -L$QT/lib/x86_64-linux-gnu -lQt5Core
//   LD_LIBRARY_PATH=$QT/lib/x86_64-linux-gnu ./probe_qdatastream_openmode

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QSysInfo>
#include <QtGlobal>

#include <cstdio>

#define L(...)                                                                                     \
    do {                                                                                           \
        printf(__VA_ARGS__);                                                                       \
        printf("\n");                                                                              \
        fflush(stdout);                                                                            \
    } while (0)

static void section(const char *s)
{
    printf("\n########## %s ##########\n", s);
    fflush(stdout);
}

static const char *byteOrderName(int v)
{
    switch (v) {
    case QDataStream::BigEndian:
        return "QDataStream::BigEndian";
    case QDataStream::LittleEndian:
        return "QDataStream::LittleEndian";
    default:
        return "?";
    }
}

static const char *fpName(int v)
{
    switch (v) {
    case QDataStream::SinglePrecision:
        return "QDataStream::SinglePrecision (float is written as 4 bytes)";
    case QDataStream::DoublePrecision:
        return "QDataStream::DoublePrecision (float is written as 8 bytes)";
    default:
        return "?";
    }
}

static const char *statusName(int v)
{
    switch (v) {
    case QDataStream::Ok:
        return "QDataStream::Ok";
    case QDataStream::ReadPastEnd:
        return "QDataStream::ReadPastEnd";
    case QDataStream::ReadCorruptData:
        return "QDataStream::ReadCorruptData";
    case QDataStream::WriteFailed:
        return "QDataStream::WriteFailed";
    default:
        return "?";
    }
}

int main()
{
    L("Qt runtime version: %s (compiled against %s)", qVersion(), QT_VERSION_STR);

    // ---- 14. QDataStream defaults ----------------------------------------
    section("Item 14: QDataStream defaults");
    {
        QByteArray ba;
        QDataStream s(&ba, QIODevice::WriteOnly);
        L("[14] default version()=%d", s.version());
        L("[14] Qt_5_15 == %d ; Qt_DefaultCompiledVersion == %d ; Qt_1_0 == %d",
          (int)QDataStream::Qt_5_15, (int)QDataStream::Qt_DefaultCompiledVersion,
          (int)QDataStream::Qt_1_0);
        L("[14] default byteOrder()=%d -> %s   (host is %s)", (int)s.byteOrder(),
          byteOrderName(s.byteOrder()),
          QSysInfo::ByteOrder == QSysInfo::LittleEndian ? "LittleEndian" : "BigEndian");
        L("[14] enum values: BigEndian=%d LittleEndian=%d", (int)QDataStream::BigEndian,
          (int)QDataStream::LittleEndian);
        L("[14] default floatingPointPrecision()=%d -> %s", (int)s.floatingPointPrecision(),
          fpName(s.floatingPointPrecision()));
        L("[14] enum values: SinglePrecision=%d DoublePrecision=%d",
          (int)QDataStream::SinglePrecision, (int)QDataStream::DoublePrecision);
        L("[14] default status()=%d -> %s", (int)s.status(), statusName(s.status()));
        L("[14] status enum: Ok=%d ReadPastEnd=%d ReadCorruptData=%d WriteFailed=%d",
          (int)QDataStream::Ok, (int)QDataStream::ReadPastEnd, (int)QDataStream::ReadCorruptData,
          (int)QDataStream::WriteFailed);
    }

    // proof of what the default byte order actually emits on the wire
    {
        QByteArray ba;
        QDataStream s(&ba, QIODevice::WriteOnly);
        s << (quint32)0x01020304u;
        printf("[14] wrote quint32 0x01020304 with default settings -> bytes:");
        for (int i = 0; i < ba.size(); ++i)
            printf(" %02x", (unsigned char)ba.at(i));
        printf("  (size=%d)\n", (int)ba.size());
        fflush(stdout);

        QByteArray fb;
        QDataStream fs(&fb, QIODevice::WriteOnly);
        fs << (float)1.0f;
        L("[14] wrote a `float` 1.0f with default settings -> %d bytes on the wire  <-- "
          "DoublePrecision default widens float to 8 bytes",
          (int)fb.size());
    }

    // ---- 14b. reading past the end ---------------------------------------
    section("Item 14b: reading a quint32 past the end of the stream");
    {
        QByteArray empty;
        QBuffer b(&empty);
        b.open(QIODevice::ReadOnly);
        QDataStream s(&b);
        quint32 v = 0xDEADBEEFu;
        s >> v;
        L("[14] empty stream, `s >> quint32` : status()=%d -> %s ; target var = 0x%08x (was "
          "0xDEADBEEF)",
          (int)s.status(), statusName(s.status()), v);
        L("[14] after failure: atEnd()=%s device-pos=%lld", s.atEnd() ? "true" : "false",
          (long long)b.pos());
        // a second read on a stream already in ReadPastEnd
        quint32 v2 = 0xCAFEBABEu;
        s >> v2;
        L("[14] second `>>` on a ReadPastEnd stream: status()=%d target var = 0x%08x (was "
          "0xCAFEBABE)",
          (int)s.status(), v2);
        s.resetStatus();
        L("[14] after resetStatus(): status()=%d -> %s", (int)s.status(), statusName(s.status()));
    }
    {
        // partial data: 2 bytes available, need 4
        QByteArray two("\x01\x02", 2);
        QBuffer b(&two);
        b.open(QIODevice::ReadOnly);
        QDataStream s(&b);
        quint32 v = 0xDEADBEEFu;
        s >> v;
        L("[14] 2-byte stream, `s >> quint32` : status()=%d -> %s ; target var = 0x%08x ; "
          "device-pos-after=%lld  <-- were the 2 bytes consumed?",
          (int)s.status(), statusName(s.status()), v, (long long)b.pos());
    }
    {
        // a good read followed by a past-end read
        QByteArray four;
        {
            QDataStream w(&four, QIODevice::WriteOnly);
            w << (quint32)0x11223344u;
        }
        QBuffer b(&four);
        b.open(QIODevice::ReadOnly);
        QDataStream s(&b);
        quint32 a = 0, c = 0x5A5A5A5Au;
        s >> a;
        L("[14] 4-byte stream: first `>>` gave 0x%08x status=%s atEnd=%s", a,
          statusName(s.status()), s.atEnd() ? "true" : "false");
        s >> c;
        L("[14] then a second `>>`: status=%s target var = 0x%08x (was 0x5A5A5A5A)",
          statusName(s.status()), c);
    }
    {
        // QDataStream on an UNOPENED device
        QByteArray ba("abcd", 4);
        QBuffer b(&ba); // not open
        QDataStream s(&b);
        quint32 v = 0xDEADBEEFu;
        s >> v;
        L("[14] QDataStream over an UNOPENED QBuffer: status()=%s var=0x%08x",
          statusName(s.status()), v);
    }

    // ---- 15. OpenMode bit values -----------------------------------------
    section("Item 15: QIODevice::OpenMode bit values");
    {
#define P(x)                                                                                       \
    L("[15] QIODevice::%-14s = 0x%04x = %6d", #x, (unsigned)QIODevice::x, (int)QIODevice::x)
        P(NotOpen);
        P(ReadOnly);
        P(WriteOnly);
        P(ReadWrite);
        P(Append);
        P(Truncate);
        P(Text);
        P(Unbuffered);
        P(NewOnly);
        P(ExistingOnly);
#undef P
        L("[15] check: ReadWrite == (ReadOnly|WriteOnly) ? %s",
          (QIODevice::ReadWrite == (QIODevice::ReadOnly | QIODevice::WriteOnly)) ? "yes" : "no");
    }

    L("\n--- probe_qdatastream_openmode done ---");
    return 0;
}
