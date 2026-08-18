// oracle.cpp —— pk/variant 与真 Qt5 的逐输入对拍。
//
// 架构：真 Qt 侧直接 #include <QVariant>，Pk 侧通过 C 桥接（pk_side.h）调用。
// 两侧头文件永不同时出现在同一个翻译单元里，彻底避免 qAbs/qRound 等的重定义冲突。
//
// 输出契约（run_oracle.sh 读这两种行）：
//     DIFF total=<N> mismatch=<M>      恰好一行，程序末尾打
//     DIFFTAG <api> <tag> <count>      一类差异一行
// 退出码必须是 0，即使 M>0。

#include <QVariant>
#include <QString>
#include <QStringList>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QByteArray>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QLine>
#include <QLineF>

#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

// 垫片一旦混进 -I，<QVariant> 会解析到 compat/QVariant，两侧变成同一个类型。
#if defined(QVariantList) || defined(QVariantHash) || defined(QVariantMap)
#  error "对拍两侧解析成了同一个类型 —— -I 里混进了 pk/variant/compat"
#endif

// Pk 侧 C 桥接
#include "pk_side.h"

// ═══ 计数与记录 ════════════════════════════════════════════════════════════

static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags;
static long g_printed = 0;

static void rec(const char* api, bool same, const std::string& tag)
{
    ++g_total;
    if (same) return;
    ++g_mismatch;
    ++g_tags[std::string(api) + " " + tag];
    if (g_printed < 40) {
        ++g_printed;
        std::printf("MISMATCH: %s [%s]\n", api, tag.c_str());
    }
}

// ═══ 比较原语 ══════════════════════════════════════════════════════════════

static bool same_double(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);
}

static std::string istr(int v) { return std::to_string(v); }
static std::string dstr(double d)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g", d);
    return buf;
}

// ═══ 输入集 ════════════════════════════════════════════════════════════════

static const int int_tokens[] = {
    -1, 0, 1, 42, 100, -5, -100, 255, 65535, -65536
};
static constexpr int n_int_tokens = sizeof(int_tokens) / sizeof(int_tokens[0]);

static const unsigned int uint_tokens[] = {
    0u, 1u, 42u, 100u, 4294967295u
};
static constexpr int n_uint_tokens = sizeof(uint_tokens) / sizeof(uint_tokens[0]);

static const long long ll_tokens[] = {
    0LL, 1LL, 42LL, -1LL, 999999999999LL
};
static constexpr int n_ll_tokens = sizeof(ll_tokens) / sizeof(ll_tokens[0]);

static const unsigned long long ull_tokens[] = {
    0ULL, 1ULL, 42ULL
};
static constexpr int n_ull_tokens = sizeof(ull_tokens) / sizeof(ull_tokens[0]);

static const double double_tokens[] = {
    0.0, 1.0, -1.0, 3.14159, 2.718, 3.7, 0.5, -0.0
};
static constexpr int n_double_tokens = sizeof(double_tokens) / sizeof(double_tokens[0]);

static const float float_tokens[] = {
    0.0f, 1.0f, -1.0f, 2.718f, 3.7f, 0.5f
};
static constexpr int n_float_tokens = sizeof(float_tokens) / sizeof(float_tokens[0]);

static const bool bool_tokens[] = { false, true };
static constexpr int n_bool_tokens = sizeof(bool_tokens) / sizeof(bool_tokens[0]);

static const char* string_tokens[] = {
    "", "hello", "123", "0", "-1", "42", "true", "false", "3.14", "abc"
};
static constexpr int n_string_tokens = sizeof(string_tokens) / sizeof(string_tokens[0]);

// ═══ 对拍函数 ═════════════════════════════════════════════════════════════

// ── 默认构造 ──────────────────────────────────────────────────────────────

