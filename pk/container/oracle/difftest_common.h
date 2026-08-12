// difftest_common.h —— R-02 容器族对拍的共用地基
//
// R-01（QString ↔ PkString）用一个 331 行的单文件覆盖 14 个标量 API。容器不行：
// **8 个类型 × 约 40 个方法**，一个文件一个 API 地抄会写成几千行重复代码，而且
// 每个 API 都要为 Qt 侧和 Pk 侧各写一遍。
//
// 所以这里换一种组织：
//
//   1. **两侧类型不同、方法名相同** —— 这正是「API 形状与 Qt 一致」的意义。
//      一个函数模板同时吃 `QVector<int>` 与 `PkVector<int>`，编得过本身就是
//      形状对齐的证明；编不过说明某个方法签名与 Qt 不一致。
//   2. **输入是「初始状态 + 一串操作」而不是「一个值 + 参数」**。容器的状态
//      本身就是输出，所以每执行一条操作要比两样东西：**该操作的返回值** +
//      **操作后的容器状态**（`dump()` 出来的规范字符串）。两样都同才算 same。
//      只比末态是错的：一个 bug 让 removeAt 少删一个、另一个让 append 多加
//      一个，末态可能碰巧一致。
//   3. **共享维度**：一半用例在跑操作之前先拷一份 alias。COW 破了（写穿到
//      alias）就会在 alias 的 dump 上现形。alias 的比对折进同一次 `rec`，
//      于是「哪一条操作破的 COW」直接落在那条操作的 tag 上。
//
// ── 输出契约（判据只读这两种行）────────────────────────────
//     DIFF total=<N> mismatch=<M>      恰好一行
//     DIFFTAG <api> <tag> <count>      0..N 行，一类差异一行
// 退出码**必须是 0**，即使 M>0 —— 判定差异该不该存在是 reviewer 的事。
// `MISMATCH:` 明细行是给人读的。
//
// ── tag 的两条硬规则（都是 R-01 被真 bug 打出来的）────────
// **规则一：tag 必须由触发这次差异的输入形态构造**，绝不能「每个 API 一个字面量
// 常量」。R-01 第一版那么写，14 个 API 里 7 个每一次比对都预先落进白名单、完全
// 退出对拍；注入三组真 bug 全部绿灯通过。所以这里的 tag 一律是
// `<下标/空否/key在否 形态>/<shared|unshared>/<elem-int|elem-str>`，
// 声明一条偏离只豁免它真正解释得了的那一格。
//
// **规则二：tag 的判定谓词不能比 `.deviation` 里的理由宽。** 写完每一条
// `.deviation`，把理由里的每个限定词回头对一遍谓词：理由说「空容器」，谓词里就
// 必须出现 `size()==0`；理由说「key 不存在」，谓词就不能是「任何 key」。
//
// ── 形态契约 ───────────────────────────────────────────────
// 两侧真的分别 include `<QVector>` 与 `<PkVector.h>`，每个容器类型一条
// `static_assert(!std::is_same<...>::value)`。**编译时 `-I` 绝不能给
// `pk/container/compat/`** —— 给了两侧会解析成同一个类型，对拍恒等、永远绿。
// 别指望 static_assert 兜底，一开始就别写。

#pragma once

#include <QtGlobal>
#include <QMessageLogContext>
#include <QString>
#include <QByteArray>

#include <PkString.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

// 撞名会在**编译期**炸，成本一行。
static_assert(!std::is_same<QString, PkString>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");

// ── 计数 ──────────────────────────────────────────────────

inline long g_total = 0;
inline long g_mismatch = 0;
inline std::map<std::string, long> g_tags;   // "<api> <tag>" -> count
inline std::map<std::string, long> g_cover;  // "<api>"       -> 比对次数
inline long g_printed = 0;

inline void rec(const std::string &api, bool same, const std::string &tag,
                const std::string &in, const std::string &qs, const std::string &ps)
{
    ++g_total;
    ++g_cover[api];
    if (same) {
        return;
    }
    ++g_mismatch;
    ++g_tags[api + " " + tag];
    if (g_printed < 40) {           // 明细只打前 40 条，判据不读它
        ++g_printed;
        std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n",
                    api.c_str(), tag.c_str(), in.c_str(), qs.c_str(), ps.c_str());
    }
}

