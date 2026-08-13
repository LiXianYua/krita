// main.cpp —— pointer_difftest.cpp 的驱动程序，独立 TU（裁决 R2）。
//
// pointer_difftest.cpp 被编译两次（-DORACLE_QT_SIDE 与否），各自只看得到一侧的
// 真实类型，没法在那份文件里直接比较两个类型是不是被垫片编成了同一个。这份文件
// 只编译一次，能同时看到 <QSharedPointer>（真 Qt5 头，见下面的 -I）与
// "PkSharedPointer.h"（不走 compat，直接读源文件），是唯一能放"两个类型不是
// 同一个类型"这条自证的地方——这就是形态契约第一条自证的落点：
#include <QSharedPointer>
#include "PkSharedPointer.h"
#include <type_traits>

namespace {
struct OracleAssertPayload {};
}  // namespace

static_assert(!std::is_same<QSharedPointer<OracleAssertPayload>,
                             PkSharedPointer<OracleAssertPayload>>::value,
              "两侧被编成了同一个类型，对拍失去意义——检查 include 路径有没有误给 compat/");

#include "TraceOps.h"

#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

// pointer_difftest.cpp 里两次编译各自导出的符号（ORACLE_SIDE_FN 生成的名字）。
extern std::string runSharedScriptQt(const Script &s);
extern std::string runSharedScriptPk(const Script &s);
extern std::string runScopedScriptQt(const Script &s);
extern std::string runScopedScriptPk(const Script &s);

