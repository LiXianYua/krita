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
#include "../../container/PkByteArray.h"
#include "../../string/PkString.h"

#include <cstring>
#include <algorithm>

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

class ScriptedStream : public PkStream
{
public:
    ScriptedStream(std::string data, bool sequential, pk_int64 cap, pk_int64 failAfter = -1)
        : m_data(std::move(data)), m_sequential(sequential), m_cap(cap), m_failAfter(failAfter) {}
    bool isSequential() const override { return m_sequential; }
    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }
    pk_int64 calls() const { return m_calls; }
    pk_int64 requested() const { return m_requested; }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        ++m_calls;
        m_requested += maxSize;
        if (m_failAfter >= 0 && m_cursor >= m_failAfter) {
            setErrorString(PkString("scripted-error"));
            return -1;
        }
        const pk_int64 remaining = static_cast<pk_int64>(m_data.size()) - m_cursor;
        if (remaining <= 0) return 0;
        pk_int64 n = std::min({maxSize, m_cap, remaining});
        if (m_failAfter >= 0) n = std::min(n, m_failAfter - m_cursor);
        std::memcpy(data, m_data.data() + m_cursor, static_cast<std::size_t>(n));
        m_cursor += n;
        return n;
    }
    pk_int64 writeData(const char *, pk_int64) override { return -1; }

private:
    std::string m_data;
    bool m_sequential;
    pk_int64 m_cap;
    pk_int64 m_failAfter;
    pk_int64 m_cursor = 0;
    pk_int64 m_calls = 0;
    pk_int64 m_requested = 0;
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

    // ── R-12 评审补测（Task 1+2 评审 C-1/C-2/I-1/I-2 + 4 条回归） ──────────
    // C-1：write() 之后 unget 缓冲必须作废，不能吐陈旧字节。
    void testWriteInvalidatesUngetBuffer();
    // C-2：pos()==0 时 ungetChar() 不能丢真实字节（去掉 m_pos>0 守卫）。
    void testUngetCharAtPosZero();
    // I-1：未 open 的设备 atEnd() 恒为 true。
    void testAtEndTrueWhenUnopened();
    // I-2：maxSize<0 与「未 open 时 maxSize==0」都要返回 -1，顺序见评审表格。
    void testMaxSizeEdgeCasesMatchRealQt();
    // 回归①：ungetChar 连续多次按 LIFO 顺序被下一次 read() 取走。
    void testMultipleUngetCharLifoOrder();
    // 回归②：peek 跨越 unget 缓冲与底层数据的边界。
    void testPeekAcrossUngetBufferAndUnderlyingData();
    // 回归③：readLine 遇到 '\n' 截断、末尾无 '\n' 到 EOF 时返回 0。
    void testReadLineNewlineAndEof();
    // 回归④：write 到只读设备返回 -1。
    void testWriteToReadOnlyDeviceReturnsMinusOne();

    // ── R-12 Task 3 复评修复（这一轮自己引入的 3 条：Critical① + Important②③） ──
    // Critical①：pos()<0 时 write()/putChar() 必须返回 -1，且不能把负 pos
    // 递给 writeData()（否则 writeData() 里 static_cast<size_t> 会内存破坏）。
    void testWriteAtNegativePosReturnsMinusOne();
    // Important②：非顺序设备的 bytesAvailable() 不能重复计入 unget 缓冲——
    // 真 Qt 表格四行：seek(5)+1unget / seek(5)+3unget / peek(3) / pos==0 上 unget。
    void testBytesAvailableDoesNotDoubleCountUngetForNonSequential();
    // Important③：readLine 在 EOF 返回 -1（不是 0），真 Qt 四种情形全覆盖：
    // 末尾带 '\n' / 末尾不带 '\n' / 空设备 / 多行读完后。
    void testReadLineEofReturnsMinusOneAllScenarios();
    void testReadAllRandomAccessStopsOnPositiveShortRead();
    void testReadAllSequentialAccumulatesShortReadsToEof();
    void testReadAllEofAndImmediateErrorReturnEmpty();
    void testReadAllSequentialPartialErrorKeepsPrefix();
};

