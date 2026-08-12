// R-12 probe: QIODevice family exact behaviour, measured against real Qt 5.15.13.
//
// Build:
//   QT=/mnt/ssd-disk/liyang/projects/kde-deps/usr
//   g++ -fPIC -std=c++17 probe_qiodevice.cpp -o probe_qiodevice \
//     -I$QT/include/x86_64-linux-gnu/qt5 -I$QT/include/x86_64-linux-gnu/qt5/QtCore \
//     -L$QT/lib/x86_64-linux-gnu -lQt5Core
//   LD_LIBRARY_PATH=$QT/lib/x86_64-linux-gnu ./probe_qiodevice
//
// Every observation prints one readable line. Qt's own warnings are captured by a
// message handler and printed inline as [qt-warning] so they line up with the call
// that produced them.

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QtGlobal>

#include <cstdio>
#include <cstring>
#include <unistd.h>

static void msgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    printf("    [qt-warning] %s\n", msg.toLocal8Bit().constData());
    fflush(stdout);
}

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

static QString q(const QString &s) { return s; }
static const char *cs(const QString &s)
{
    static QByteArray keep;
    keep = s.toLocal8Bit();
    return keep.constData();
}
static const char *tf(bool b) { return b ? "true" : "false"; }

// A QBuffer that lies about being sequential -- one of the two sequential-device
// vehicles used below.
class SeqBuffer : public QBuffer
{
public:
    bool isSequential() const override { return true; }
};

static const char *DATA = "0123456789"; // 10 bytes
static QString g_path;