static void cmp_default()
{
    QVariant q;
    void* p = pkvar_new();
    rec("default.isNull", q.isNull() == (pkvar_isNull(p) != 0), "isNull");
    rec("default.isValid", q.isValid() == (pkvar_isValid(p) != 0), "isValid");
    rec("default.type", q.type() == pkvar_type(p), "type=" + istr(q.type()));
    rec("default.userType", q.userType() == pkvar_userType(p), "userType");
    rec("default.toInt", q.toInt() == pkvar_toInt(p), "toInt=" + istr(q.toInt()));
    rec("default.toBool", q.toBool() == (pkvar_toBool(p) != 0), "toBool");
    rec("default.toDouble", same_double(q.toDouble(), pkvar_toDouble(p)), "toDouble");
    rec("default.toFloat", same_double(q.toFloat(), pkvar_toFloat(p)), "toFloat");
    rec("default.toUInt", q.toUInt() == pkvar_toUInt(p), "toUInt");
    rec("default.canConvertInt", q.canConvert<int>() == (pkvar_canConvertInt(p) != 0), "canConvertInt");
    rec("default.canConvertString", q.canConvert<QString>() == (pkvar_canConvertString(p) != 0), "canConvertString");
    rec("default.constData", (q.constData() != nullptr) == (pkvar_constData(p) != nullptr), "constData-nonnull");
    pkvar_delete(p);
}

// ── 基础类型 ──────────────────────────────────────────────────────────────

static void cmp_int_tokens()
{
    for (int i = 0; i < n_int_tokens; ++i) {
        int tok = int_tokens[i];
        QVariant q(tok);
        void* pk = pkvar_from_int(tok);
        char buf[64];
        std::snprintf(buf, sizeof buf, "int(%d)", tok);
        std::string tag(buf);

        rec("type", q.type() == pkvar_type(pk), tag);
        rec("isNull", q.isNull() == (pkvar_isNull(pk) != 0), tag);
        rec("isValid", q.isValid() == (pkvar_isValid(pk) != 0), tag);
        rec("toInt", q.toInt() == pkvar_toInt(pk), tag + "-toInt");
        rec("toBool", q.toBool() == (pkvar_toBool(pk) != 0), tag + "-toBool");
        rec("toDouble", same_double(q.toDouble(), pkvar_toDouble(pk)), tag + "-toDouble");
        rec("toFloat", same_double(q.toFloat(), pkvar_toFloat(pk)), tag + "-toFloat");
        rec("canConvertInt", q.canConvert<int>() == (pkvar_canConvertInt(pk) != 0), tag + "-canConvertInt");
        rec("canConvertBool", q.canConvert<bool>() == (pkvar_canConvertBool(pk) != 0), tag + "-canConvertBool");
        rec("canConvertString", q.canConvert<QString>() == (pkvar_canConvertString(pk) != 0), tag + "-canConvertString");

        QString qs = q.toString();
        const char* ps = pkvar_toString(pk);
        rec("toString", qs.toStdString() == std::string(ps), tag + "-toString");

        pkvar_delete(pk);
    }
}

static void cmp_uint_tokens()
{
    for (int i = 0; i < n_uint_tokens; ++i) {
        unsigned int tok = uint_tokens[i];
        QVariant q(tok);
        void* pk = pkvar_from_uint(tok);
        rec("type", q.type() == pkvar_type(pk), "uint");
        rec("toUInt", q.toUInt() == pkvar_toUInt(pk), "uint-toUInt");
        rec("toInt", q.toInt() == pkvar_toInt(pk), "uint-toInt");
        rec("toDouble", same_double(q.toDouble(), pkvar_toDouble(pk)), "uint-toDouble");
        pkvar_delete(pk);
    }
}

static void cmp_ll_tokens()
{
    for (int i = 0; i < n_ll_tokens; ++i) {
        long long tok = ll_tokens[i];
        QVariant q(tok);
        void* pk = pkvar_from_ll(tok);
        rec("type", q.type() == pkvar_type(pk), "ll");
        rec("toLongLong", q.toLongLong() == pkvar_toLongLong(pk), "ll-toLongLong");
        rec("toInt", q.toInt() == pkvar_toInt(pk), "ll-toInt");
        pkvar_delete(pk);
    }
}