void PkStreamTestCase::testReadAllRandomAccessStopsOnPositiveShortRead()
{
    ScriptedStream dev("abcdef", false, 2);
    dev.open(PkStream::ReadOnly);
    const PkByteArray bytes = dev.readAll();
    PK_COMPARE(bytes.size(), 2);
    PK_VERIFY(std::memcmp(bytes.constData(), "ab", 2) == 0);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)2);
    PK_VERIFY(!dev.atEnd());
    PK_COMPARE(dev.calls(), (PkStream::pk_int64)1);
}

void PkStreamTestCase::testReadAllSequentialAccumulatesShortReadsToEof()
{
    ScriptedStream dev("abcdef", true, 2);
    dev.open(PkStream::ReadOnly);
    const PkByteArray bytes = dev.readAll();
    PK_COMPARE(bytes.size(), 6);
    PK_VERIFY(std::memcmp(bytes.constData(), "abcdef", 6) == 0);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);
    // Three capped reads produce the payload; the fourth read must probe EOF.
    PK_COMPARE(dev.calls(), (PkStream::pk_int64)4);
    PK_COMPARE(dev.requested(), (PkStream::pk_int64)(4 * 16384));
    PK_VERIFY(dev.atEnd());
}

void PkStreamTestCase::testReadAllEofAndImmediateErrorReturnEmpty()
{
    ScriptedStream empty("", false, 2);
    empty.open(PkStream::ReadOnly);
    PK_COMPARE(empty.readAll().size(), 0);
    PK_COMPARE(empty.errorString().PkToUtf8(), std::string("Unknown error"));
    ScriptedStream unopened("abcdef", false, 2);
    PK_COMPARE(unopened.readAll().size(), 0);
    PK_COMPARE(unopened.calls(), (PkStream::pk_int64)0);
    ScriptedStream error("abcdef", false, 2, 0);
    error.open(PkStream::ReadOnly);
    PK_COMPARE(error.readAll().size(), 0);
    PK_COMPARE(error.errorString().PkToUtf8(), std::string("scripted-error"));
}

void PkStreamTestCase::testReadAllSequentialPartialErrorKeepsPrefix()
{
    ScriptedStream dev("abcdef", true, 2, 3);
    dev.open(PkStream::ReadOnly);
    const PkByteArray bytes = dev.readAll();
    PK_COMPARE(bytes.size(), 3);
    PK_VERIFY(std::memcmp(bytes.constData(), "abc", 3) == 0);
    PK_COMPARE(dev.errorString().PkToUtf8(), std::string("scripted-error"));
}

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

    // 评审台账 minor：bytesAvailable() 的顺序设备分支
    // （`return m_ungetBuffer.size();`）此前没有任何断言覆盖——改成
    // `return 0;` 测试照样全绿。顺序设备上 unget 缓冲里的字节就是「可用」，
    // 这里直接验证：先确认没有 unget 时是 0，ungetChar 两次后必须变成 2。
    SequentialMemoryStream dev4("xyz");
    dev4.open(PkStream::ReadOnly);
    PK_COMPARE(dev4.bytesAvailable(), (PkStream::pk_int64)0);
    dev4.ungetChar('B');
    dev4.ungetChar('A');
    PK_COMPARE(dev4.bytesAvailable(), (PkStream::pk_int64)2);
}

void PkStreamTestCase::testWriteInvalidatesUngetBuffer()
{
    // 场景：ReadWrite 内存设备 "0123456789"，peek(buf,3) 后 write("XY",2)。
    // 真 Qt：pos=2 bytesAvailable=8，随后 read(buf,4)="2345"（评审 C-1）。
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    char peekBuf[8];
    PK_COMPARE(dev.peek(peekBuf, 3), (PkStream::pk_int64)3);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);

    PK_COMPARE(dev.write("XY", 2), (PkStream::pk_int64)2);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)2);
    PK_COMPARE(dev.bytesAvailable(), (PkStream::pk_int64)8);

    char buf[8] = {};
    PK_COMPARE(dev.read(buf, 4), (PkStream::pk_int64)4);
    PK_VERIFY(std::memcmp(buf, "2345", 4) == 0);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)6);
}

