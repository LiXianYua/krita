// ba_oracle.cpp —— PkByteArray 与真 Qt5 QByteArray 的逐输入对拍。
//
// 架构：真 Qt 侧直接 #include <QByteArray>，Pk 侧通过 C 桥接（ba_pk_side.h）调用。
// 两侧头文件永不同时出现在同一个翻译单元里，避免 qAbs/qRound 等的重定义冲突。
// 对拍源 include 路径绝不能给 compat/（见 run_ba_oracle.sh）。
//
// 输出契约（run_ba_oracle.sh 读这两种行）：
//     DIFF total=<N> mismatch=<M>      恰好一行，程序末尾打
//     DIFFTAG <api> <tag> <count>      一类差异一行
// 退出码必须是 0，即使 M>0。
//
// 对拍目标：0 偏离。谓词就是精确相等（size + 字节逐位），不放宽任何一档。

#include <QByteArray>

#include <type_traits>

// 防垫片合并两侧：若 -I 误混进 compat/ 且 compat/QByteArray 把 QByteArray 映射
// 成 PkByteArray，则这里两个名字解析成同一个类型 → static_assert 编译期炸掉
// （否则 oracle 会无声退化成自比，mismatch 恒 0）。无垫片时 PkByteArray 只是
// 前置声明（不完整类型也可进 is_same），QByteArray 是真 Qt 类型，断言必然通过。
class PkByteArray;
static_assert(!std::is_same<QByteArray, PkByteArray>::value,
              "oracle 两侧解析成了同一个类型 —— -I 里混进了 compat/");

#include "ba_pk_side.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

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

// ═══ 比较原语：精确相等（size + 字节逐位） ═══════════════════════════════════

static bool ba_same(const QByteArray& q, const char* pk, int pksize)
{
    if (q.size() != pksize) return false;
    return std::memcmp(q.constData(), pk, static_cast<size_t>(pksize)) == 0;
}

static std::string itag(const char* what, int a, int b)
{
    return std::string(what) + "(" + std::to_string(a) + "," + std::to_string(b) + ")";
}
static std::string utag(const char* what, unsigned int a, int b)
{
    return std::string(what) + "(" + std::to_string(a) + "," + std::to_string(b) + ")";
}

// ═══ 输入集 ════════════════════════════════════════════════════════════════

struct BaseStr { const char* data; int len; const char* name; };
static const BaseStr base_strings[] = {
    { "", 0, "empty" },
    { "a", 1, "a" },
    { "hello", 5, "hello" },
    { "\x00\x01\x02", 3, "nul3" },
};
static const int resize_sizes[] = { -5, -1, 0, 1, 2, 5, 100 };

// ═══ 对拍函数 ═════════════════════════════════════════════════════════════

// ── resize ────────────────────────────────────────────────────────────────
// 每个基串 × 每个 size：resize 后比 size/isEmpty，再把新 size 写满已知值
// （验证 data() 可变、buffer 有效）后比全量。
//
// ⚠ 填充前**不比裸内容**：Qt 5.15 的 QByteArray::resize 在扩张（n>size）时
// 新字节是 malloc 未初始化残留（探针实测：default->resize(5) 首字节读到上一步
// 释放的 'a'），不保证补 0。PkByteArray 侧按 brief 设计用 std::vector::resize
// 确定性补 0——这是比 Qt 更确定的行为，不是偏离。oracle 只对拍**确定性**可观测
// 契约：resize 后的 size/isEmpty，以及写满后的全量内容。
static void cmp_resize()
{
    for (const BaseStr& bs : base_strings) {
        for (int n : resize_sizes) {
            QByteArray q(bs.data, bs.len);
            void* pk = pkb_from_data(bs.data, bs.len);
            q.resize(n);
            pkb_resize(pk, n);
            std::string tag = std::string("resize/") + bs.name + "/n=" + std::to_string(n);

            rec("resize.size", q.size() == pkb_size(pk), tag + "-size");
            rec("resize.isEmpty", q.isEmpty() == (pkb_isEmpty(pk) != 0), tag + "-isEmpty");

            if (n > 0) {
                for (int i = 0; i < n; ++i) {
                    char c = static_cast<char>((i * 31 + 7) & 0x7f);
                    q[i] = c;
                    pkb_set_byte(pk, i, c);
                }
                rec("resize.fill", ba_same(q, pkb_constData(pk), pkb_size(pk)), tag + "-fill");
                rec("resize.fill.size", q.size() == pkb_size(pk), tag + "-fill-size");
            }
            pkb_delete(pk);
        }
    }
}