namespace {

std::vector<std::string> splitLines(const std::string &s)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

// ── 手挑对抗用例（简报 Step 3 表格，逐条对应 §0 某行探针或某条判据）───────
std::vector<Script> buildSharedHandpicked()
{
    return {
        // makeNullRaw 之后 weakFrom → strongFromWeak：判据 A（P2 那条 std/Qt 分叉）
        {{OpMakeNullRaw, 0, 0}, {OpWeakFrom, 0, 0}, {OpStrongFromWeak, 1, 0}},
        // makeNullDeleter 之后 clear：判据 D（空指针也调 deleter）
        {{OpMakeNullDeleter, 0, 0}, {OpClear, 0, 0}},
        // makeNew → weakFrom → clear → strongFromWeak：弱引用过期（P5）
        {{OpMakeNew, 0, 0}, {OpWeakFrom, 0, 0}, {OpClear, 0, 0}, {OpStrongFromWeak, 1, 0}},
        // makeDerived → dynCastUnrelated：转型失败返回 null（P6）
        {{OpMakeDerived, 0, 0}, {OpDynamicCastToUnrelated, 0, 1}},
        // makeDefault → dynCastDerived：对 null 转型不崩（P6）
        {{OpMakeDefault, 0, 0}, {OpDynamicCastToDerived, 0, 1}},
        // selfAssign：P15
        {{OpMakeNew, 0, 0}, {OpSelfAssign, 0, 0}},
    };
}

std::vector<Script> buildScopedHandpicked()
{
    return {
        // resetSame（scoped）：Task 1 Step 5 那条自赋值保护
        {{SOpMakeNew, 0, 0}, {SOpResetSame, 0, 0}},
        // take → reset 同一指针（scoped）：P9 的所有权转出/转回
        {{SOpMakeNew, 0, 0}, {SOpTakeThenReset, 0, 0}},
        // arrayMake(3) → arrayReset：delete[] 而不是 delete（P14）
        {{SOpArrayMake, 0, 0}, {SOpArrayReset, 0, 0}},
    };
}

// ── 组合爆破 ────────────────────────────────────────────────────────────
// 槽号不是组合的一个维度（那会再乘一个维度，见简报 Step 3）：a/b 由一个跨脚本
// 连续递增的计数器 g 生成，g%SLOTS 与 (g/SLOTS)%SLOTS 在小范围内轮换，与"选哪个
// 操作码"这个真正的组合维度解耦。
std::vector<Script> buildSharedCombinatorial()
{
    const long ops = OpSharedOpCount;   // 20
    const long steps = 4;
    const long total = ops * ops * ops * ops;  // 20^4 = 160000
    std::vector<Script> v;
    v.reserve(total);
    long g = 0;
    for (long c = 0; c < total; ++c) {
        long rem = c;
        Script s;
        s.reserve(steps);
        for (long p = 0; p < steps; ++p) {
            int op = int(rem % ops);
            rem /= ops;
            int a = int(g % 4);
            int b = int((g / 4) % 4);
            ++g;
            s.push_back({op, a, b});
        }
        v.push_back(std::move(s));
    }
    return v;
}

std::vector<Script> buildScopedCombinatorial()
{
    const long ops = SOpScopedOpCount;  // 11
    const long steps = 5;
    long total = 1;
    for (long i = 0; i < steps; ++i) total *= ops;  // 11^5 = 161051
    std::vector<Script> v;
    v.reserve(total);
    long g = 0;
    for (long c = 0; c < total; ++c) {
        long rem = c;
        Script s;
        s.reserve(steps);
        for (long p = 0; p < steps; ++p) {
            int op = int(rem % ops);
            rem /= ops;
            int a = int(g % 2);
            int b = int((g / 2) % 2);
            ++g;
            s.push_back({op, a, b});
        }
        v.push_back(std::move(s));
    }
    return v;
}

struct DiffCounters {
    long total = 0;
    long mismatch = 0;
    std::map<std::pair<std::string, std::string>, long> tags;  // (api,tag) -> count
};

// tag 取"触发这次差异的那一步"（单步子脚本喂 scriptTag），不是整条脚本：
// 组合爆破里 a/b 由跨脚本连续计数器生成，同一个根因在不同脚本里前后步的槽号
// 会不一样，整条脚本序列化会把同一个根因拆成上万个几乎不重复的 tag——不是
// "每个 API 一个字面量常量"那种反面（规则一仍然满足：tag 由触发差异的那一步的
// 真实 op+槽号构造），但整脚本粒度在这个生成方案下退化成"每次几乎唯一"，
// 对 reviewer 判定没有意义。单步粒度是同一个 scriptTag() helper 的另一种喂法，
// 不是新写一套 tag 逻辑。
void runFamily(const char *api, const std::vector<Script> &scripts,
               std::string (*runQt)(const Script &), std::string (*runPk)(const Script &),
               const char *(*nameFn)(int), DiffCounters &out)
{
    for (const Script &s : scripts) {
        std::string qt = runQt(s);
        std::string pk = runPk(s);
        std::vector<std::string> ql = splitLines(qt);
        std::vector<std::string> pl = splitLines(pk);
        size_t n = ql.size() > pl.size() ? ql.size() : pl.size();
        for (size_t i = 0; i < n; ++i) {
            ++out.total;
            const std::string &a = i < ql.size() ? ql[i] : std::string("<missing-line>");
            const std::string &b = i < pl.size() ? pl[i] : std::string("<missing-line>");
            if (a != b) {
                ++out.mismatch;
                Script single;
                if (i < s.size()) single.push_back(s[i]);
                std::string tag = scriptTag(single, nameFn);
                ++out.tags[{std::string(api), tag}];
            }
        }
    }
}

}  // namespace

int main()
{
    DiffCounters out;

    std::vector<Script> sharedScripts = buildSharedHandpicked();
    std::vector<Script> sharedCombo = buildSharedCombinatorial();
    sharedScripts.insert(sharedScripts.end(), sharedCombo.begin(), sharedCombo.end());
    runFamily("shared", sharedScripts, runSharedScriptQt, runSharedScriptPk, sharedOpName, out);

    std::vector<Script> scopedScripts = buildScopedHandpicked();
    std::vector<Script> scopedCombo = buildScopedCombinatorial();
    scopedScripts.insert(scopedScripts.end(), scopedCombo.begin(), scopedCombo.end());
    runFamily("scoped", scopedScripts, runScopedScriptQt, runScopedScriptPk, scopedOpName, out);

    std::printf("DIFF total=%ld mismatch=%ld\n", out.total, out.mismatch);
    for (const auto &kv : out.tags) {
        std::printf("DIFFTAG %s %s %ld\n", kv.first.first.c_str(), kv.first.second.c_str(), kv.second);
    }
    return 0;
}