void PkStreamTestCase::testUngetCharAtPosZero()
{
    // 场景：pos=0 的设备 "0123456789"，ungetChar('Q') 后 read(buf,4)。
    // 真 Qt：pos 变 -1；read → "Q012"，pos=3（探针 §3.8，评审 C-2）。
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);

    dev.ungetChar('Q');
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)-1);

    char buf[8] = {};
    PK_COMPARE(dev.read(buf, 4), (PkStream::pk_int64)4);
    PK_VERIFY(std::memcmp(buf, "Q012", 4) == 0);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)3);
}

void PkStreamTestCase::testAtEndTrueWhenUnopened()
{
    // 探针 §3.7：未 open 的设备 atEnd()==true，即便 bytesAvailable() 仍报
    // 「底层还有多少字节」——这两者故意自相矛盾，评审 I-1。
    MemoryStream dev("0123456789");   // 故意不 open()
    PK_VERIFY(dev.atEnd());
    PK_COMPARE(dev.bytesAvailable(), (PkStream::pk_int64)10);
}

void PkStreamTestCase::testMaxSizeEdgeCasesMatchRealQt()
{
    // 评审 I-2 表格四行：maxSize<0 恒 -1；未 open 时 maxSize==0 也是 -1
    // （CHECK_MAXLEN 与 CHECK_READABLE/CHECK_WRITABLE 都排在 maxSize==0
    // 短路之前）。
    MemoryStream opened("0123456789");
    opened.open(PkStream::ReadWrite);
    char buf[4];
    PK_COMPARE(opened.read(buf, -1), (PkStream::pk_int64)-1);
    PK_COMPARE(opened.write(buf, -1), (PkStream::pk_int64)-1);

    MemoryStream closed("0123456789");   // 故意不 open()
    PK_COMPARE(closed.read(buf, 0), (PkStream::pk_int64)-1);
    PK_COMPARE(closed.write(buf, 0), (PkStream::pk_int64)-1);
}

void PkStreamTestCase::testMultipleUngetCharLifoOrder()
{
    // 回归：连续 ungetChar('A')('B')('C') 之后，read() 按 "CBA" 的顺序吐出
    // ——最近一次 unget 的字节最先被读到（后进先出）。挑 pos=5（非 0）以避免
    // 和 C-2 的 pos 归零行为耦合，这条只验证顺序本身。
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.seek(5);
    dev.ungetChar('A');
    dev.ungetChar('B');
    dev.ungetChar('C');

    char buf[8] = {};
    PK_COMPARE(dev.read(buf, 3), (PkStream::pk_int64)3);
    PK_VERIFY(std::memcmp(buf, "CBA", 3) == 0);
}

void PkStreamTestCase::testPeekAcrossUngetBufferAndUnderlyingData()
{
    // 回归：unget 缓冲里只有 1 个字节，peek(4) 要跨过它继续读底层数据，
    // 且 peek 完 pos 要和 peek 之前一致。
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.seek(3);
    dev.ungetChar('Y');
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)2);

    char buf[8] = {};
    PK_COMPARE(dev.peek(buf, 4), (PkStream::pk_int64)4);
    PK_VERIFY(std::memcmp(buf, "Y345", 4) == 0);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)2);
}

void PkStreamTestCase::testReadLineNewlineAndEof()
{
    // 回归：readLine 遇到 '\n' 就截断（含 '\n' 本身）；没有 '\n' 时读到 EOF
    // 停止；再读一次（已经 EOF）返回 0。
    MemoryStream dev("ab\ncd");
    dev.open(PkStream::ReadWrite);
    char buf[16];

    PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)3);
    PK_VERIFY(std::memcmp(buf, "ab\n", 3) == 0);

    PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)2);
    PK_VERIFY(std::memcmp(buf, "cd", 2) == 0);

    PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)-1);
}

void PkStreamTestCase::testWriteToReadOnlyDeviceReturnsMinusOne()
{
    // 回归：write 到 ReadOnly 设备返回 -1（isWritable() 为 false）。
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadOnly);
    char buf[4] = {'A', 'B', 'C', 'D'};
    PK_COMPARE(dev.write(buf, 4), (PkStream::pk_int64)-1);
}

