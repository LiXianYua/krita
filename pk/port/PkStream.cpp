#include "PkStream.h"

#include "../container/PkByteArray.h"
#include "PkString.h"

#include <limits>
#include <vector>

PkStream::PkStream()
    : m_openMode(NotOpen)
    , m_pos(0)
{
}

// 基类故意不在析构里调用 close()——真 Qt 的 QIODevice 基类本身也不这么做
// （"隐式关闭"是具体子类的责任，见 QFileDevice）。R-12 只出端口，没有任何
// 子类需要这条行为，留给需要它的适配器（S-01 等）自己决定。
PkStream::~PkStream() = default;

void PkStream::setOpenMode(OpenMode mode)
{
    // open()/close() 都走这里：一次性把「打开」这件事的全部副作用收在一处——
    // 游标清零、unget 缓冲清空、错误文案清空。close() 之后再 open() 得到的是
    // 一个干净状态，而不是上一次 session 的残留。
    m_openMode = mode;
    m_pos = 0;
    m_ungetBuffer.clear();
    m_errorString.clear();
}

bool PkStream::open(OpenMode mode)
{
    setOpenMode(mode);
    return true;
}

void PkStream::close()
{
    setOpenMode(NotOpen);
}

bool PkStream::isOpen() const
{
    return m_openMode != NotOpen;
}

PkStream::OpenMode PkStream::openMode() const
{
    return m_openMode;
}

bool PkStream::isReadable() const
{
    return isOpen() && (m_openMode & ReadOnly) != 0;
}

bool PkStream::isWritable() const
{
    return isOpen() && (m_openMode & WriteOnly) != 0;
}

bool PkStream::isSequential() const
{
    return false;
}

PkStream::pk_int64 PkStream::size() const
{
    return 0;
}

PkStream::pk_int64 PkStream::pos() const
{
    // 顺序设备上恒为 0——见头文件的硬契约。m_pos 对顺序设备从来不会被挪动
    // （read()/ungetChar() 里全部挡在 !isSequential() 分支后面），这里的判断
    // 只是把契约钉死在 getter 上，不依赖调用方守规矩。
    return isSequential() ? 0 : m_pos;
}

bool PkStream::seek(pk_int64 pos)
{
    if (!isOpen() || isSequential() || pos < 0) {
        return false;
    }
    m_pos = pos;
    // seek 之后旧的 unget 缓冲已经对不上新位置了，必须扔掉——不扔的话下一次
    // read() 会先吐出 seek 之前 ungetChar() 进去的字节，读到的内容跟 seek 到
    // 的目标位置对不上。
    m_ungetBuffer.clear();
    return true;
}

bool PkStream::atEnd() const
{
    // 「atEnd 是 bytesAvailable()==0，不是『上次读失败了』」——探针 Follow-up C。
    // 但未 open 的设备是例外：真 Qt 在这种情况下无条件返回 true（探针 §3.7），
    // 即便 bytesAvailable() 仍在报告底层还有多少字节——这两者故意自相矛盾，
    // 不是本类能"修正"的语义分歧，原样对齐。
    return !isOpen() || bytesAvailable() == 0;
}

PkStream::pk_int64 PkStream::bytesAvailable() const
{
    // 顺序设备：unget 缓冲里的字节算「可用」，底层还剩多少基类无从得知
    // （管道/socket 这类东西没有「大小」的概念），只能靠子类自己 override
    // 这个函数把它加上——这是探针没有直接给出、但与 QFileDevice::bytesAvailable()
    // 的真实架构一致的默认实现（QFileDevice 也是在 QIODevice::bytesAvailable()
    // 之上叠加 size()-pos()），顺序分支目前只有一个演示性质的最小测试子类，
    // 没有真实调用点验证过。
    //
    // 非顺序设备：见下面 return 前的注释——不能重复计入 unget 缓冲。
    if (isSequential()) {
        return static_cast<pk_int64>(m_ungetBuffer.size());
    }
    // 非顺序设备：真 Qt 的公式就是 qMax(size()-pos(), 0)，unget 缓冲不再单独
    // 加一次——ungetChar() 已经把 m_pos 往回退了，remaining 里已经把 unget
    // 出来的字节算进去了。再加 m_ungetBuffer.size() 是重复计入（评审②，四格
    // 对照表：真 Qt 6/8/10/11，旧实现因为重复计入多算成 7/11/13/12）。
    const pk_int64 remaining = size() - pos();
    return remaining > 0 ? remaining : 0;
}

bool PkStream::canReadLine() const
{
    // 没有通用缓冲可扫描换行符，这里只能给一个保守近似：「还有字节可读」就
    // 认为可能能读到一行。真实的换行探测要么需要子类自己维护缓冲并 override
    // 这个函数，要么等 R-02 的 PkByteArray/字符串扫描能力到位。8 条硬断言里
    // 没有一条测 canReadLine()，这条是按 Qt 文档字面意思写的近似值，不是探针
    // 实测——报告里按「猜的」登记。
    return bytesAvailable() > 0;
}

