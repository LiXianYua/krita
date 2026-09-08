#include "csv_read_line.h"

#include "PkStream.h"
#include "PkString.h"
#include "PkTest.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

namespace {

class CsvMemoryStream : public PkStream
{
public:
    explicit CsvMemoryStream(std::string data, pk_int64 cap = 1024, pk_int64 failAfter = -1)
        : m_data(std::move(data))
        , m_cap(cap)
        , m_failAfter(failAfter)
    {
    }

    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        if (m_failAfter >= 0 && pos() >= m_failAfter) {
            setErrorString(PkString("scripted-error"));
            return -1;
        }
        const pk_int64 remaining = static_cast<pk_int64>(m_data.size()) - pos();
        if (remaining <= 0) {
            return 0;
        }
        pk_int64 count = std::min({maxSize, m_cap, remaining});
        if (m_failAfter >= 0) {
            count = std::min(count, m_failAfter - pos());
        }
        std::memcpy(data, m_data.data() + pos(), static_cast<std::size_t>(count));
        return count;
    }

    pk_int64 writeData(const char *, pk_int64) override { return -1; }

private:
    std::string m_data;
    pk_int64 m_cap;
    pk_int64 m_failAfter;
};

} // namespace

class CsvReadLineTestCase : public PkTestObject
{
public:
    void testQuotedAndUnquotedBinaryCrLfFields();
    void testTextCrLfAndEmptyFinalRecord();
    void testShortReadsAndDeviceErrors();
};

void CsvReadLineTestCase::testQuotedAndUnquotedBinaryCrLfFields()
{
    CsvMemoryStream stream("plain,\"quoted\"\r\n");
    stream.open(PkStream::ReadOnly);
    CSVReadLine reader;
    PK_COMPARE(reader.nextLine(&stream), 1);

    PkString field;
    PK_VERIFY(reader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string("plain"));
    PK_VERIFY(reader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string("quoted"));
    PK_COMPARE(reader.nextLine(&stream), 0);
}

void CsvReadLineTestCase::testTextCrLfAndEmptyFinalRecord()
{
    CsvMemoryStream stream("plain,\"quoted\"\r\n\r\n");
    stream.open(PkStream::ReadOnly | PkStream::Text);
    CSVReadLine reader;

    PK_COMPARE(reader.nextLine(&stream), 1);
    PkString field;
    PK_VERIFY(reader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string("plain"));
    PK_VERIFY(reader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string("quoted"));

    // The final newline-only physical record is distinct from the following
    // null/empty EOF result, and CSV exposes it as one empty field.
    PK_COMPARE(reader.nextLine(&stream), 1);
    PK_VERIFY(reader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string());
    PK_COMPARE(reader.nextLine(&stream), 0);
}

void CsvReadLineTestCase::testShortReadsAndDeviceErrors()
{
    CsvMemoryStream shortReads("raw,\"value\"\r\n", 1);
    shortReads.open(PkStream::ReadOnly);
    CSVReadLine reader;
    PK_COMPARE(reader.nextLine(&shortReads), 1);
    PkString field;
    PK_VERIFY(reader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string("raw"));
    PK_VERIFY(reader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string("value"));

    CsvMemoryStream partial("prefix-tail", 1, 6);
    partial.open(PkStream::ReadOnly);
    CSVReadLine partialReader;
    PK_COMPARE(partialReader.nextLine(&partial), 1);
    PK_VERIFY(partialReader.nextField(&field));
    PK_COMPARE(field.PkToUtf8(), std::string("prefix"));
    PK_COMPARE(partialReader.nextLine(&partial), -1);
    PK_COMPARE(partial.errorString().PkToUtf8(), std::string("scripted-error"));

    CsvMemoryStream immediate("ignored", 1, 0);
    immediate.open(PkStream::ReadOnly);
    CSVReadLine immediateReader;
    PK_COMPARE(immediateReader.nextLine(&immediate), -1);
}

template <>
struct PkTestBinder<CsvReadLineTestCase> {
    static const char *className() { return "CsvReadLineTestCase"; }
    static const PkTestFunction *functions()
    {
        static const PkTestFunction fns[] = {
            {"testQuotedAndUnquotedBinaryCrLfFields",
             [](PkTestObject *o) { static_cast<CsvReadLineTestCase *>(o)->testQuotedAndUnquotedBinaryCrLfFields(); },
             nullptr},
            {"testTextCrLfAndEmptyFinalRecord",
             [](PkTestObject *o) { static_cast<CsvReadLineTestCase *>(o)->testTextCrLfAndEmptyFinalRecord(); },
             nullptr},
            {"testShortReadsAndDeviceErrors",
             [](PkTestObject *o) { static_cast<CsvReadLineTestCase *>(o)->testShortReadsAndDeviceErrors(); },
             nullptr},
        };
        return fns;
    }
    static int count() { return 3; }
    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

int run_csv_read_line_tests(int argc, char **argv)
{
    CsvReadLineTestCase tc;
    return PkTest::qExec(&tc, argc, argv);
}