void PkStreamTestCase::testWriteAtNegativePosReturnsMinusOne()
{
    // 场景：ReadWrite 设备在 pos==0 上 ungetChar()，pos 变 -1（评审 C-2 允许
    // 的行为），随后 write()/putChar()。真 Qt：write 返回 -1，设备内容不变、
    // pos 不变（探针复核：QBuffer::seek: Invalid pos: -1，评审 Critical①）。
    MemoryStream dev("0123456789");
    dev.open(PkStream::ReadWrite);
    dev.ungetChar('Q');
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)-1);

    char wbuf[4] = {'W', 'X', 'Y', 'Z'};
    PK_COMPARE(dev.write(wbuf, 4), (PkStream::pk_int64)-1);
    PK_COMPARE(dev.pos(), (PkStream::pk_int64)-1);   // 设备位置不变

    // 底层数据没有被破坏：还能正常读到 unget 遮蔽字节 + 原始数据开头。
    char rbuf[4] = {};
    PK_COMPARE(dev.read(rbuf, 4), (PkStream::pk_int64)4);
    PK_VERIFY(std::memcmp(rbuf, "Q012", 4) == 0);

    // putChar 走同一条 write() 路径，同样必须返回 false（评审 Critical①同源）。
    MemoryStream dev2("abcdef");
    dev2.open(PkStream::ReadWrite);
    dev2.ungetChar('Z');
    PK_COMPARE(dev2.pos(), (PkStream::pk_int64)-1);
    PK_VERIFY(!dev2.putChar('X'));
    PK_COMPARE(dev2.pos(), (PkStream::pk_int64)-1);

    // 评审台账 minor：write() 的实现里 pos()<0 检查特意排在 maxSize==0 判断
    // 之前（write() 注释「真 Qt 连 write(buf,0) 在负 pos 上也返回 -1」），但
    // 此前没有断言真的用 maxSize==0 触发过这条路径——把检查顺序换回
    // "maxSize==0 先短路成 0" 这里也会全绿。0 字节写在负 pos 上必须仍是 -1，
    // 不能被 maxSize==0 提前短路成 0。
    MemoryStream dev3("abcdef");
    dev3.open(PkStream::ReadWrite);
    dev3.ungetChar('Y');
    PK_COMPARE(dev3.pos(), (PkStream::pk_int64)-1);
    char zeroBuf[1] = {'Z'};
    PK_COMPARE(dev3.write(zeroBuf, 0), (PkStream::pk_int64)-1);
    PK_COMPARE(dev3.pos(), (PkStream::pk_int64)-1);
}

void PkStreamTestCase::testBytesAvailableDoesNotDoubleCountUngetForNonSequential()
{
    // 真 Qt 四格对照（探针复核，评审②）：非顺序设备的 bytesAvailable() 就是
    // max(size()-pos(),0)，pos() 回退已经把 unget 出来的字节体现过了，不能
    // 再靠 + m_ungetBuffer.size() 重复计入一次。
    {
        // seek(5) + 1 次 unget → 真 Qt 6（旧实现因为重复计入算成 7）。
        MemoryStream dev("0123456789");
        dev.open(PkStream::ReadWrite);
        dev.seek(5);
        dev.ungetChar('X');
        PK_COMPARE(dev.bytesAvailable(), (PkStream::pk_int64)6);
    }
    {
        // seek(5) + 3 次 unget → 真 Qt 8（旧实现算成 11）。
        MemoryStream dev("0123456789");
        dev.open(PkStream::ReadWrite);
        dev.seek(5);
        dev.ungetChar('X');
        dev.ungetChar('Y');
        dev.ungetChar('Z');
        PK_COMPARE(dev.bytesAvailable(), (PkStream::pk_int64)8);
    }
    {
        // peek(3) 之后（pos 仍是 0，peek 内部靠 read+ungetChar 实现，会在
        // unget 缓冲里留下 3 个字节）→ 真 Qt 10（旧实现算成 13）。
        MemoryStream dev("0123456789");
        dev.open(PkStream::ReadWrite);
        char pbuf[3];
        PK_COMPARE(dev.peek(pbuf, 3), (PkStream::pk_int64)3);
        PK_COMPARE(dev.pos(), (PkStream::pk_int64)0);
        PK_COMPARE(dev.bytesAvailable(), (PkStream::pk_int64)10);
    }
    {
        // pos==0 上 unget（评审 C-2 的场景）→ 真 Qt 11（旧实现算成 12）。
        MemoryStream dev("0123456789");
        dev.open(PkStream::ReadWrite);
        dev.ungetChar('Q');
        PK_COMPARE(dev.pos(), (PkStream::pk_int64)-1);
        PK_COMPARE(dev.bytesAvailable(), (PkStream::pk_int64)11);
    }
}

