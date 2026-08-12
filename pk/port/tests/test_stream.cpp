// PkStream 的失败测试（先于 PkStream.h 存在而写）。用 pk/test（R-11）的
// PK_* 宏 + 真实的 PkTest::qExec 派发，不经 pk_test_moc.py：
//
// 生成器（pk_test_moc.py）只扫 .h，而本任务的 Files 清单里没有给测试类留一个
// 头文件；测试类成员干脆就是 public，不需要 Q_OBJECT/friend 那套访问权限
// 戏法，于是 PkTestBinder<PkStreamTestCase> 特化可以手写在这一个 .cpp 里
// ——形状和 pk_test_moc.py 会生成的东西完全一致（对照 pk/test/pk_test_moc.py
// 的 emit_binder()），只是不需要额外的 Python 代码生成构建步骤。
//
// PK_VERIFY/PK_COMPARE 依赖 PkTestCase::current() 的「当前测试函数」状态，
// 这个状态只有 PkTest::qExec 会正确维护（beginFunction/endFunction），所以
// 测试类必须真的走 qExec，不能绕开它直接调用测试方法。
#include "../PkStream.h"
#include "PkTest.h"

#include <cstring>

namespace {

// 最小的内存 PkStream 子类：只 override readData()/writeData()，其余全部
// 用基类的默认实现。非顺序设备——readData()/writeData() 按 pos() 索引自己的
// std::string，这正是头文件契约里「非顺序设备可以用 pos() 索引」那一支。
class MemoryStream : public PkStream
{
public:
    explicit MemoryStream(std::string data) : m_data(std::move(data)) {}

    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 avail = static_cast<pk_int64>(m_data.size()) - pos();
        if (avail <= 0) {
            return 0;   // EOF，不是错误。
        }
        const pk_int64 n = maxSize < avail ? maxSize : avail;
        std::memcpy(data, m_data.data() + pos(), static_cast<std::size_t>(n));
        return n;
    }

    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        const std::size_t p = static_cast<std::size_t>(pos());
        if (p + static_cast<std::size_t>(maxSize) > m_data.size()) {
            m_data.resize(p + static_cast<std::size_t>(maxSize));
        }
        std::memcpy(&m_data[p], data, static_cast<std::size_t>(maxSize));
        return maxSize;
    }

private:
    std::string m_data;
};

// 顺序设备演示：isSequential()==true，readData() 自己维护一个私有游标
// m_cursor，绝不看 pos()（pos() 对它恒为 0）。用来验证头文件那条硬契约—— 如果
// readData() 错误地用 pos() 索引，这里会立刻死循环/重复读到同一个字节，而不是
// 正确地把 5 个字节读完。不是 8 条硬断言之一，是额外加的契约回归测试。
class SequentialMemoryStream : public PkStream
{
public:
    explicit SequentialMemoryStream(std::string data) : m_data(std::move(data)), m_cursor(0) {}

    bool isSequential() const override { return true; }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 avail = static_cast<pk_int64>(m_data.size()) - m_cursor;
        if (avail <= 0) {
            return 0;
        }
        const pk_int64 n = maxSize < avail ? maxSize : avail;
        std::memcpy(data, m_data.data() + m_cursor, static_cast<std::size_t>(n));
        m_cursor += n;
        return n;
    }

    pk_int64 writeData(const char *, pk_int64) override { return -1; }

private:
    std::string m_data;
    pk_int64 m_cursor;
};

} // namespace

class PkStreamTestCase : public PkTestObject
{
public:
    // ① EOF 不是错误：read() 到末尾返回 0，不是 -1。
    void testEofReturnsZeroNotMinusOne();
    // ② 短读返回实际字节数，不补零。
    void testShortReadReturnsActualCount();
    // ③ peek 不动 pos，不足 n 返回剩余。
    void testPeekDoesNotMovePos();
    // ④ atEnd 是 bytesAvailable()==0。
    void testAtEndIsBytesAvailableZero();
    // ⑤ ungetChar 后 atEnd 翻回 false，且注入的字节遮蔽真实字节。
    void testUngetCharShadowsRealByte();
    // ⑥ skip 到 EOF 返回剩余、EOF 上返回 0。
    void testSkipToEofReturnsRemaining();
    // ⑦ readLine(buf, 0)/(buf, 1) 都返回 -1，且不动 pos。
    void testReadLineTinyMaxSizeReturnsMinusOne();
    // ⑧ 未 open 的设备 read()/write() 都返回 -1。
    void testUnopenedDeviceReadWriteReturnsMinusOne();
    // 额外：顺序设备契约（头注释规定的硬契约，非 8 条断言之一）。
    void testSequentialDevicePosStaysZero();
};