static void cmp_ull_tokens()
{
    for (int i = 0; i < n_ull_tokens; ++i) {
        unsigned long long tok = ull_tokens[i];
        QVariant q(tok);
        void* pk = pkvar_from_ull(tok);
        rec("type", q.type() == pkvar_type(pk), "ull");
        rec("toULongLong", q.toULongLong() == pkvar_toULongLong(pk), "ull-toULongLong");
        rec("toInt", q.toInt() == pkvar_toInt(pk), "ull-toInt");
        pkvar_delete(pk);
    }
}

static void cmp_double_tokens()
{
    for (int i = 0; i < n_double_tokens; ++i) {
        double tok = double_tokens[i];
        QVariant q(tok);
        void* pk = pkvar_from_double(tok);
        char buf[64];
        std::snprintf(buf, sizeof buf, "double(%.17g)", tok);
        std::string tag(buf);

        rec("type", q.type() == pkvar_type(pk), tag);
        rec("toInt", q.toInt() == pkvar_toInt(pk), tag + "-toInt");
        rec("toBool", q.toBool() == (pkvar_toBool(pk) != 0), tag + "-toBool");
        rec("toDouble", same_double(q.toDouble(), pkvar_toDouble(pk)), tag + "-toDouble");
        rec("isNull", q.isNull() == (pkvar_isNull(pk) != 0), tag);
        rec("isValid", q.isValid() == (pkvar_isValid(pk) != 0), tag);
        pkvar_delete(pk);
    }
}

static void cmp_float_tokens()
{
    for (int i = 0; i < n_float_tokens; ++i) {
        float tok = float_tokens[i];
        QVariant q(tok);
        void* pk = pkvar_from_float(tok);
        rec("type", q.type() == pkvar_type(pk), "float");
        rec("toFloat", same_double(q.toFloat(), pkvar_toFloat(pk)), "float-toFloat");
        rec("toDouble", same_double(q.toDouble(), pkvar_toDouble(pk)), "float-toDouble");
        rec("toInt", q.toInt() == pkvar_toInt(pk), "float-toInt");
        rec("canConvertInt", q.canConvert<int>() == (pkvar_canConvertInt(pk) != 0), "float-canConvertInt");
        rec("canConvertDouble", q.canConvert<double>() == (pkvar_canConvertDouble(pk) != 0), "float-canConvertDouble");
        rec("canConvertString", q.canConvert<QString>() == (pkvar_canConvertString(pk) != 0), "float-canConvertString");
        pkvar_delete(pk);
    }
}

static void cmp_bool_tokens()
{
    for (int i = 0; i < n_bool_tokens; ++i) {
        bool tok = bool_tokens[i];
        QVariant q(tok);
        void* pk = pkvar_from_bool(tok ? 1 : 0);
        rec("type", q.type() == pkvar_type(pk), "bool");
        rec("toInt", q.toInt() == pkvar_toInt(pk), "bool-toInt");
        rec("toBool", q.toBool() == (pkvar_toBool(pk) != 0), "bool-toBool");
        rec("toDouble", same_double(q.toDouble(), pkvar_toDouble(pk)), "bool-toDouble");
        rec("canConvertInt", q.canConvert<int>() == (pkvar_canConvertInt(pk) != 0), "bool-canConvertInt");
        rec("canConvertString", q.canConvert<QString>() == (pkvar_canConvertString(pk) != 0), "bool-canConvertString");
        {
            QString qs = q.toString();
            const char* ps = pkvar_toString(pk);
            rec("toString", qs.toStdString() == std::string(ps), "bool-toString");
        }
        pkvar_delete(pk);
    }
}