void PkStreamTestCase::testReadLineEofReturnsMinusOneAllScenarios()
{
    // 真 Qt 实测（探针复核，评审③）：readLine 在 EOF 返回 -1，同一设备上
    // read() 在 EOF 仍返回 0——两者在这一点上语义不同。四种情形全覆盖：
    // 末尾带 '\n' / 末尾不带 '\n' / 空设备 / 多行读完之后。
    char buf[16];
    {
        MemoryStream dev("ab\n");
        dev.open(PkStream::ReadWrite);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)3);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)-1);
        PK_COMPARE(dev.read(buf, 16), (PkStream::pk_int64)0);
    }
    {
        MemoryStream dev("ab");
        dev.open(PkStream::ReadWrite);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)2);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)-1);
        PK_COMPARE(dev.read(buf, 16), (PkStream::pk_int64)0);
    }
    {
        MemoryStream dev("");
        dev.open(PkStream::ReadWrite);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)-1);
        PK_COMPARE(dev.read(buf, 16), (PkStream::pk_int64)0);
    }
    {
        MemoryStream dev("ab\ncd");
        dev.open(PkStream::ReadWrite);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)3);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)2);
        PK_COMPARE(dev.readLine(buf, 16), (PkStream::pk_int64)-1);
        PK_COMPARE(dev.read(buf, 16), (PkStream::pk_int64)0);
    }
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
            {"testWriteInvalidatesUngetBuffer",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testWriteInvalidatesUngetBuffer(); },
             nullptr},
            {"testUngetCharAtPosZero",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testUngetCharAtPosZero(); },
             nullptr},
            {"testAtEndTrueWhenUnopened",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testAtEndTrueWhenUnopened(); },
             nullptr},
            {"testMaxSizeEdgeCasesMatchRealQt",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testMaxSizeEdgeCasesMatchRealQt(); },
             nullptr},
            {"testMultipleUngetCharLifoOrder",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testMultipleUngetCharLifoOrder(); },
             nullptr},
            {"testPeekAcrossUngetBufferAndUnderlyingData",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testPeekAcrossUngetBufferAndUnderlyingData(); },
             nullptr},
            {"testReadLineNewlineAndEof",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testReadLineNewlineAndEof(); },
             nullptr},
            {"testWriteToReadOnlyDeviceReturnsMinusOne",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testWriteToReadOnlyDeviceReturnsMinusOne(); },
             nullptr},
            {"testWriteAtNegativePosReturnsMinusOne",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testWriteAtNegativePosReturnsMinusOne(); },
             nullptr},
            {"testBytesAvailableDoesNotDoubleCountUngetForNonSequential",
             [](PkTestObject *o) {
                 static_cast<PkStreamTestCase *>(o)->testBytesAvailableDoesNotDoubleCountUngetForNonSequential();
             },
             nullptr},
            {"testReadLineEofReturnsMinusOneAllScenarios",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testReadLineEofReturnsMinusOneAllScenarios(); },
             nullptr},
            {"testReadAllRandomAccessStopsOnPositiveShortRead",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testReadAllRandomAccessStopsOnPositiveShortRead(); },
             nullptr},
            {"testReadAllSequentialAccumulatesShortReadsToEof",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testReadAllSequentialAccumulatesShortReadsToEof(); },
             nullptr},
            {"testReadAllEofAndImmediateErrorReturnEmpty",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testReadAllEofAndImmediateErrorReturnEmpty(); },
             nullptr},
            {"testReadAllSequentialPartialErrorKeepsPrefix",
             [](PkTestObject *o) { static_cast<PkStreamTestCase *>(o)->testReadAllSequentialPartialErrorKeepsPrefix(); },
             nullptr},
        };
        return fns;
    }
    static int count() { return 24; }

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
