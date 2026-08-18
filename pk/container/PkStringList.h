#pragma once

#include "PkList.h"

#include "PkString.h"

#include <algorithm>
#include <initializer_list>
#include <set>
#include <vector>

// ---------------------------------------------------------------------------
// PkStringList —— Qt5 QStringList 的替代品。`QStringList` 在 SRC 里作为
// **类型名**出现 1547 次，所以这个名字必须存在。
//
// ---- 为什么是派生类而不是 typedef ----
//
// 专有方法在调用点是**成员调用**（`list.join(", ")`、`list.removeDuplicates()`）。
// 写成 `typedef PkList<PkString> PkStringList` 的话，`PkList<int>` 也会长出
// join —— 那是错的。Qt 自己就是用派生类解决的
// （`class QStringList : public QList<QString>`），照做。
//
// ---- 本文件为什么是纯 header、且不进 libpkcontainer.a ----
//
// 与 PkStringHash.h 同一条理由：它要 include pk/string 的 PkString.h，而
// libpkcontainer.a **刻意不依赖 pkstring**（判据③ 的 `nm -u` 口径靠这条保持
// 干净，见 CMakeLists.txt 里那段注释）。所以本文件全 inline，不加 .cpp 到
// 库里；需要它的测试目标自己额外链 pkstring。
//
// ---- COW ----
//
// 拷贝 O(1)、PkMut() 是唯一写入口、PkConst() 绝不 detach —— 全部由
// PkList → PkArrayContainer → PkArrayData 那条链兜住。**但本类新增的写方法
// （sort / removeDuplicates / replaceInStrings）是新的漏洞面**：它们直接碰
// this->m_d，每一个都必须经 PkMut()。单测逐个压过「共享态下调用后不再共享，
// 且另一边一个字节不变」。
//
// ---- 真 Qt 5.15.7 实测（本文件全部行为的对齐依据）----
//
//   --- join ---
//   空列表     join(",")  = ''      （长度 0）
//   单元素     join(",")  = 'a'     （无分隔符）
//   多元素     join(",")  = 'a,b,c'    join("") = 'abc'
//   含空串 {"", "b", ""}  join("-")  = '-b-'   ← 空元素照样参与，不跳过
//   join(QChar('/'))      = 'a/b/c'            ← QChar 重载存在
//   --- removeDuplicates ---
//   {"b","a","b","c","a"} → 返回 2，结果 = b,a,c  ← 返回删除个数，保留首次出现顺序
//   --- filter ---
//   {"apple","banana","cherry"}.filter("an") → 1 个命中: banana  ← 子串包含，非前缀
//   --- 与 QList<QString> 互转 ---
//   QList<QString> → QStringList  ：可隐式转换，size 保持
//   QStringList    → QList<QString>：可隐式转换，size 保持
//   --- 链式 << ---
//   (c << "p" << "q").join("|") = 'p|q'   ← 链式后仍是 QStringList，能直接 join
//   --- replaceInStrings (before 为空) ---
//   "a".replace("","b")        len=3 : 0062 0061 0062        （= "bab"）
//   "ab".replace("","x")       len=5 : 0078 0061 0078 0062 0078   （= "xaxbx"）
//   "".replace("","b")         len=1 : 0062                  （= "b"，不是继续保持空）
//   "ab".replace("","")        len=2 : 0061 0062              （after 为空 → 不可见地不变）
//   🎨(U+1F3A8,即 d83c dfa8).replace("","X")
//                               len=5 : 0058 d83c 0058 dfa8 0058   （代理对被从中间插断）
//   ("a"+🎨+"b").replace("","-")
//                               len=9 : 002d 0061 002d d83c 002d dfa8 002d 0062 002d
//   （这份原始探针输出是 `pkStringReplaceAll` 空 `before` 分支的第一手依据，
//   随本文件走、不依赖外部仓库；docs/superpowers/plans/R-23.md「探针实测」一节
//   有同一批数据与更完整的命令记录，但那份文档在工作空间仓库里，fork 若脱离
//   工作空间独立签出就找不到了——上面这份是兜底。）
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// PkCaseSensitivity —— Qt::CaseSensitivity 的替代品。
//
// **枚举值刻意与 Qt 对齐**（Qt::CaseInsensitive == 0、Qt::CaseSensitive == 1），
// 这样 compat 垫片可以直接 `#define Qt::CaseInsensitive PkCaseInsensitive`
// 之类地改写，而不必翻译数值。
//
// **它的正确归宿是 pk/string，不是这里。** 大小写敏感性是字符串的概念，
// QString/QStringList 两边都要用。放在本文件里是因为 R-02 不许改 pk/string
// （那是 R-01/R-13 的地盘）。**R-13 迁 pk/string 时应当把它挪过去，本文件改成
// include** —— 挪的时候记得保持枚举值不变。
// ---------------------------------------------------------------------------
enum PkCaseSensitivity {
    PkCaseInsensitive = 0,
    PkCaseSensitive = 1
};