static void cmp_string_tokens()
{
    for (int i = 0; i < n_string_tokens; ++i) {
        const char* tok = string_tokens[i];
        QVariant q{QString(tok)};
        void* pk = pkvar_from_string(tok);
        char buf[128];
        std::snprintf(buf, sizeof buf, "string(%s)", tok[0] ? tok : "<empty>");
        std::string tag(buf);

        rec("type", q.type() == pkvar_type(pk), tag);
        rec("isNull", q.isNull() == (pkvar_isNull(pk) != 0), tag + "-isNull");
        rec("isValid", q.isValid() == (pkvar_isValid(pk) != 0), tag + "-isValid");
        rec("toInt", q.toInt() == pkvar_toInt(pk), tag + "-toInt");
        rec("toBool", q.toBool() == (pkvar_toBool(pk) != 0), tag + "-toBool");
        rec("toDouble", same_double(q.toDouble(), pkvar_toDouble(pk)), tag + "-toDouble");
        rec("canConvertInt", q.canConvert<int>() == (pkvar_canConvertInt(pk) != 0), tag + "-canConvertInt");
        rec("canConvertBool", q.canConvert<bool>() == (pkvar_canConvertBool(pk) != 0), tag + "-canConvertBool");
        rec("canConvertDouble", q.canConvert<double>() == (pkvar_canConvertDouble(pk) != 0), tag + "-canConvertDouble");
        rec("canConvertString", q.canConvert<QString>() == (pkvar_canConvertString(pk) != 0), tag + "-canConvertString");

        QString qs = q.toString();
        const char* ps = pkvar_toString(pk);
        rec("toString", qs.toStdString() == std::string(ps), tag + "-toString");

        pkvar_delete(pk);
    }
}

// ── 几何类型（仅测试 type() 等基本查询）──────────────────────────────────

static void cmp_point()
{
    QPoint qp(5, 10);
    QVariant q(qp);
    rec("point.type", q.type() == QVariant::Point, "point");
    // Can't easily construct PkPoint from C bridge, but we can test canConvert
    void* pk = pkvar_new();
    rec("point.canConvertInt", q.canConvert<int>() == (pkvar_canConvertInt(pk) != 0), "point-canConvertInt");
    pkvar_delete(pk);
}

static void cmp_pointf()
{
    QPointF qp(1.5, 2.5);
    QVariant q(qp);
    rec("pointf.type", q.type() == QVariant::PointF, "pointf");
}

static void cmp_rect()
{
    QRect qr(1, 2, 3, 4);
    QVariant q(qr);
    rec("rect.type", q.type() == QVariant::Rect, "rect");
}

static void cmp_rectf()
{
    QRectF qr(1.0, 2.0, 3.0, 4.0);
    QVariant q(qr);
    rec("rectf.type", q.type() == QVariant::RectF, "rectf");
}

static void cmp_size()
{
    QSize qs(10, 20);
    QVariant q(qs);
    rec("size.type", q.type() == QVariant::Size, "size");
}

static void cmp_sizef()
{
    QSizeF qs(10.0, 20.0);
    QVariant q(qs);
    rec("sizef.type", q.type() == QVariant::SizeF, "sizef");
}

static void cmp_line()
{
    QLine ql(0, 1, 2, 3);
    QVariant q(ql);
    rec("line.type", q.type() == QVariant::Line, "line");
}

static void cmp_linef()
{
    QLineF ql(0.0, 1.0, 2.0, 3.0);
    QVariant q(ql);
    rec("linef.type", q.type() == QVariant::LineF, "linef");
}

// ── 时间类型 ──────────────────────────────────────────────────────────────

static void cmp_date()
{
    QDate qd(2024, 1, 15);
    QVariant q(qd);
    rec("date.type", q.type() == QVariant::Date, "date");
}

static void cmp_time()
{
    QTime qt(12, 30, 45);
    QVariant q(qt);
    rec("time.type", q.type() == QVariant::Time, "time");
}

static void cmp_datetime()
{
    QDateTime qdt = QDateTime{QDate{2024, 1, 15}, QTime{12, 30, 45}};
    QVariant q(qdt);
    rec("datetime.type", q.type() == QVariant::DateTime, "datetime");
}

// ── 集合类型 ──────────────────────────────────────────────────────────────

static void cmp_list()
{
    QVariantList ql;
    ql.append(QVariant(1));
    ql.append(QVariant(2));
    ql.append(QVariant(3));
    QVariant q(ql);
    rec("list.type", q.type() == QVariant::List, "list");
    rec("list.toList.size", q.toList().size() == 3, "list-size");
}

static void cmp_hash()
{
    QVariantHash qh;
    qh["a"] = QVariant(1);
    qh["b"] = QVariant(2);
    QVariant q(qh);
    rec("hash.type", q.type() == QVariant::Hash, "hash");
}