// ── 运行期自报与收尾 ──────────────────────────────────────

inline void oracleBegin(const char *suiteFile)
{
    // **把 Qt 的运行期警告吞掉。** 越界一类输入正是要对拍的形态，不吞会把
    // stderr 刷满，失败时那句 tail 只剩噪音。
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});
    // 自报运行期真正链上的 Qt 版本 —— 防「悄悄跑了系统那份 5.15.13」。
    std::printf("ORACLE-QT file=%s qVersion=%s\n", suiteFile, qVersion());
}

inline int oracleEnd()
{
    // ORACLE-COVER 判据不读，是给人读的：一个 API 如果这里是 0（或压根没出现），
    // 说明它的守卫把输入全拒了 / 指令表里漏了它 —— **零差异有可能只是零输入**。
    for (const auto &kv : g_cover) {
        std::printf("ORACLE-COVER %s %ld\n", kv.first.c_str(), kv.second);
    }
    for (const auto &kv : g_tags) {
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    }
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;   // 已声明的偏离不算失败
}

// ── 元素的规范字符串 ──────────────────────────────────────
//
// 字符串元素两侧分别转 UTF-8 再比原始字节；int 直接 to_string。
// 加引号是为了让 dump 无歧义（`[1|]` 与 `[1|""]` 分得开）。

inline std::string es(int v) { return std::to_string(v); }
inline std::string es(bool v) { return v ? "true" : "false"; }
inline std::string es(const QString &s)
{
    const QByteArray b = s.toUtf8();
    return "\"" + std::string(b.constData(), static_cast<std::size_t>(b.size())) + "\"";
}
inline std::string es(const PkString &s) { return "\"" + s.PkToUtf8() + "\""; }

// ── 元素 token 表 ─────────────────────────────────────────
//
// 两侧按**同一个下标**各自材料化自己的元素类型，所以 token 表只有一份下标空间。
//
// 字符串 token 里那三个非 ASCII 的是**故意挑的**：
//   é      U+00E9    UTF-16 一个码元 0x00E9
//   U+FFFD           UTF-16 一个码元 0xFFFD
//   🎨     U+1F3A8   UTF-16 代理对   0xD83C 0xDFA8
// 「按码元排序」与「按码点排序」在 U+FFFD 与 🎨 这一对上**结论相反**
// （码元序 🎨 < U+FFFD，码点序 U+FFFD < 🎨）。QMap 的迭代顺序、QStringList::sort
// 都由 `operator<` 决定，这一对能把两侧的比较口径差异逼出来。
// "a" 与 "ab" 那一对压的是前缀序。

inline const int kIntTok[] = { 0, 1, 2, 3, -1, 7, 42 };
inline const char *const kStrTok[] = {
    "", "a", "ab", "b", "\xC3\xA9", "\xEF\xBF\xBD", "\xF0\x9F\x8E\xA8",
};
inline constexpr int kNTok = 7;

template <typename T> struct PkMake;
template <> struct PkMake<int>      { static int      make(int i) { return kIntTok[i]; } };
template <> struct PkMake<QString>  { static QString  make(int i) { return QString::fromUtf8(kStrTok[i]); } };
template <> struct PkMake<PkString> { static PkString make(int i) { return PkString(kStrTok[i]); } };

// ── 规范 dump ─────────────────────────────────────────────
//
//   序列容器   [3|1|2|3]          size 后跟逐元素，`|` 分隔
//   有序关联   {2|"a"=1|"b"=2}    QMap/std::map 按 key 有序，直接按迭代顺序
//   无序关联   {2|...}            **先收集再排序**，与有序同形
//
// 全部取 const 引用，避免 dump 自己触发 detach 把共享状态搅了。

template <typename C>
std::string dumpSeq(const C &c)
{
    std::string r = "[" + std::to_string(c.size());
    for (int i = 0; i < c.size(); ++i) {
        r += "|";
        r += es(c.at(i));
    }
    return r + "]";
}

template <typename M>
std::string dumpAssocOrdered(const M &m)
{
    std::string r = "{" + std::to_string(m.size());
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        r += "|" + es(it.key()) + "=" + es(it.value());
    }
    return r + "}";
}