// ---- 大小写折叠：**只覆盖 ASCII，这是一条已知的能力缺口** ----
//
// 正确的 Unicode 大小写折叠要查表（土耳其语的 i/İ、德语 ß、希腊语终末 sigma
// 都不是「减 0x20」能解决的），那是 PkString 该提供的能力
// （`QString::toCaseFolded()` 的对应物），而 pk/string 不归 R-02 改。
//
// **退化方向是安全的**：非 ASCII 字符在不区分大小写模式下退回逐码元精确比较，
// 结果是「可能漏掉本该匹配的项」，不会「匹配到不该匹配的项」。对 filter 这种
// 搜索过滤的用途，漏匹配是可接受的降级，误匹配才是危险的。
//
// **8 个 filter 调用点里有 5 个传 CaseInsensitive**（资源/标签搜索框），而
// Krita 的标签是能输中日韩与西里尔文的 —— 所以这条缺口是真会被用户碰到的，
// 不是理论问题。**R-13 给 PkString 补上大小写折叠之后，这里应当改成调它。**
inline char16_t pkAsciiFold(char16_t c) noexcept
{
    return (c >= u'A' && c <= u'Z') ? static_cast<char16_t>(c - u'A' + u'a') : c;
}

// s 的 pos 位置起是不是 needle。调用方保证 pos + needle.size() <= s.size()。
inline bool pkStringMatchesAt(const PkString &s, int pos, const PkString &needle,
                              PkCaseSensitivity cs)
{
    const int m = needle.size();
    for (int j = 0; j < m; ++j) {
        char16_t a = s.at(pos + j);
        char16_t b = needle.at(j);
        if (cs == PkCaseInsensitive) {
            a = pkAsciiFold(a);
            b = pkAsciiFold(b);
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

// 子串包含。空 needle 恒为真 —— 与 QString::contains("") 以及本仓库
// PkString::contains("")（pkIndexOf 对空 needle 返回 0）都一致。
//
// 逐码元用 at() 比较，**不走 PkToU16()**：后者每次调用都拷一份缓冲区出来，
// 而 filter 要对整张表的每个元素调一次。
inline bool pkStringContains(const PkString &s, const PkString &needle, PkCaseSensitivity cs)
{
    const int n = s.size();
    const int m = needle.size();
    if (m == 0) {
        return true;
    }
    if (m > n) {
        return false;
    }
    for (int i = 0; i + m <= n; ++i) {
        if (pkStringMatchesAt(s, i, needle, cs)) {
            return true;
        }
    }
    return false;
}

// 把 s 里所有的 before 换成 after。
//
// **`before` 为空时**：真 Qt 5.15.7 实测（探针见
// docs/superpowers/plans/R-23.md「探针实测」一节），把 `after` 插到 s 的每一个
// UTF-16 码元之间以及首尾，插入点数 = size()+1，**与某个码元是不是落在代理对
// 内部无关**——Qt 本身按码元位置操作，不做代理对判断，本实现照此对齐。
// `cs`（大小写敏感性）对这条分支不起作用：没有东西可比较（探针用例 8 证实）。
inline PkString pkStringReplaceAll(const PkString &s, const PkString &before,
                                   const PkString &after, PkCaseSensitivity cs)
{
    const int n = s.size();
    const int bn = before.size();
    if (bn == 0) {
        PkString out;
        for (int i = 0; i < n; ++i) {
            out += after;
            out += s.mid(i, 1);
        }
        out += after;
        return out;
    }
    if (bn > n) {
        return s;
    }

    PkString res;
    int i = 0;
    int runStart = 0;
    while (i + bn <= n) {
        if (pkStringMatchesAt(s, i, before, cs)) {
            if (i > runStart) {
                res += s.mid(runStart, i - runStart);
            }
            res += after;
            i += bn;
            runStart = i;
        } else {
            ++i;
        }
    }
    if (runStart < n) {
        res += s.mid(runStart, n - runStart);
    }
    return res;
}

// 单个 UTF-16 码元 → PkString。join(char16_t) 要用。
//
// PkString 没有「从 char16_t 构造」的公开入口，只有 PkFromUtf8(const char*, int)，
// 所以这里就地把一个 BMP 码元编成 UTF-8（1–3 字节）。**孤立代理项
// （0xD800–0xDFFF）编出来是病态的 UTF-8** —— 那种输入本身就没有意义
// （QChar 分隔符不可能是半个代理对），不特判。
inline PkString pkCharToString(char16_t c)
{
    char buf[3];
    int n = 0;
    if (c < 0x80) {
        buf[0] = static_cast<char>(c);
        n = 1;
    } else if (c < 0x800) {
        buf[0] = static_cast<char>(0xC0 | (c >> 6));
        buf[1] = static_cast<char>(0x80 | (c & 0x3F));
        n = 2;
    } else {
        buf[0] = static_cast<char>(0xE0 | (c >> 12));
        buf[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        buf[2] = static_cast<char>(0x80 | (c & 0x3F));
        n = 3;
    }
    return PkString::PkFromUtf8(buf, n);
}

class PkStringList : public PkList<PkString>
{
    using PkBase = PkList<PkString>;
    using PkInner = PkArrayContainer<PkString, PkList<PkString>>::PkInner;

public:
    PkStringList() = default;

    // 从基类隐式转换（**不加 explicit**）：实测「QList<QString> → QStringList
    // 可隐式转换，size 保持」。调用点里大量函数返回 QList<QString> 却赋给
    // QStringList。反方向（PkStringList → PkList<PkString>）由继承本身给到。
    PkStringList(const PkList<PkString> &other) : PkBase(other) {}

    // 调用点有 `QStringList{...}` 写法（试接目标 B 的 KisGlobalTest 就是）。
    PkStringList(std::initializer_list<PkString> args) : PkBase(args) {}

    ~PkStringList() = default;
    PkStringList(const PkStringList &) = default;
    PkStringList &operator=(const PkStringList &) = default;
    PkStringList(PkStringList &&) = default;
    PkStringList &operator=(PkStringList &&) = default;

    // ---- QStringList 专有 ----

    // join —— 专有 API 的绝对大头（SRC 155 处）。
    //
    // 实测的四条边界全部照此实现：空列表出空串、单元素不带分隔符、分隔符可以
    // 是空串、**空元素照样参与**（{"", "b", ""}.join("-") == "-b-"，不跳过）。
    PkString join(const PkString &sep) const
    {
        const PkInner &v = this->m_d.PkConst();
        PkString out;
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (i != 0) {
                out += sep;
            }
            out += v[i];
        }
        return out;
    }

    // Qt 的 QChar 重载（实测 join(QChar('/')) == "a/b/c"）。char16_t 是本仓库
    // 的 QChar 对应物（PkString::split(char16_t) 已经立了这个先例）。
    PkString join(char16_t sep) const { return join(pkCharToString(sep)); }

    // Qt5 的签名带 Qt::CaseSensitivity。区分大小写时用 PkString 自己的
    // operator<（逐码元序，与 QString::operator< 同口径）。
    void sort(PkCaseSensitivity cs = PkCaseSensitive)
    {
        PkInner &v = this->m_d.PkMut();
        if (cs == PkCaseSensitive) {
            std::sort(v.begin(), v.end(),
                      [](const PkString &a, const PkString &b) { return a < b; });
        } else {
            std::sort(v.begin(), v.end(), [](const PkString &a, const PkString &b) {
                const int na = a.size();
                const int nb = b.size();
                const int n = na < nb ? na : nb;
                for (int i = 0; i < n; ++i) {
                    const char16_t x = pkAsciiFold(a.at(i));
                    const char16_t y = pkAsciiFold(b.at(i));
                    if (x != y) {
                        return x < y;
                    }
                }
                return na < nb;
            });
        }
    }

    // filter —— **只实现这一个重载**，依据是逐处核实过的 8 个真实调用点：
    //   libs/libkis/Krita.cpp:311                        单参（默认 CaseSensitive）
    //   libs/ui/input/KisInputProfileMigrator.cpp:29     单参（默认 CaseSensitive）
    //   libs/pigment/KoColor.cpp:599                     显式 CaseInsensitive
    //   libs/resources/KisResourceSearchBoxFilter.cpp:98/105/113/121  显式 CaseInsensitive
    // 默认参数与显式第二参两条路径都有真实调用点，所以第二参不能省。
    //
    // **QRegExp 重载：全仓 0 处调用点，不做。**
    // **QRegularExpression 重载：1 处（TextPropertyConfigModel.cpp:234），本任务
    // 做不了** —— pk/ 下还没有正则类型，造一个不是容器任务的事。见报告。
    //
    // 语义是**子串包含，不是前缀**（实测 {"apple","banana","cherry"}.filter("an")
    // 只命中 banana）。
    PkStringList filter(const PkString &str, PkCaseSensitivity cs = PkCaseSensitive) const
    {
        const PkInner &v = this->m_d.PkConst();
        PkStringList out;
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (pkStringContains(v[i], str, cs)) {
                out.append(v[i]);
            }
        }
        return out;
    }

    // 返回删掉了几个，**保留首次出现的顺序**
    // （实测 {"b","a","b","c","a"} → 返回 2，结果 b,a,c）。
    //
    // 先 const 扫一遍确认有没有重复，没有就**不碰 PkMut()、不 detach**。
    // 这不是自作聪明：Qt 的实现（QtPrivate::QStringList_removeDuplicates）
    // 只在 `j != i` 时才写 `(*that)[j]`、只在 `n != j` 时才 erase，没有重复
    // 时一次写都没有，同样不 detach。与 PkList::removeAll 那条修复同一条道理。
    int removeDuplicates()
    {
        const PkInner &cv = this->m_d.PkConst();
        const std::size_t n = cv.size();
        std::set<PkString> probe;
        for (std::size_t i = 0; i < n; ++i) {
            probe.insert(cv[i]);
        }
        if (probe.size() == n) {
            return 0;
        }

        PkInner &v = this->m_d.PkMut();
        std::set<PkString> seen;
        std::size_t out = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (seen.insert(v[i]).second) {
                if (out != i) {
                    v[out] = v[i];
                }
                ++out;
            }
        }
        v.erase(v.begin() + static_cast<std::ptrdiff_t>(out), v.end());
        return static_cast<int>(n - out);
    }

    // 唯一调用点：libs/ui/animation/KisDlgImportVideoAnimation.cpp:246
    //   frameFileList.replaceInStrings("output_", directory.absolutePath() + ... );
    // 两个实参都是 QString、未显式传 CaseSensitivity，所以默认参数要留、
    // 正则那两个重载不做（各 0 处调用点）。语义是**原地修改**并返回自身引用。
    //
    // 无条件走 PkMut()：Qt 的 replaceInStrings 经非 const operator[] 写回，
    // 一律 detach，这里不开例外。
    PkStringList &replaceInStrings(const PkString &before, const PkString &after,
                                   PkCaseSensitivity cs = PkCaseSensitive)
    {
        PkInner &v = this->m_d.PkMut();
        for (std::size_t i = 0; i < v.size(); ++i) {
            v[i] = pkStringReplaceAll(v[i], before, after, cs);
        }
        return *this;
    }

    // ---- 链式操作符：返回 PkStringList&，不让类型退化 ----
    //
    // 基类的 operator<< 返回 `PkList<PkString>&`（CRTP 的 Derived 是
    // PkList<PkString>，不是 PkStringList）。不重新声明的话：
    //     PkStringList l;
    //     (l << "a" << "b").join(", ");   // ← 编不过：PkList<PkString> 没有 join
    // Qt 的 QStringList 正是为此重新声明了这几个。单测里有一条直接写
    // `(c << "p" << "q").join("|")`，**编得过就是证明**。

    PkStringList &operator<<(const PkString &s)
    {
        this->append(s);
        return *this;
    }

    PkStringList &operator<<(const PkList<PkString> &other)
    {
        this->append(other);
        return *this;
    }

    PkStringList &operator+=(const PkString &s)
    {
        this->append(s);
        return *this;
    }

    PkStringList &operator+=(const PkList<PkString> &other)
    {
        this->append(other);
        return *this;
    }
};