static void cmp_map()
{
    QVariantMap qm;
    qm["key"] = QVariant(42);
    QVariant q(qm);
    rec("map.type", q.type() == QVariant::Map, "map");
}

// ── setValue / clear ──────────────────────────────────────────────────────

static void cmp_setValue()
{
    {
        QVariant q;
        q.setValue(42);
        void* pk = pkvar_new();
        pkvar_setValueInt(pk, 42);
        rec("setValue.type-int", q.type() == pkvar_type(pk), "setValue-int-type");
        rec("setValue.toInt", q.toInt() == pkvar_toInt(pk), "setValue-int-toInt");
        rec("setValue.isNull", q.isNull() == (pkvar_isNull(pk) != 0), "setValue-int-isNull");
        pkvar_delete(pk);
    }

    {
        QVariant q;
        q.setValue(42);
        q.setValue(QString("world"));
        void* pk = pkvar_new();
        pkvar_setValueInt(pk, 42);
        pkvar_setValueString(pk, "world");
        rec("setValue.type-string", q.type() == pkvar_type(pk), "setValue-string-type");
        QString qs = q.toString();
        const char* ps = pkvar_toString(pk);
        rec("setValue.toString", qs.toStdString() == std::string(ps), "setValue-string-toString");
        pkvar_delete(pk);
    }
}

static void cmp_clear()
{
    {
        QVariant q;
        q.clear();
        void* pk = pkvar_new();
        pkvar_clear(pk);
        rec("clear.isNull", q.isNull() == (pkvar_isNull(pk) != 0), "clear-default-isNull");
        rec("clear.isValid", q.isValid() == (pkvar_isValid(pk) != 0), "clear-default-isValid");
        pkvar_delete(pk);
    }

    {
        QVariant q(42);
        q.clear();
        void* pk = pkvar_from_int(42);
        pkvar_clear(pk);
        rec("clear.type", q.type() == pkvar_type(pk), "clear-after-int-type");
        rec("clear.isNull", q.isNull() == (pkvar_isNull(pk) != 0), "clear-after-int-isNull");
        rec("clear.isValid", q.isValid() == (pkvar_isValid(pk) != 0), "clear-after-int-isValid");
        rec("clear.toInt", q.toInt() == pkvar_toInt(pk), "clear-after-int-toInt");
        pkvar_delete(pk);
    }
}

// ── 拷贝与移动 ────────────────────────────────────────────────────────────

static void cmp_copy_move()
{
    // Copy
    {
        QVariant q1(99);
        QVariant q2 = q1;
        void* pk1 = pkvar_from_int(99);
        void* pk2 = pkvar_copy(pk1);
        rec("copy.toInt", q2.toInt() == pkvar_toInt(pk2), "copy-toInt");
        rec("copy.orig.toInt", q1.toInt() == pkvar_toInt(pk1), "copy-orig-toInt");
        pkvar_delete(pk1);
        pkvar_delete(pk2);
    }
}

// ── 边界用例 ──────────────────────────────────────────────────────────────