template <typename M>
std::string dumpAssocSorted(const M &m)
{
    std::vector<std::string> v;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        v.push_back(es(it.key()) + "=" + es(it.value()));
    }
    std::sort(v.begin(), v.end());
    std::string r = "{" + std::to_string(m.size());
    for (const auto &s : v) {
        r += "|" + s;
    }
    return r + "}";
}

template <typename S>
std::string dumpSetSorted(const S &s)
{
    std::vector<std::string> v;
    for (auto it = s.constBegin(); it != s.constEnd(); ++it) {
        v.push_back(es(*it));
    }
    std::sort(v.begin(), v.end());
    std::string r = "{" + std::to_string(s.size());
    for (const auto &e : v) {
        r += "|" + e;
    }
    return r + "}";
}

// 序列型返回值（keys()/values()/toList() 之类）的规范化：无序容器要先排序。
template <typename L>
std::string dumpListSorted(const L &l)
{
    std::vector<std::string> v;
    for (int i = 0; i < l.size(); ++i) {
        v.push_back(es(l.at(i)));
    }
    std::sort(v.begin(), v.end());
    std::string r = "[" + std::to_string(l.size());
    for (const auto &e : v) {
        r += "|" + e;
    }
    return r + "]";
}

// ── 下标编码与形态 ────────────────────────────────────────
//
// 下标写死成字面量没法覆盖「刚好等于 size」这一格（size 是跑起来才知道的），
// 所以用编码：900/901/902 分别是 size / size-1 / size+1，其余按字面量。

enum PkIdxCode {
    PK_IDX_SIZE    = 900,
    PK_IDX_SIZE_M1 = 901,
    PK_IDX_SIZE_P1 = 902,
};

inline int pkResolveIdx(int code, int n)
{
    switch (code) {
    case PK_IDX_SIZE:    return n;
    case PK_IDX_SIZE_M1: return n - 1;
    case PK_IDX_SIZE_P1: return n + 1;
    default:             return code;
    }
}

// **形态由「这个下标相对当前 size 落在哪」决定，不是由 API 名字决定。**
// 规则一就是靠这个函数落实的。
inline const char *pkIdxShape(int r, int n)
{
    if (r < 0)  return "idx-neg";
    if (r > n)  return "idx-oob";
    if (r == n) return "idx-at-size";
    return "idx-in-range";
}

inline const char *pkEmptyShape(int n) { return n == 0 ? "empty" : "nonempty"; }
inline const char *pkPresentShape(bool present) { return present ? "key-present" : "key-absent"; }

// tag = <形态>/<共享>/<元素类型>。三段都参与，声明一条偏离只豁免那一格。
inline std::string pkTag(const std::string &shape, bool shared, bool elemStr)
{
    return shape + (shared ? "/shared" : "/unshared") + (elemStr ? "/elem-str" : "/elem-int");
}

// ── 组合生成器 ────────────────────────────────────────────
//
// 三层：
//   depth1  每条指令 × 每个初始状态 × shared/unshared —— 保证没有指令漏测
//   depth2  指令两两全组合 —— 主力，R-01 那 121 处差异就是全组合挖出来的
//   depth3  只用**会改状态**的那部分指令做三层（全表三层会到千万量级、跑几分钟，
//           R-01 踩过）
//
// 回调签名：void(const std::vector<int> &instrIdx, const std::vector<int> &init, bool shared)

template <typename Fn>
void pkForEachCase(int nInstr, const std::vector<int> &mutIdx,
                   const std::vector<std::vector<int>> &inits, Fn fn)
{
    std::vector<int> seq;
    for (const auto &init : inits) {
        for (int shared = 0; shared < 2; ++shared) {
            // depth 1
            for (int a = 0; a < nInstr; ++a) {
                seq = { a };
                fn(seq, init, shared != 0);
            }
            // depth 2
            for (int a = 0; a < nInstr; ++a) {
                for (int b = 0; b < nInstr; ++b) {
                    seq = { a, b };
                    fn(seq, init, shared != 0);
                }
            }
        }
        // depth 3：只在 unshared 下跑，且只用改状态的指令
        for (int a : mutIdx) {
            for (int b : mutIdx) {
                for (int c : mutIdx) {
                    seq = { a, b, c };
                    fn(seq, init, false);
                }
            }
        }
    }
}