static bool makeFile()
{
    QFile f(g_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(DATA, 10);
    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// Items 1..6, 9, 11, 12, 13 run against both QFile and QBuffer via this template.
// ---------------------------------------------------------------------------
template <typename Open>
static void randomAccessSuite(const char *tag, Open open)
{
    char buf[64];

    // ---- 1. read() at EOF -------------------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        d->seek(d->size());
        qint64 r1 = d->read(buf, 10);
        QString e1 = d->errorString();
        qint64 r2 = d->read(buf, 10);
        QString e2 = d->errorString();
        L("[1][%s] at-EOF read(buf,10) #1 ret=%lld errorString=\"%s\" atEnd=%s pos=%lld", tag,
          (long long)r1, cs(e1), tf(d->atEnd()), (long long)d->pos());
        L("[1][%s] at-EOF read(buf,10) #2 ret=%lld errorString=\"%s\"", tag, (long long)r2, cs(e2));
        // errorString before any failure, for comparison
        auto fresh = open(QIODevice::ReadOnly);
        L("[1][%s] fresh-open errorString=\"%s\"  (compare with the two above)", tag,
          cs(fresh->errorString()));
    }

    // ---- 2. partial read across EOF --------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        d->seek(7); // 3 bytes left
        memset(buf, 0, sizeof buf);
        qint64 r = d->read(buf, 10);
        L("[2][%s] pos=7 (3 left) read(buf,10) ret=%lld data=\"%.*s\" pos-after=%lld atEnd=%s", tag,
          (long long)r, (int)(r > 0 ? r : 0), buf, (long long)d->pos(), tf(d->atEnd()));
    }

    // ---- 3. readAll() at EOF ---------------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        d->seek(d->size());
        QByteArray a = d->readAll();
        L("[3][%s] at-EOF readAll() size=%d isNull=%s isEmpty=%s errorString=\"%s\"", tag,
          (int)a.size(), tf(a.isNull()), tf(a.isEmpty()), cs(d->errorString()));
        QByteArray b = d->readAll(); // second call
        L("[3][%s] at-EOF readAll() again size=%d isNull=%s isEmpty=%s", tag, (int)b.size(),
          tf(b.isNull()), tf(b.isEmpty()));
    }

    // ---- 4. peek() --------------------------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        QByteArray p = d->peek(4);
        L("[4][%s] fresh peek(4) -> \"%s\" size=%d pos-after=%lld bytesAvailable-after=%lld", tag,
          p.constData(), (int)p.size(), (long long)d->pos(), (long long)d->bytesAvailable());
        d->seek(8); // 2 bytes left
        QByteArray p2 = d->peek(10);
        L("[4][%s] pos=8 (2 left) peek(10) -> \"%s\" size=%d isNull=%s pos-after=%lld", tag,
          p2.constData(), (int)p2.size(), tf(p2.isNull()), (long long)d->pos());
        d->seek(d->size());
        QByteArray p3 = d->peek(10);
        L("[4][%s] at-EOF peek(10) size=%d isNull=%s isEmpty=%s pos-after=%lld atEnd-after=%s", tag,
          (int)p3.size(), tf(p3.isNull()), tf(p3.isEmpty()), (long long)d->pos(), tf(d->atEnd()));
        char pb[16];
        d->seek(0);
        qint64 pr = d->peek(pb, 4);
        L("[4][%s] peek(char*,4) ret=%lld pos-after=%lld", tag, (long long)pr, (long long)d->pos());
    }

    // ---- 5. seek() boundaries --------------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        bool s1 = d->seek(d->size());
        L("[5][%s] seek(size()=%lld) ret=%s pos=%lld atEnd=%s", tag, (long long)d->size(), tf(s1),
          (long long)d->pos(), tf(d->atEnd()));
        bool s2 = d->seek(d->size() + 100);
        L("[5][%s] seek(size()+100) ret=%s pos=%lld atEnd=%s bytesAvailable=%lld errorString=\"%s\"",
          tag, tf(s2), (long long)d->pos(), tf(d->atEnd()), (long long)d->bytesAvailable(),
          cs(d->errorString()));
        if (s2) {
            qint64 r = d->read(buf, 10);
            L("[5][%s] after seek(size()+100): read(buf,10) ret=%lld pos=%lld atEnd=%s "
              "errorString=\"%s\"",
              tag, (long long)r, (long long)d->pos(), tf(d->atEnd()), cs(d->errorString()));
        }
        auto d2 = open(QIODevice::ReadOnly);
        d2->seek(4);
        bool s3 = d2->seek(-1);
        L("[5][%s] from pos=4: seek(-1) ret=%s pos-after=%lld errorString=\"%s\"", tag, tf(s3),
          (long long)d2->pos(), cs(d2->errorString()));
    }

    // ---- 6. atEnd() timing ------------------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        L("[6][%s] freshly opened non-empty: atEnd=%s pos=%lld size=%lld", tag, tf(d->atEnd()),
          (long long)d->pos(), (long long)d->size());
        qint64 r = d->read(buf, 10); // consume exactly all 10 bytes, no failing read yet
        L("[6][%s] after read(buf,10) ret=%lld pos=%lld : atEnd=%s   <-- is it pos>=size, or "
          "'last read failed'?",
          tag, (long long)r, (long long)d->pos(), tf(d->atEnd()));
        qint64 r2 = d->read(buf, 1);
        L("[6][%s] then read(buf,1) ret=%lld atEnd=%s", tag, (long long)r2, tf(d->atEnd()));
    }

    // ---- 7. bytesAvailable() on random-access ----------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        L("[7][%s] fresh: bytesAvailable=%lld size()-pos()=%lld", tag, (long long)d->bytesAvailable(),
          (long long)(d->size() - d->pos()));
        d->seek(6);
        L("[7][%s] pos=6: bytesAvailable=%lld size()-pos()=%lld", tag, (long long)d->bytesAvailable(),
          (long long)(d->size() - d->pos()));
        d->peek(3);
        L("[7][%s] pos=6 after peek(3): bytesAvailable=%lld (peek fills the read buffer)", tag,
          (long long)d->bytesAvailable());
    }

    // ---- 9. getChar / ungetChar ------------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        char c = 0;
        bool g = d->getChar(&c);
        L("[9][%s] fresh getChar ret=%s c='%c' pos=%lld bytesAvailable=%lld", tag, tf(g), c,
          (long long)d->pos(), (long long)d->bytesAvailable());
        d->ungetChar(c);
        L("[9][%s] after ungetChar('%c'): pos=%lld bytesAvailable=%lld atEnd=%s", tag, c,
          (long long)d->pos(), (long long)d->bytesAvailable(), tf(d->atEnd()));
        // two ungetChar in a row (second one un-gets a byte that was never read)
        char c2 = 0;
        d->getChar(&c2);
        char c3 = 0;
        d->getChar(&c3);
        L("[9][%s] read two more: '%c''%c' pos=%lld", tag, c2, c3, (long long)d->pos());
        d->ungetChar(c3);
        d->ungetChar(c2);
        L("[9][%s] two consecutive ungetChar: pos=%lld bytesAvailable=%lld", tag, (long long)d->pos(),
          (long long)d->bytesAvailable());
        memset(buf, 0, sizeof buf);
        qint64 rr = d->read(buf, 4);
        L("[9][%s] then read(buf,4) ret=%lld data=\"%.*s\"  <-- did the unget bytes come back in "
          "order?",
          tag, (long long)rr, (int)(rr > 0 ? rr : 0), buf);
        // getChar at EOF
        auto d2 = open(QIODevice::ReadOnly);
        d2->seek(d2->size());
        char c4 = 0x7f;
        bool g2 = d2->getChar(&c4);
        L("[9][%s] at-EOF getChar ret=%s c-byte=0x%02x (was 0x7f before) pos=%lld", tag, tf(g2),
          (unsigned char)c4, (long long)d2->pos());
        // ungetChar at EOF, then read
        char inject = 'Z';
        d2->ungetChar(inject);
        memset(buf, 0, sizeof buf);
        qint64 r5 = d2->read(buf, 4);
        L("[9][%s] at-EOF ungetChar('Z') then read(buf,4) ret=%lld data=\"%.*s\" pos=%lld atEnd=%s",
          tag, (long long)r5, (int)(r5 > 0 ? r5 : 0), buf, (long long)d2->pos(), tf(d2->atEnd()));
    }

    // ---- 11. write() return values ---------------------------------------
    {
        auto rw = open(QIODevice::ReadWrite);
        qint64 w = rw->write("AB", 2);
        L("[11][%s] ReadWrite write(\"AB\",2) ret=%lld pos=%lld errorString=\"%s\"", tag,
          (long long)w, (long long)rw->pos(), cs(rw->errorString()));
        qint64 w0 = rw->write("", 0);
        L("[11][%s] ReadWrite write(\"\",0) ret=%lld", tag, (long long)w0);
        auto ro = open(QIODevice::ReadOnly);
        qint64 w2 = ro->write("XY", 2);
        L("[11][%s] ReadOnly write(\"XY\",2) ret=%lld errorString=\"%s\" error-enum-nonzero-check "
          "done",
          tag, (long long)w2, cs(ro->errorString()));
    }

    // ---- 12. skip() -------------------------------------------------------
    {
        auto d = open(QIODevice::ReadOnly);
        qint64 s = d->skip(4);
        L("[12][%s] fresh skip(4) ret=%lld pos=%lld", tag, (long long)s, (long long)d->pos());
        qint64 s2 = d->skip(100);
        L("[12][%s] pos=4 (6 left) skip(100) ret=%lld pos=%lld atEnd=%s", tag, (long long)s2,
          (long long)d->pos(), tf(d->atEnd()));
        qint64 s3 = d->skip(100);
        L("[12][%s] at-EOF skip(100) ret=%lld pos=%lld errorString=\"%s\"", tag, (long long)s3,
          (long long)d->pos(), cs(d->errorString()));
        qint64 s4 = d->skip(0);
        L("[12][%s] skip(0) ret=%lld", tag, (long long)s4);
    }

    // ---- 13. readLine() ---------------------------------------------------
    {
        // device content has NO newline at all
        auto d = open(QIODevice::ReadOnly);
        QByteArray l1 = d->readLine();
        L("[13][%s] no-newline content readLine() -> \"%s\" size=%d isNull=%s pos=%lld", tag,
          l1.constData(), (int)l1.size(), tf(l1.isNull()), (long long)d->pos());
        QByteArray l2 = d->readLine();
        L("[13][%s] at-EOF readLine() size=%d isNull=%s isEmpty=%s", tag, (int)l2.size(),
          tf(l2.isNull()), tf(l2.isEmpty()));
        char lb[32];
        memset(lb, 0x7f, sizeof lb);
        auto d2 = open(QIODevice::ReadOnly);
        qint64 rl0 = d2->readLine(lb, 0);
        L("[13][%s] readLine(buf,0) ret=%lld pos=%lld buf[0]=0x%02x", tag, (long long)rl0,
          (long long)d2->pos(), (unsigned char)lb[0]);
        qint64 rl1 = d2->readLine(lb, 1);
        L("[13][%s] readLine(buf,1) ret=%lld pos=%lld", tag, (long long)rl1, (long long)d2->pos());
        memset(lb, 0x7f, sizeof lb);
        qint64 rl2 = d2->readLine(lb, 4);
        L("[13][%s] readLine(buf,4) ret=%lld data=\"%s\" pos=%lld  (maxSize includes the '\\0')",
          tag, (long long)rl2, lb, (long long)d2->pos());
    }
}