static void cmp_edge_cases()
{
    // -5 toUInt
    {
        QVariant q(-5);
        void* pk = pkvar_from_int(-5);
        rec("edge.-5.toUInt", q.toUInt() == pkvar_toUInt(pk), "neg5-toUInt");
        rec("edge.-5.toBool", q.toBool() == (pkvar_toBool(pk) != 0), "neg5-toBool");
        pkvar_delete(pk);
    }

    // 999999999999 toInt
    {
        QVariant q(999999999999LL);
        void* pk = pkvar_from_ll(999999999999LL);
        rec("edge.999.toInt", q.toInt() == pkvar_toInt(pk), "big-toInt");
        pkvar_delete(pk);
    }

    // double(3.7) toInt = 4
    {
        QVariant q(3.7);
        void* pk = pkvar_from_double(3.7);
        rec("edge.3.7.toInt", q.toInt() == pkvar_toInt(pk), "3.7-toInt");
        rec("edge.3.7.toBool", q.toBool() == (pkvar_toBool(pk) != 0), "3.7-toBool");
        pkvar_delete(pk);
    }

    // float(3.7f) toInt = 4
    {
        QVariant q(3.7f);
        void* pk = pkvar_from_float(3.7f);
        rec("edge.3.7f.toInt", q.toInt() == pkvar_toInt(pk), "3.7f-toInt");
        pkvar_delete(pk);
    }

    // double(0.0) toBool = false
    {
        QVariant q(0.0);
        void* pk = pkvar_from_double(0.0);
        rec("edge.0.0.toBool", q.toBool() == (pkvar_toBool(pk) != 0), "0.0-toBool");
        pkvar_delete(pk);
    }

    // double(0.5) toBool = true
    {
        QVariant q(0.5);
        void* pk = pkvar_from_double(0.5);
        rec("edge.0.5.toBool", q.toBool() == (pkvar_toBool(pk) != 0), "0.5-toBool");
        pkvar_delete(pk);
    }

    // NaN
    {
        QVariant q(NAN);
        void* pk = pkvar_from_double(NAN);
        rec("nan.toInt", q.toInt() == pkvar_toInt(pk), "nan-toInt");
        rec("nan.toBool", q.toBool() == (pkvar_toBool(pk) != 0), "nan-toBool");
        rec("nan.isNull", q.isNull() == (pkvar_isNull(pk) != 0), "nan-isNull");
        rec("nan.isValid", q.isValid() == (pkvar_isValid(pk) != 0), "nan-isValid");
        pkvar_delete(pk);
    }

    // Inf
    {
        double inf = INFINITY;
        QVariant q(inf);
        void* pk = pkvar_from_double(inf);
        rec("inf.toInt", q.toInt() == pkvar_toInt(pk), "inf-toInt");
        rec("inf.toBool", q.toBool() == (pkvar_toBool(pk) != 0), "inf-toBool");
        rec("inf.isNull", q.isNull() == (pkvar_isNull(pk) != 0), "inf-isNull");
        rec("inf.isValid", q.isValid() == (pkvar_isValid(pk) != 0), "inf-isValid");
        pkvar_delete(pk);
    }
}

// ═══ main ══════════════════════════════════════════════════════════════════

int main()
{
    std::printf("=== PkVariant vs QVariant Oracle ===\n\n");

    std::printf("--- default construction ---\n");
    cmp_default();

    std::printf("--- int tokens ---\n");
    cmp_int_tokens();
    std::printf("--- uint tokens ---\n");
    cmp_uint_tokens();
    std::printf("--- long long tokens ---\n");
    cmp_ll_tokens();
    std::printf("--- unsigned long long tokens ---\n");
    cmp_ull_tokens();
    std::printf("--- double tokens ---\n");
    cmp_double_tokens();
    std::printf("--- float tokens ---\n");
    cmp_float_tokens();
    std::printf("--- bool tokens ---\n");
    cmp_bool_tokens();
    std::printf("--- string tokens ---\n");
    cmp_string_tokens();

    std::printf("--- point ---\n");
    cmp_point();
    std::printf("--- pointf ---\n");
    cmp_pointf();
    std::printf("--- rect ---\n");
    cmp_rect();
    std::printf("--- rectf ---\n");
    cmp_rectf();
    std::printf("--- size ---\n");
    cmp_size();
    std::printf("--- sizef ---\n");
    cmp_sizef();
    std::printf("--- line ---\n");
    cmp_line();
    std::printf("--- linef ---\n");
    cmp_linef();

    std::printf("--- date ---\n");
    cmp_date();
    std::printf("--- time ---\n");
    cmp_time();
    std::printf("--- datetime ---\n");
    cmp_datetime();

    std::printf("--- list ---\n");
    cmp_list();
    std::printf("--- hash ---\n");
    cmp_hash();
    std::printf("--- map ---\n");
    cmp_map();

    std::printf("--- setValue ---\n");
    cmp_setValue();
    std::printf("--- clear ---\n");
    cmp_clear();

    std::printf("--- copy/move ---\n");
    cmp_copy_move();

    std::printf("--- edge cases ---\n");
    cmp_edge_cases();

    std::printf("\nDIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    for (const auto& kv : g_tags) {
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    }

    return 0;
}