PkStream::pk_int64 PkStream::read(char *data, pk_int64 maxSize)
{
    // 顺序必须是「maxSize<0 → -1」在最前，其次「未 open/不可读 → -1」，
    // maxSize==0 的短路排在两者之后——真 Qt 的 CHECK_MAXLEN 与
    // CHECK_READABLE 都先于 maxSize==0 判断（评审 I-2）。颠倒顺序会让
    // 「未 open 时 read(buf,0)」这类组合错误地先撞上 maxSize==0 而返回 0，
    // 而不是 -1。
    if (maxSize < 0) {
        return -1;
    }
    if (!isOpen() || !isReadable()) {
        // 未 open 的设备返回 -1（探针 §3.7 / 头注释①）。
        return -1;
    }
    if (maxSize == 0) {
        return 0;
    }

    pk_int64 total = 0;

    // 先吃 unget 缓冲：ungetChar() 注入的字节必须先于底层真实数据被读到，
    // 且不再重新触达 readData()。
    while (total < maxSize && !m_ungetBuffer.empty()) {
        data[total] = m_ungetBuffer.front();
        m_ungetBuffer.erase(m_ungetBuffer.begin());
        ++total;
        if (!isSequential()) {
            ++m_pos;
        }
    }

    // 剩余额度只转发**一次** readData()——不循环重试去补齐 maxSize，这是
    // 「短读返回实际字节数，不补零」（探针 §3.9 / 头注释②）的直接体现。
    if (total < maxSize) {
        const pk_int64 n = readData(data + total, maxSize - total);
        if (n > 0) {
            if (!isSequential()) {
                m_pos += n;
            }
            total += n;
        } else if (total == 0) {
            // 只有在 unget 缓冲什么都没给的时候，才把 readData() 的 0(EOF)/
            // 负值原样透传；已经从 unget 缓冲拿到过字节的话，这次 read()
            // 调用本身是成功的，不能因为底层紧接着 EOF 就报错或报 0。
            return n;
        }
    }
    return total;
}

PkByteArray PkStream::readAll()
{
    if (!isOpen() || !isReadable()) {
        return PkByteArray();
    }

    constexpr pk_int64 minimumChunkSize = 16384;
    std::vector<uint8_t> bytes;
    for (;;) {
        pk_int64 request = minimumChunkSize;
        if (!isSequential() && bytesAvailable() > request) {
            request = bytesAvailable();
        }
        const std::size_t oldSize = bytes.size();
        const std::size_t maxResult = static_cast<std::size_t>(std::numeric_limits<int>::max());
        if (request < 0 || static_cast<std::size_t>(request) > maxResult - oldSize) {
            setErrorString(PkString("readAll result exceeds PkByteArray capacity"));
            break;
        }
        bytes.resize(oldSize + static_cast<std::size_t>(request));
        const pk_int64 count = read(reinterpret_cast<char *>(bytes.data() + oldSize), request);
        if (count <= 0) {
            bytes.resize(oldSize);
            break;
        }
        bytes.resize(oldSize + static_cast<std::size_t>(count));
        if (!isSequential() && (count < request || atEnd())) {
            break;
        }
    }
    return PkByteArray(bytes);
}

PkStream::pk_int64 PkStream::peek(char *data, pk_int64 maxSize)
{
    // peek == 读一次再原样吐回去。read() 已经处理了 unget 缓冲/EOF/未 open
    // 的全部分支，这里只需要在读到东西之后按逆序把每个字节 ungetChar()
    // 回去——逐字节 ungetChar 是"后进先出"地插到缓冲最前面，倒着插一遍就把
    // 原始顺序还原了。read() 挪动过的 m_pos 也会被同样次数的 ungetChar()
    // 挪回去（一次 read 前进 1，一次 ungetChar 后退 1），净效果就是 pos()
    // 不变——不需要再手写一份「不动 pos」的逻辑。
    const pk_int64 n = read(data, maxSize);
    if (n <= 0) {
        return n;
    }
    for (pk_int64 i = n - 1; i >= 0; --i) {
        ungetChar(data[i]);
    }
    return n;
}

PkStream::pk_int64 PkStream::skip(pk_int64 maxSize)
{
    if (maxSize <= 0) {
        return 0;
    }
    if (!isOpen() || !isReadable()) {
        return -1;
    }
    // 没有「不搬数据只挪游标」的底层原语（那需要子类专门提供，本任务不出
    // 适配器），退化成分块读、扔掉——效果一致：真正读到底层 EOF 就停。
    char discard[4096];
    pk_int64 total = 0;
    while (total < maxSize) {
        pk_int64 chunk = maxSize - total;
        if (chunk > static_cast<pk_int64>(sizeof(discard))) {
            chunk = static_cast<pk_int64>(sizeof(discard));
        }
        const pk_int64 n = read(discard, chunk);
        if (n <= 0) {
            break;
        }
        total += n;
    }
    return total;
}

