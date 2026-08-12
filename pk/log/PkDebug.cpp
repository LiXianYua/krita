#include "PkDebug.h"

#include "PkLogBackend.h"

PkDebug::PkDebug(PkLogLevel level, const PkLogContext &ctx) : _s(std::make_shared<Impl>())
{
    _s->level = level;
    _s->ctx = ctx;
}

PkDebug::~PkDebug()
{
    // 共享状态的最后一个持有者析构时才 flush——use_count()==1 意味着此刻
    // 只有*this*一个引用（还没走到 shared_ptr 自身的成员析构那一步）。
    //
    // use_count() 只在单线程持有下才是可靠的 flush 判据：它是"读后再减"，
    // 不是原子的读-改-写。真 Qt 用的是引用计数的原子递减（QAtomicInt），
    // 无竞态；这里如果同一个 PkDebug 的副本被两个线程各持一份并发析构，
    // 两边都可能读到 use_count()==2、都不 flush，整行日志丢失。现实中一条
    // `qDebug() << ...` 表达式产生的副本不会跨线程传递（都在同一条语句里
    // 生成、使用、析构），所以不要求改成原子实现；但 PkLogSink.cpp 是明确
    // 按多线程设计的（g_mutex 保护注册表），这个不一致值得记在这——评审
    // Minor 项。
    if (_s && _s.use_count() == 1 && !_s->silent) {
        std::string text = _s->ts.str();
        // 评审 Important 项：真 Qt 的规则是"最多砍一个尾随空格，且仅当
        // space 标志（析构时的当前值）为真"，不是"无条件砍掉全部尾随空格"。
        // 探针实测（见 task-8-report.md）：`nospace() << "a  "` 保留两个
        // 字面尾随空格原样不动（space 为假，完全不砍）；
        // `qSetFieldWidth(6) << 1 << 2` 因为 writeSeparator() 把分隔符也纳入
        // 了字段宽度格式化，行尾会累出多个空格，真 Qt 也只砍其中一个。
        if (_s->space && !text.empty() && text.back() == ' ') {
            text.pop_back();
        }
        PkLogEmit(_s->level, _s->ctx, text);
    }
}

void PkDebug::applyFieldWidth()
{
    if (_s->fieldWidth != 0) {
        _s->ts.width(_s->fieldWidth);
    }
}

void PkDebug::writeSeparator()
{
    applyFieldWidth();
    _s->ts << ' ';
}

PkDebug &PkDebug::space()
{
    _s->space = true;
    writeSeparator();
    return *this;
}

PkDebug &PkDebug::nospace()
{
    _s->space = false;
    return *this;
}

PkDebug &PkDebug::quote()
{
    _s->quote = true;
    return *this;
}

PkDebug &PkDebug::noquote()
{
    _s->quote = false;
    return *this;
}

PkDebug &PkDebug::maybeSpace()
{
    if (_s->space) {
        writeSeparator();
    }
    return *this;
}

PkDebug &PkDebug::operator<<(const char *value)
{
    applyFieldWidth();
    _s->ts << (value ? value : "");
    return maybeSpace();
}

PkDebug &PkDebug::operator<<(char value)
{
    applyFieldWidth();
    _s->ts << value;
    return maybeSpace();
}

PkDebug &PkDebug::operator<<(bool value)
{
    applyFieldWidth();
    _s->ts << (value ? "true" : "false");
    return maybeSpace();
}

// 操纵符：只改状态，不写内容——不经过 maybeSpace，不产生分隔符。
PkDebug &PkDebug::operator<<(PkDebugFieldWidth w)
{
    _s->fieldWidth = w.width;
    return *this;
}

PkDebug &PkDebug::operator<<(PkDebugPadChar p)
{
    _s->ts.fill(p.ch);
    return *this;
}

PkDebug &PkDebug::operator<<(PkDebugRealNumberPrecision p)
{
    _s->ts.precision(p.precision);
    return *this;
}

PkDebug PkDebugMakeForTest(PkLogLevel level, const char *category)
{
    PkLogContext ctx;
    ctx.category = category;
    return PkDebug(level, ctx);
}

PkDebug PkDebugMakeSilent()
{
    PkDebug d(PkLogDebug, PkLogContext{});
    d._s->silent = true;
    return d;
}