int main()
{
    qInstallMessageHandler(msgHandler);

    L("Qt runtime version: %s (compiled against %s)", qVersion(), QT_VERSION_STR);

    const char *tmpdir = getenv("PROBE_TMPDIR");
    g_path = QString::fromLocal8Bit(tmpdir ? tmpdir : "/tmp") + "/r12_probe_data.bin";
    if (!makeFile()) {
        L("FATAL: cannot create %s", cs(g_path));
        return 1;
    }
    L("data file: %s  content=\"0123456789\" (10 bytes)", cs(g_path));

    // ======================= QFile =========================================
    section("QFile (random-access device)");
    randomAccessSuite("QFile", [&](QIODevice::OpenMode m) {
        makeFile(); // reset content so each sub-test is isolated from the write tests
        auto *f = new QFile(g_path);
        f->open(m);
        return f;
    });

    // ======================= QBuffer =======================================
    section("QBuffer (memory device)");
    // QBuffer needs a backing QByteArray that lives as long as the buffer.
    static QList<QByteArray *> keepAlive;
    randomAccessSuite("QBuffer", [&](QIODevice::OpenMode m) {
        auto *ba = new QByteArray(DATA, 10);
        keepAlive.append(ba);
        auto *b = new QBuffer(ba);
        b->open(m);
        return b;
    });

    // ======================= Item 6, empty devices =========================
    section("Item 6 extra: atEnd() on freshly-opened EMPTY device");
    {
        QString ep = QString::fromLocal8Bit(tmpdir ? tmpdir : "/tmp") + "/r12_probe_empty.bin";
        {
            QFile f(ep);
            f.open(QIODevice::WriteOnly | QIODevice::Truncate);
            f.close();
        }
        QFile f(ep);
        bool ok = f.open(QIODevice::ReadOnly);
        L("[6][QFile-empty] open ret=%s size=%lld pos=%lld atEnd=%s bytesAvailable=%lld", tf(ok),
          (long long)f.size(), (long long)f.pos(), tf(f.atEnd()), (long long)f.bytesAvailable());
        QByteArray a = f.readAll();
        L("[6][QFile-empty] readAll size=%d isNull=%s isEmpty=%s", (int)a.size(), tf(a.isNull()),
          tf(a.isEmpty()));

        QByteArray empty;
        QBuffer b(&empty);
        bool ok2 = b.open(QIODevice::ReadOnly);
        L("[6][QBuffer-empty] open ret=%s size=%lld pos=%lld atEnd=%s bytesAvailable=%lld", tf(ok2),
          (long long)b.size(), (long long)b.pos(), tf(b.atEnd()), (long long)b.bytesAvailable());
        QByteArray a2 = b.readAll();
        L("[6][QBuffer-empty] readAll size=%d isNull=%s isEmpty=%s", (int)a2.size(),
          tf(a2.isNull()), tf(a2.isEmpty()));
        char cb[4];
        qint64 r = b.read(cb, 4);
        L("[6][QBuffer-empty] read(buf,4) ret=%lld errorString=\"%s\"", (long long)r,
          cs(b.errorString()));
    }

    // ======================= Item 10: unopened devices =====================
    section("Item 10: calls on a device that was never open()ed");
    {
        char buf[16];
        QFile f(g_path); // exists on disk, but NOT opened
        L("[10][QFile-unopened] isOpen=%s openMode=0x%x", tf(f.isOpen()), (unsigned)f.openMode());
        qint64 r = f.read(buf, 4);
        L("[10][QFile-unopened] read(buf,4) ret=%lld errorString=\"%s\"", (long long)r,
          cs(f.errorString()));
        QByteArray a = f.readAll();
        L("[10][QFile-unopened] readAll size=%d isNull=%s isEmpty=%s", (int)a.size(),
          tf(a.isNull()), tf(a.isEmpty()));
        qint64 w = f.write("A", 1);
        L("[10][QFile-unopened] write(\"A\",1) ret=%lld errorString=\"%s\"", (long long)w,
          cs(f.errorString()));
        bool s = f.seek(2);
        L("[10][QFile-unopened] seek(2) ret=%s", tf(s));
        L("[10][QFile-unopened] size()=%lld  <-- QFile::size() consults the filesystem",
          (long long)f.size());
        L("[10][QFile-unopened] pos()=%lld atEnd()=%s bytesAvailable()=%lld", (long long)f.pos(),
          tf(f.atEnd()), (long long)f.bytesAvailable());
        L("[10][QFile-unopened] isReadable=%s isWritable=%s isSequential=%s", tf(f.isReadable()),
          tf(f.isWritable()), tf(f.isSequential()));

        QFile nf("/definitely/does/not/exist/xyz");
        L("[10][QFile-unopened-missing] size()=%lld exists=%s pos()=%lld atEnd()=%s",
          (long long)nf.size(), tf(nf.exists()), (long long)nf.pos(), tf(nf.atEnd()));

        QByteArray ba(DATA, 10);
        QBuffer b(&ba);
        L("[10][QBuffer-unopened] isOpen=%s", tf(b.isOpen()));
        qint64 br = b.read(buf, 4);
        L("[10][QBuffer-unopened] read(buf,4) ret=%lld errorString=\"%s\"", (long long)br,
          cs(b.errorString()));
        qint64 bw = b.write("A", 1);
        L("[10][QBuffer-unopened] write(\"A\",1) ret=%lld", (long long)bw);
        L("[10][QBuffer-unopened] seek(2) ret=%s", tf(b.seek(2)));
        L("[10][QBuffer-unopened] size()=%lld pos()=%lld atEnd()=%s bytesAvailable()=%lld",
          (long long)b.size(), (long long)b.pos(), tf(b.atEnd()), (long long)b.bytesAvailable());
    }

    // ======================= Items 7 & 8: sequential devices ===============
    section("Items 7 & 8: SEQUENTIAL device #1 = QFile on a pipe read-end (real sequential)");
    {
        int fds[2];
        if (pipe(fds) != 0) {
            L("[seq] pipe() failed");
        } else {
            const char *msg = "0123456789";
            ssize_t wn = ::write(fds[1], msg, 10);
            (void)wn;
            ::close(fds[1]); // writer closed -> EOF reachable
            QFile p;
            bool ok = p.open(fds[0], QIODevice::ReadOnly);
            L("[8][seq-QFile-pipe] open(fd) ret=%s isSequential=%s", tf(ok), tf(p.isSequential()));
            L("[8][seq-QFile-pipe] size()=%lld  pos()=%lld  atEnd()=%s  bytesAvailable()=%lld  "
              "<-- BEFORE any read",
              (long long)p.size(), (long long)p.pos(), tf(p.atEnd()),
              (long long)p.bytesAvailable());
            bool sk = p.seek(2);
            L("[8][seq-QFile-pipe] seek(2) ret=%s pos-after=%lld errorString=\"%s\"", tf(sk),
              (long long)p.pos(), cs(p.errorString()));
            bool sk0 = p.seek(0);
            L("[8][seq-QFile-pipe] seek(0) ret=%s pos-after=%lld", tf(sk0), (long long)p.pos());
            QByteArray pk = p.peek(3);
            L("[7][seq-QFile-pipe] peek(3)=\"%s\" size=%d pos-after=%lld bytesAvailable-after=%lld "
              "<-- peek fills the internal buffer",
              pk.constData(), (int)pk.size(), (long long)p.pos(), (long long)p.bytesAvailable());
            char buf[64];
            memset(buf, 0, sizeof buf);
            qint64 r = p.read(buf, 4);
            L("[7][seq-QFile-pipe] read(buf,4) ret=%lld data=\"%.*s\" pos=%lld bytesAvailable=%lld "
              "size=%lld atEnd=%s",
              (long long)r, (int)(r > 0 ? r : 0), buf, (long long)p.pos(),
              (long long)p.bytesAvailable(), (long long)p.size(), tf(p.atEnd()));
            memset(buf, 0, sizeof buf);
            qint64 r2 = p.read(buf, 32);
            L("[7][seq-QFile-pipe] read(buf,32) ret=%lld data=\"%.*s\" pos=%lld atEnd=%s",
              (long long)r2, (int)(r2 > 0 ? r2 : 0), buf, (long long)p.pos(), tf(p.atEnd()));
            qint64 r3 = p.read(buf, 32);
            L("[1][seq-QFile-pipe] at-EOF read(buf,32) ret=%lld atEnd=%s bytesAvailable=%lld "
              "errorString=\"%s\"",
              (long long)r3, tf(p.atEnd()), (long long)p.bytesAvailable(), cs(p.errorString()));
            QByteArray ra = p.readAll();
            L("[3][seq-QFile-pipe] at-EOF readAll size=%d isNull=%s isEmpty=%s", (int)ra.size(),
              tf(ra.isNull()), tf(ra.isEmpty()));
            p.close();
        }
    }

    section("Items 7 & 8: SEQUENTIAL device #2 = QBuffer subclass with isSequential()==true");
    {
        QByteArray ba(DATA, 10);
        SeqBuffer sb;
        sb.setBuffer(&ba);
        bool ok = sb.open(QIODevice::ReadOnly);
        L("[8][seq-QBuffer] open ret=%s isSequential=%s", tf(ok), tf(sb.isSequential()));
        L("[8][seq-QBuffer] size()=%lld pos()=%lld atEnd()=%s bytesAvailable()=%lld  <-- BEFORE any "
          "read",
          (long long)sb.size(), (long long)sb.pos(), tf(sb.atEnd()),
          (long long)sb.bytesAvailable());
        L("[8][seq-QBuffer] seek(2) ret=%s pos-after=%lld", tf(sb.seek(2)), (long long)sb.pos());
        char buf[64];
        memset(buf, 0, sizeof buf);
        qint64 r = sb.read(buf, 4);
        L("[7][seq-QBuffer] read(buf,4) ret=%lld data=\"%.*s\" pos=%lld bytesAvailable=%lld "
          "atEnd=%s",
          (long long)r, (int)(r > 0 ? r : 0), buf, (long long)sb.pos(),
          (long long)sb.bytesAvailable(), tf(sb.atEnd()));
        QByteArray pk = sb.peek(3);
        L("[7][seq-QBuffer] peek(3)=\"%s\" pos-after=%lld bytesAvailable-after=%lld",
          pk.constData(), (long long)sb.pos(), (long long)sb.bytesAvailable());
        // DANGER: readAll() on this device never terminates -- see the note printed below.
        // Bounded loop instead, so the probe can demonstrate why without hanging.
        int iter = 0;
        qint64 total = 0;
        while (iter < 5) {
            memset(buf, 0, sizeof buf);
            qint64 n = sb.read(buf, 4);
            L("[7][seq-QBuffer] loop#%d read(buf,4) ret=%lld data=\"%.*s\" pos=%lld", iter,
              (long long)n, (int)(n > 0 ? n : 0), buf, (long long)sb.pos());
            if (n <= 0)
                break;
            total += n;
            ++iter;
        }
        L("[7][seq-QBuffer] !! after %lld bytes read, pos() is still %lld -- QIODevice does NOT "
          "advance its internal pos for sequential devices, so QBuffer::readData (which indexes by "
          "pos()) re-serves byte 0 forever. readAll() on this device NEVER TERMINATES.",
          (long long)total, (long long)sb.pos());
    }

    section("Item 7 extra: bytesAvailable() on UNOPENED devices");
    {
        QFile f(g_path);
        L("[7][QFile-unopened] bytesAvailable()=%lld (file is 10 bytes on disk)",
          (long long)f.bytesAvailable());
        QByteArray ba(DATA, 10);
        QBuffer b(&ba);
        L("[7][QBuffer-unopened] bytesAvailable()=%lld size()=%lld", (long long)b.bytesAvailable(),
          (long long)b.size());
        SeqBuffer sb;
        sb.setBuffer(&ba);
        L("[7][seq-QBuffer-unopened] bytesAvailable()=%lld size()=%lld",
          (long long)sb.bytesAvailable(), (long long)sb.size());
    }

    // ======================= Follow-ups ====================================
    // errorString() turned out to be the useless constant "Unknown error" in every
    // situation above; the machine-readable signal is QFileDevice::error().
    section("Follow-up A: QFileDevice::error() enum vs errorString()");
    {
        L("[A] enum: NoError=%d ReadError=%d WriteError=%d FatalError=%d ResourceError=%d "
          "OpenError=%d AbortError=%d TimeOutError=%d UnspecifiedError=%d PositionError=%d",
          (int)QFileDevice::NoError, (int)QFileDevice::ReadError, (int)QFileDevice::WriteError,
          (int)QFileDevice::FatalError, (int)QFileDevice::ResourceError,
          (int)QFileDevice::OpenError, (int)QFileDevice::AbortError,
          (int)QFileDevice::TimeOutError, (int)QFileDevice::UnspecifiedError,
          (int)QFileDevice::PositionError);
        makeFile();
        char buf[16];
        QFile f(g_path);
        f.open(QIODevice::ReadOnly);
        L("[A] fresh ReadOnly: error()=%d errorString=\"%s\"", (int)f.error(), cs(f.errorString()));
        f.seek(f.size());
        f.read(buf, 4);
        L("[A] after at-EOF read: error()=%d errorString=\"%s\"  <-- EOF is NOT an error", (int)f.error(),
          cs(f.errorString()));
        f.write("X", 1);
        L("[A] after write() on a ReadOnly file: error()=%d errorString=\"%s\"", (int)f.error(),
          cs(f.errorString()));
        f.unsetError();
        L("[A] after unsetError(): error()=%d errorString=\"%s\"", (int)f.error(),
          cs(f.errorString()));

        QFile missing("/definitely/does/not/exist/xyz");
        bool ok = missing.open(QIODevice::ReadOnly);
        L("[A] open() a missing file: ret=%s error()=%d errorString=\"%s\"", tf(ok),
          (int)missing.error(), cs(missing.errorString()));
    }

    section("Follow-up B: ungetChar() details -- pos, shadowing, unget below position 0");
    {
        makeFile();
        char buf[16];
        QFile f(g_path);
        f.open(QIODevice::ReadOnly);
        f.seek(f.size());
        L("[B] at EOF: pos=%lld atEnd=%s bytesAvailable=%lld", (long long)f.pos(), tf(f.atEnd()),
          (long long)f.bytesAvailable());
        f.ungetChar('Z');
        L("[B] ungetChar('Z') at EOF -> pos=%lld atEnd=%s bytesAvailable=%lld  <-- pos moved BACK "
          "one, and 'Z' now shadows the real byte at offset 9 ('9')",
          (long long)f.pos(), tf(f.atEnd()), (long long)f.bytesAvailable());
        memset(buf, 0, sizeof buf);
        qint64 r = f.read(buf, 4);
        L("[B] read(buf,4) -> ret=%lld data=\"%.*s\" pos=%lld  <-- the real byte '9' was NOT "
          "re-delivered",
          (long long)r, (int)(r > 0 ? r : 0), buf, (long long)f.pos());

        QFile g(g_path);
        g.open(QIODevice::ReadOnly);
        L("[B] fresh device at pos=0, calling ungetChar('Q')...");
        g.ungetChar('Q');
        L("[B] pos-after=%lld bytesAvailable=%lld", (long long)g.pos(),
          (long long)g.bytesAvailable());
        memset(buf, 0, sizeof buf);
        qint64 r2 = g.read(buf, 4);
        L("[B] then read(buf,4) ret=%lld data=\"%.*s\" pos=%lld", (long long)r2,
          (int)(r2 > 0 ? r2 : 0), buf, (long long)g.pos());

        QByteArray ba(DATA, 10);
        QBuffer b(&ba);
        b.open(QIODevice::ReadOnly);
        b.seek(b.size());
        b.ungetChar('Z');
        L("[B][QBuffer] ungetChar('Z') at EOF -> pos=%lld atEnd=%s bytesAvailable=%lld",
          (long long)b.pos(), tf(b.atEnd()), (long long)b.bytesAvailable());
    }

    section("Follow-up C: atEnd() is bytesAvailable()==0, including buffered data");
    {
        makeFile();
        QFile f(g_path);
        f.open(QIODevice::ReadOnly);
        f.seek(f.size());
        L("[C] pos==size: atEnd=%s bytesAvailable=%lld", tf(f.atEnd()),
          (long long)f.bytesAvailable());
        f.ungetChar('Z'); // now there IS buffered data even though pos<size again
        L("[C] after ungetChar: pos=%lld atEnd=%s", (long long)f.pos(), tf(f.atEnd()));
        char c;
        f.getChar(&c);
        L("[C] after consuming it: pos=%lld atEnd=%s  <-- atEnd flips purely on "
          "bytesAvailable()==0, never on 'a read already failed'",
          (long long)f.pos(), tf(f.atEnd()));
    }

    L("\n--- probe_qiodevice done ---");
    return 0;
}
