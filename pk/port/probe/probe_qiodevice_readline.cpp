// Independent Qt 5.15 oracle for the PkStream::readLine() convenience API.
// This file intentionally links Qt and is never part of the Pk build.

#include <QBuffer>
#include <QIODevice>
#include <QtGlobal>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

class ScriptedDevice final : public QIODevice
{
public:
    ScriptedDevice(QByteArray data, qint64 cap, qint64 failAfter = -1)
        : m_data(std::move(data))
        , m_cap(cap)
        , m_failAfter(failAfter)
    {
    }

    bool isSequential() const override { return true; }
    qint64 calls() const { return m_calls; }

    qint64 bytesAvailable() const override
    {
        return std::max<qint64>(m_data.size() - m_cursor, 0) + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        ++m_calls;
        if (m_failAfter >= 0 && m_cursor >= m_failAfter) {
            setErrorString(QStringLiteral("scripted-error"));
            return -1;
        }
        const qint64 remaining = m_data.size() - m_cursor;
        if (remaining <= 0) {
            return 0;
        }
        qint64 count = std::min({maxSize, m_cap, remaining});
        if (m_failAfter >= 0) {
            count = std::min(count, m_failAfter - m_cursor);
        }
        std::memcpy(data, m_data.constData() + m_cursor, static_cast<std::size_t>(count));
        m_cursor += count;
        return count;
    }

    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QByteArray m_data;
    qint64 m_cap;
    qint64 m_failAfter;
    qint64 m_cursor = 0;
    qint64 m_calls = 0;
};

void printResult(const char *name, const QByteArray &value, const QIODevice &device)
{
    std::printf("%s\tsize=%d\tnull=%d\thex=%s\terror=%s\n",
                name,
                value.size(),
                value.isNull() ? 1 : 0,
                value.toHex().constData(),
                device.errorString().toUtf8().constData());
}

void runBufferCase(const char *name, const QByteArray &input, QIODevice::OpenMode mode)
{
    QBuffer device;
    device.setData(input);
    device.open(mode);
    printResult(name, device.readLine(), device);
}

} // namespace

