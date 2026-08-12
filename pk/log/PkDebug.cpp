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
    if (_s && _s.use_count() == 1 && !_s->silent) {
        std::string text = _s->ts.str();
        while (!text.empty() && text.back() == ' ') {
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