PkStream::pk_int64 PkStream::readLine(char *data, pk_int64 maxSize)
{
    // maxSize 含终止 '\0'（探针 §3.9）：小于等于 1 时连一个真实字符都放不下，
    // 直接返回 -1，且不碰设备（不调用 read()，pos 原地不动）。
    if (maxSize <= 1) {
        return -1;
    }

    const pk_int64 limit = maxSize - 1;   // 留一个位置给终止符
    pk_int64 count = 0;
    while (count < limit) {
        char c;
        const pk_int64 n = read(&c, 1);
        if (n <= 0) {
            if (count == 0) {
                // 一个字符都没读到：readLine 与 read 在 EOF 语义上不同——
                // read() 在 EOF 返回 0，但 readLine() 在 EOF 返回 -1（真 Qt
                // 实测：QBuffer "ab\n"/"ab"/空、QFile "ab\ncd" 四种情形全是
                // -1，评审③）。未 open 时 read() 本身就返回 -1，与这里统一
                // 返回 -1 并不冲突，所以不需要区分 n==0 还是 n==-1。
                data[0] = '\0';
                return -1;
            }
            break;
        }
        data[count++] = c;
        if (c == '\n') {
            break;
        }
    }
    data[count] = '\0';
    return count;
}

PkStream::pk_int64 PkStream::write(const char *data, pk_int64 maxSize)
{
    // 同 read()：maxSize<0 最先判，其次未 open/不可写，maxSize==0 排最后
    // （评审 I-2）——未 open 时 write(buf,0) 也要返回 -1，而不是被
    // maxSize==0 提前短路成 0。pos()<0 检查插在「可写」与「maxSize==0」
    // 之间（评审 Critical①，见下）。
    if (maxSize < 0) {
        return -1;
    }
    if (!isOpen() || !isWritable()) {
        return -1;
    }
    if (!isSequential() && m_pos < 0) {
        // pos()<0 只出现在 ReadWrite 设备 pos==0 上 ungetChar() 之后
        // （评审 C-2 放开的行为，允许 pos() 到 -1）。真 Qt 在这种情况下
        // write() 返回 -1、设备内容不变（内部 seek(d->pos) 失败，
        // stderr 打 "QBuffer::seek: Invalid pos: -1"，探针复核）——这条检查
        // 排在 maxSize==0 判断之前，因为真 Qt 连 write(buf,0) 在负 pos 上也
        // 返回 -1（评审 Critical①）。不查这一条的话，负 pos 会被 writeData()
        // 的实现直接拿去 static_cast<size_t> 索引，酿成内存破坏。
        return -1;
    }
    if (maxSize == 0) {
        return 0;
    }
    const pk_int64 n = writeData(data, maxSize);
    if (n > 0) {
        if (!isSequential()) {
            m_pos += n;
        }
        // write() 推进游标之后，旧的 unget 缓冲已经对不上新位置了：不扔掉的
        // 话下一次 read() 会先吐出 write() 之前 ungetChar()/peek() 留下的
        // 陈旧字节，再跳过刚写入的数据——与 seek() 里同一条注释是同一个根因
        // （评审 C-1）。
        m_ungetBuffer.clear();
    }
    return n;
}

bool PkStream::getChar(char *c)
{
    char ch = '\0';
    const pk_int64 n = read(&ch, 1);
    if (n != 1) {
        return false;
    }
    if (c) {
        *c = ch;
    }
    return true;
}

bool PkStream::putChar(char c)
{
    return write(&c, 1) == 1;
}

void PkStream::ungetChar(char c)
{
    // 插到最前面：最近一次 ungetChar() 的字节最先被下一次 read() 取走
    // （后进先出），与 peek() 靠"逆序 ungetChar 还原顺序"这个用法对应。
    m_ungetBuffer.insert(m_ungetBuffer.begin(), c);
    if (!isSequential()) {
        // 无条件 --m_pos，不再守 m_pos>0——真 Qt 允许 pos() 到 -1（探针
        // §3.8，评审 C-2）。守卫会让 unget（不减）与 read 消费 unget 字节
        // 时的 ++m_pos 不对称，游标净前进一格，从而在 pos()==0 时丢掉一个
        // 真实字节。read() 会先吃完 unget 缓冲才调用 readData()，届时
        // m_pos 已经被同等次数的 ++m_pos 加回 ≥0，readData() 不会拿到负
        // pos；peek() 从 pos=0 出发同样自洽（同上，read 内部会推回来）。
        // **这条只对 readData() 成立，对 writeData() 不成立**：write() 不
        // 经过 unget 缓冲，会拿当前 m_pos 直接调用 writeData()，所以负 pos
        // 必须在 write() 自己的入口挡掉（评审 Critical①，见 write() 实现），
        // 不能靠这里的注释兜底。
        --m_pos;
    }
}

PkString PkStream::errorString() const
{
    if (m_errorString.empty()) {
        return PkString("Unknown error");
    }
    return PkString::PkFromUtf8(m_errorString.c_str(), static_cast<int>(m_errorString.size()));
}

void PkStream::setErrorString(const PkString &str)
{
    m_errorString = str.PkToUtf8();
}