int main()
{
    std::printf("qt_runtime=%s\tqt_compiled=%s\n", qVersion(), QT_VERSION_STR);

    runBufferCase("csv-unquoted-binary-crlf", "plain,42\r\nrest", QIODevice::ReadOnly);
    runBufferCase("csv-quoted-binary-crlf", "\"quoted\",\"42\"\r\nrest", QIODevice::ReadOnly);
    runBufferCase("csv-unquoted-text-crlf", "plain,42\r\nrest", QIODevice::ReadOnly | QIODevice::Text);
    runBufferCase("csv-quoted-text-crlf", "\"quoted\",\"42\"\r\nrest", QIODevice::ReadOnly | QIODevice::Text);

    {
        QBuffer device;
        device.setData("a\r\nb\rc\n");
        device.open(QIODevice::ReadOnly | QIODevice::Text);
        char buffer[16] = {};
        const qint64 size = device.read(buffer, sizeof(buffer));
        std::printf("text-read-mixed\tsize=%lld\tpos=%lld\thex=%s\n",
                    static_cast<long long>(size),
                    static_cast<long long>(device.pos()),
                    QByteArray(buffer, size > 0 ? static_cast<int>(size) : 0).toHex().constData());
    }

    {
        QBuffer device;
        device.setData("\r\nx");
        device.open(QIODevice::ReadOnly | QIODevice::Text);
        char byte = 0;
        const qint64 size = device.read(&byte, 1);
        std::printf("text-crlf-read-one\tsize=%lld\tpos=%lld\thex=%s\n",
                    static_cast<long long>(size),
                    static_cast<long long>(device.pos()),
                    QByteArray(&byte, size > 0 ? static_cast<int>(size) : 0).toHex().constData());
        check(size == 1, "Text QBuffer read(1) at CRLF start returns one translated byte");
        check(byte == '\n', "Text QBuffer read(1) at CRLF start returns LF");
        check(device.pos() == 2, "Text QBuffer read(1) at CRLF start consumes CRLF");
    }

    {
        QBuffer device;
        device.setData("\r\nx");
        device.open(QIODevice::ReadOnly | QIODevice::Text);
        const qint64 skipped = device.skip(1);
        const qint64 posAfterSkip = device.pos();
        char byte = 0;
        const qint64 size = device.read(&byte, 1);
        std::printf("text-crlf-skip-one\tskipped=%lld\tpos_after_skip=%lld\tafter_size=%lld\tafter_hex=%s\n",
                    static_cast<long long>(skipped),
                    static_cast<long long>(posAfterSkip),
                    static_cast<long long>(size),
                    QByteArray(&byte, size > 0 ? static_cast<int>(size) : 0).toHex().constData());
        check(skipped == 1, "Text QBuffer skip(1) at CRLF start skips one translated byte");
        check(posAfterSkip == 2, "Text QBuffer skip(1) at CRLF start consumes CRLF");
        check(size == 1 && byte == 'x', "Text QBuffer skip(1) at CRLF start leaves x next");
        check(device.pos() == 3, "Text QBuffer skip(1) then read(1) consumes all input");
    }

    {
        QBuffer device;
        device.setData("a\r\nb");
        device.open(QIODevice::ReadOnly | QIODevice::Text);
        char buffer[2] = {};
        const qint64 firstSize = device.read(buffer, sizeof(buffer));
        std::printf("text-read-two-first\tsize=%lld\tpos=%lld\thex=%s\n",
                    static_cast<long long>(firstSize),
                    static_cast<long long>(device.pos()),
                    QByteArray(buffer, firstSize > 0 ? static_cast<int>(firstSize) : 0).toHex().constData());
        const qint64 secondSize = device.read(buffer, sizeof(buffer));
        std::printf("text-read-two-second\tsize=%lld\tpos=%lld\thex=%s\n",
                    static_cast<long long>(secondSize),
                    static_cast<long long>(device.pos()),
                    QByteArray(buffer, secondSize > 0 ? static_cast<int>(secondSize) : 0).toHex().constData());
    }

    {
        QBuffer device;
        device.setData("a\r\nb");
        device.open(QIODevice::ReadOnly | QIODevice::Text);
        const QByteArray value = device.peek(2);
        std::printf("text-peek-two\tsize=%d\tpos=%lld\thex=%s\n",
                    value.size(),
                    static_cast<long long>(device.pos()),
                    value.toHex().constData());
    }

    {
        QBuffer device;
        device.setData("row\n\n");
        device.open(QIODevice::ReadOnly);
        printResult("empty-final-first", device.readLine(), device);
        printResult("empty-final-record", device.readLine(), device);
        printResult("empty-final-eof", device.readLine(), device);
    }

    {
        ScriptedDevice device("short\r\nrest", 1);
        device.open(QIODevice::ReadOnly);
        printResult("sequential-short-read", device.readLine(), device);
    }

    {
        ScriptedDevice device("a\r\nb", 1);
        device.open(QIODevice::ReadOnly | QIODevice::Text);
        char buffer[4] = {};
        for (int i = 1; i <= 4; ++i) {
            const qint64 size = device.read(buffer, 1);
            std::printf("sequential-text-read-%d\tsize=%lld\tcalls=%lld\thex=%s\n",
                        i,
                        static_cast<long long>(size),
                        static_cast<long long>(device.calls()),
                        QByteArray(buffer, size > 0 ? static_cast<int>(size) : 0).toHex().constData());
            if (i == 2) {
                check(size == 0, "Text sequential positive short CR read preserves zero-output boundary");
            }
        }
    }
    {
        ScriptedDevice device("ignored", 1, 0);
        device.open(QIODevice::ReadOnly);
        printResult("immediate-error", device.readLine(), device);
    }

    {
        ScriptedDevice device("prefix-tail", 1, 6);
        device.open(QIODevice::ReadOnly);
        printResult("partial-error-first", device.readLine(), device);
        printResult("partial-error-second", device.readLine(), device);
    }

    {
        QBuffer device;
        device.setData("a\r\nb\r\n");
        device.open(QIODevice::ReadOnly | QIODevice::Text);
        char buffer[8] = {};
        const qint64 firstSize = device.readLine(buffer, sizeof(buffer));
        std::printf("char-buffer-text-first\tsize=%lld\thex=%s\n",
                    static_cast<long long>(firstSize),
                    QByteArray(buffer, firstSize > 0 ? static_cast<int>(firstSize) : 0).toHex().constData());
        printResult("char-buffer-text-second", device.readLine(), device);
    }

    return failures == 0 ? 0 : 1;
}