void PkStreamTestCase::testEofReturnsZeroNotMinusOne()
{
    MemoryStream dev("");
    dev.open(PkStream::ReadWrite);
    char buf[16];
    PK_COMPARE(dev.read(buf, 10), (PkStream::pk_int64)0);
}

void PkStreamTestCase::testShortReadReturnsActualCount()
{
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.seek(7);
    char buf[16];
    PK_COMPARE(dev.read(buf, 10), (PkStream::pk_int64)3);
}

void PkStreamTestCase::testPeekDoesNotMovePos()
{
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.seek(8);
    char buf[16];
    PK_COMPARE(dev.peek(buf, 10), (PkStream::pk_int64)2);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)8);
}

void PkStreamTestCase::testAtEndIsBytesAvailableZero()
{
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.seek(10);
    PK_VERIFY(dev.atEnd());
}

void PkStreamTestCase::testUngetCharShadowsRealByte()
{
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.seek(10);
    PK_VERIFY(dev.atEnd());
    dev.ungetChar('Z');
    PK_VERIFY(!dev.atEnd());
    char buf[16] = {};
    dev.read(buf, 4);
    PK_COMPARE(buf[0], 'Z');
}

void PkStreamTestCase::testSkipToEofReturnsRemaining()
{
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.seek(4);
    PK_COMPARE(dev.skip(100), (PkStream::pk_int64)6);
}

void PkStreamTestCase::testReadLineTinyMaxSizeReturnsMinusOne()
{
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    char buf[16];

    PK_COMPARE(dev.readLine(buf, 0), (PkStream::pk_int64)-1);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);

    PK_COMPARE(dev.readLine(buf, 1), (PkStream::pk_int64)-1);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);
}

void PkStreamTestCase::testUnopenedDeviceReadWriteReturnsMinusOne()
{
    MemoryStream closed("0123456789");   // 故意不 open()
    char buf[16];
    PK_COMPARE(closed.read(buf, 4), (PkStream::pk_int64)-1);
    PK_COMPARE(closed.write(buf, 4), (PkStream::pk_int64)-1);
}

void PkStreamTestCase::testSequentialDevicePosStaysZero()
{
    SequentialMemoryStream dev("hello");
    dev.open(PkStream::ReadOnly);
    PK_VERIFY(dev.isSequential());
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);

    char buf[8] = {};
    // 分两次读，若 readData() 错误地用 pos() 索引（对顺序设备恒为 0），
    // 第二次会重复读到 "he" 而不是往后推进读到 "llo"。
    PK_COMPARE(dev.read(buf, 2), (PkStream::pk_int64)2);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);   // 顺序设备上 pos 恒为 0
    PK_COMPARE(dev.read(buf + 2, 3), (PkStream::pk_int64)3);
    PK_VERIFY(std::memcmp(buf, "hello", 5) == 0);
    PK_COMPARE(dev.read(buf, 8), (PkStream::pk_int64)0);   // 读完之后 EOF
}

// PkTestBinder<PkStreamTestCase> 特化——手写，形状对照
// pk/test/pk_test_moc.py 的 emit_binder() 输出（见文件头注释）。
template <>
struct PkTestBinder<PkStreamTestCase> {
    static const char *className() { return "PkStreamTestCase"; }

    static const PkTestFunction *functions()
    {
        static const PkTestFunction fns[] = {
            {"testEofReturnsZeroNotMinusOne",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testEofReturnsZeroNotMinusOne(); },
             nullptr},
            {"testShortReadReturnsActualCount",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testShortReadReturnsActualCount(); },
             nullptr},
            {"testPeekDoesNotMovePos",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testPeekDoesNotMovePos(); },
             nullptr},
            {"testAtEndIsBytesAvailableZero",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testAtEndIsBytesAvailableZero(); },
             nullptr},
            {"testUngetCharShadowsRealByte",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testUngetCharShadowsRealByte(); },
             nullptr},
            {"testSkipToEofReturnsRemaining",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testSkipToEofReturnsRemaining(); },
             nullptr},
            {"testReadLineTinyMaxSizeReturnsMinusOne",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testReadLineTinyMaxSizeReturnsMinusOne(); },
             nullptr},
            {"testUnopenedDeviceReadWriteReturnsMinusOne",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testUnopenedDeviceReadWriteReturnsMinusOne(); },
             nullptr},
            {"testSequentialDevicePosStaysZero",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testSequentialDevicePosStaysZero(); },
             nullptr},
        };
        return fns;
    }
    static int count() { return 9; }

    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

int run_stream_tests(int argc, char **argv)
{
    PkStreamTestCase tc;
    return PkTest::qExec(&tc, argc, argv);
}