// ── data / constData ─────────────────────────────────────────────────────
static void cmp_data()
{
    // 空对象：constData() 非空且 byte0==0
    {
        QByteArray q;
        void* pk = pkb_new();
        rec("data.empty.size", q.size() == pkb_size(pk), "data-empty-size");
        rec("data.empty.isEmpty", q.isEmpty() == (pkb_isEmpty(pk) != 0), "data-empty-isEmpty");
        rec("data.empty.constDataNonnull",
            (q.constData() != nullptr) && (pkb_constData(pk) != nullptr),
            "data-empty-constData-nonnull");
        rec("data.empty.constDataByte0",
            static_cast<unsigned char>(q.constData()[0])
                == static_cast<unsigned char>(pkb_constData(pk)[0]),
            "data-empty-constData-byte0");
        pkb_delete(pk);
    }
    for (const BaseStr& bs : base_strings) {
        QByteArray q(bs.data, bs.len);
        void* pk = pkb_from_data(bs.data, bs.len);
        std::string tag = std::string("data/") + bs.name;
        rec("data.size", q.size() == pkb_size(pk), tag + "-size");
        rec("data.isEmpty", q.isEmpty() == (pkb_isEmpty(pk) != 0), tag + "-isEmpty");
        rec("data.content", ba_same(q, pkb_constData(pk), pkb_size(pk)), tag + "-content");
        pkb_delete(pk);
    }
}

// ── number ────────────────────────────────────────────────────────────────
static void cmp_number()
{
    const int ints[] = { INT_MIN, -255, -1, 0, 1, 42, 255, 0x7fffffff };
    const int bases[] = { 2, 8, 10, 16 };
    for (int n : ints) {
        for (int base : bases) {
            QByteArray q = QByteArray::number(n, base);
            void* pk = pkb_number(n, base);
            rec("number.int", ba_same(q, pkb_constData(pk), pkb_size(pk)), itag("int", n, base));
            pkb_delete(pk);
        }
    }
    const unsigned int us[] = { 0u, 1u, 255u, 0x7fffffffu, 0xffffffffu, 0xdeadbeefu };
    for (unsigned int u : us) {
        for (int base : bases) {
            QByteArray q = QByteArray::number(u, base);
            void* pk = pkb_number_uint(u, base);
            rec("number.uint", ba_same(q, pkb_constData(pk), pkb_size(pk)), utag("uint", u, base));
            pkb_delete(pk);
        }
    }
}

// ── 构造 与 等值 ──────────────────────────────────────────────────────────
static void cmp_ctor_equals()
{
    // 构造：(nullptr,0)、("abc",3)、("",0)
    {
        QByteArray q(nullptr, 0);
        void* pk = pkb_from_data(nullptr, 0);
        rec("ctor.null0.size", q.size() == pkb_size(pk), "ctor-null0-size");
        rec("ctor.null0.content", ba_same(q, pkb_constData(pk), pkb_size(pk)), "ctor-null0-content");
        pkb_delete(pk);
    }
    {
        QByteArray q("abc", 3);
        void* pk = pkb_from_data("abc", 3);
        rec("ctor.abc3.size", q.size() == pkb_size(pk), "ctor-abc3-size");
        rec("ctor.abc3.content", ba_same(q, pkb_constData(pk), pkb_size(pk)), "ctor-abc3-content");
        pkb_delete(pk);
    }
    {
        QByteArray q("", 0);
        void* pk = pkb_from_data("", 0);
        rec("ctor.empty0.size", q.size() == pkb_size(pk), "ctor-empty0-size");
        rec("ctor.empty0.content", ba_same(q, pkb_constData(pk), pkb_size(pk)), "ctor-empty0-content");
        pkb_delete(pk);
    }

    // 等值：含 NUL 对、空 vs 空、空 vs 非空、同串 / 异串
    struct Pair { const char* a; int la; const char* b; int lb; int expect; };
    const Pair pairs[] = {
        { "\x00\x01", 2, "\x00\x01", 2, 1 },
        { "\x00\x01", 2, "\x00\x02", 2, 0 },
        { "", 0, "", 0, 1 },
        { "", 0, "x", 1, 0 },
        { "hello", 5, "hello", 5, 1 },
        { "hello", 5, "hell", 4, 0 },
        { "\x00", 1, "", 0, 0 },
    };
    for (const Pair& p : pairs) {
        QByteArray qa(p.a, p.la), qb(p.b, p.lb);
        void* pa = pkb_from_data(p.a, p.la);
        void* pb = pkb_from_data(p.b, p.lb);
        int qeq = (qa == qb) ? 1 : 0;
        int peq = pkb_equals(pa, pb);
        std::string tag = "eq la=" + std::to_string(p.la) + " lb=" + std::to_string(p.lb);
        // 先验证测试向量本身有牙（Qt 的 == 结果符合预期），再验证两侧一致
        rec("eq.vector", qeq == p.expect, tag);
        rec("equals", qeq == peq, tag);
        pkb_delete(pa);
        pkb_delete(pb);
    }
}

// ═══ main ══════════════════════════════════════════════════════════════════

int main()
{
    std::printf("=== PkByteArray vs QByteArray Oracle ===\n\n");
    std::printf("--- resize ---\n");
    cmp_resize();
    std::printf("--- data/constData ---\n");
    cmp_data();
    std::printf("--- number ---\n");
    cmp_number();
    std::printf("--- ctor/equals ---\n");
    cmp_ctor_equals();

    std::printf("\nDIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    for (const auto& kv : g_tags) {
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    }

    return 0;
}
