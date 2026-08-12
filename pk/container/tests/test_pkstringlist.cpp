#include "PkStringListTest.h"

#include "../PkStringList.h"

#include <type_traits>
#include <utility>

#include "pk_binder_PkStringListTest.inc"

namespace {

// ---- 契约的编译期部分（签名形状，不是行为）----

// **PkStringList 必须是派生类，不是 typedef**：写成
// `typedef PkList<PkString> PkStringList` 的话，PkList<int> 也会长出 join。
// 这两条一起把它钉死。
static_assert(std::is_base_of<PkList<PkString>, PkStringList>::value,
              "PkStringList 必须派生自 PkList<PkString>");
static_assert(!std::is_same<PkStringList, PkList<PkString>>::value,
              "PkStringList 不能是 PkList<PkString> 的 typedef");

// 基类 → 派生类的隐式转换必须存在（实测「QList<QString> → QStringList
// 可隐式转换」），所以那个构造**不能**加 explicit。
static_assert(std::is_convertible<PkList<PkString>, PkStringList>::value,
              "PkList<PkString> → PkStringList 必须可隐式转换");

// join 按值返回 PkString；filter 按值返回 PkStringList（不是基类！）
static_assert(std::is_same<decltype(std::declval<const PkStringList &>().join(PkString())),
                           PkString>::value,
              "join() 必须返回 PkString");
static_assert(std::is_same<decltype(std::declval<const PkStringList &>().filter(PkString())),
                           PkStringList>::value,
              "filter() 必须返回 PkStringList，不是基类");

// removeDuplicates 返回删除个数（Qt 语义），不是 void
static_assert(std::is_same<decltype(std::declval<PkStringList &>().removeDuplicates()),
                           int>::value,
              "removeDuplicates() 必须返回 int");

// replaceInStrings 原地修改并返回自身引用
static_assert(std::is_same<decltype(std::declval<PkStringList &>().replaceInStrings(
                               PkString(), PkString())),
                           PkStringList &>::value,
              "replaceInStrings() 必须返回 PkStringList&");

// **链式操作符必须返回 PkStringList&**（协变返回类型的坑）：基类的
// operator<< 返回 Derived& == PkList<PkString>&，不重新声明就会退化，
// `(l << "a").join(", ")` 编不过。
static_assert(std::is_same<decltype(std::declval<PkStringList &>() << PkString()),
                           PkStringList &>::value,
              "operator<<(PkString) 必须返回 PkStringList&");
static_assert(std::is_same<decltype(std::declval<PkStringList &>() += PkString()),
                           PkStringList &>::value,
              "operator+=(PkString) 必须返回 PkStringList&");

// 枚举值与 Qt 对齐（Qt::CaseInsensitive == 0、Qt::CaseSensitive == 1），
// 这样 compat 垫片可以直接改写名字而不必翻译数值。
static_assert(static_cast<int>(PkCaseInsensitive) == 0, "PkCaseInsensitive 必须是 0");
static_assert(static_cast<int>(PkCaseSensitive) == 1, "PkCaseSensitive 必须是 1");

} // namespace

// ---------------------------------------------------------------------------
// 与基类的互转
// ---------------------------------------------------------------------------

void PkStringListTest::convertsToAndFromBaseList()
{
    // 实测：QList<QString> → QStringList 可隐式转换，size 保持
    PkList<PkString> base;
    base.append(PkString("a"));
    base.append(PkString("b"));

    PkStringList sl = base;   // 隐式，不写 static_cast
    PK_COMPARE(sl.size(), 2);
    PK_COMPARE(sl.at(0), PkString("a"));
    PK_COMPARE(sl.at(1), PkString("b"));
    // 转过来之后专有方法就能用了（这正是 typedef 方案给不了的）
    PK_COMPARE(sl.join(PkString(",")), PkString("a,b"));

    // 实测：QStringList → QList<QString> 可隐式转换，size 保持
    // （派生 → 基类，由继承本身给到）
    const PkList<PkString> &backRef = sl;
    PK_COMPARE(backRef.size(), 2);
    PkList<PkString> back = sl;
    PK_COMPARE(back.size(), 2);
    PK_COMPARE(back.at(1), PkString("b"));

    // 转换是 COW 的：拷过去 O(1) 且共享
    PK_VERIFY(back.PkIsSharedWith(sl));

    // 函数形参处的隐式转换也成立（调用点里最常见的形态）
    struct Helper {
        static int take(const PkStringList &l) { return l.size(); }
    };
    PK_COMPARE(Helper::take(base), 2);
}

void PkStringListTest::initializerListConstruction()
{
    // 调用点有 QStringList{...} 写法
    PkStringList l{PkString("x"), PkString("y"), PkString("z")};
    PK_COMPARE(l.size(), 3);
    PK_COMPARE(l.at(0), PkString("x"));
    PK_COMPARE(l.at(2), PkString("z"));

    // const char* → PkString 是隐式转换（PkString 刻意没加 explicit），
    // 所以字面量列表也该编得过——调用点大量这么写。
    PkStringList lit{"p", "q"};
    PK_COMPARE(lit.size(), 2);
    PK_COMPARE(lit.join(PkString("-")), PkString("p-q"));

    PkStringList empty;
    PK_COMPARE(empty.size(), 0);
    PK_VERIFY(empty.isEmpty());
}

// ---------------------------------------------------------------------------
// join —— 155 处调用点，实测的每条边界都压
// ---------------------------------------------------------------------------

void PkStringListTest::joinBoundaries()
{
    // 实测（真 Qt 5.15.7）：
    //   空列表     join(",")  = ''      （长度 0）
    //   单元素     join(",")  = 'a'     （无分隔符）
    //   多元素     join(",")  = 'a,b,c'    join("") = 'abc'
    //   含空串 {"", "b", ""}  join("-")  = '-b-'   ← 空元素照样参与，不跳过

    PkStringList empty;
    PK_COMPARE(empty.join(PkString(",")), PkString(""));
    PK_COMPARE(empty.join(PkString(",")).size(), 0);
    PK_VERIFY(empty.join(PkString(",")).isEmpty());

    PkStringList one{"a"};
    PK_COMPARE(one.join(PkString(",")), PkString("a"));

    PkStringList many{"a", "b", "c"};
    PK_COMPARE(many.join(PkString(",")), PkString("a,b,c"));
    PK_COMPARE(many.join(PkString("")), PkString("abc"));

    // 多字符分隔符
    PK_COMPARE(many.join(PkString(", ")), PkString("a, b, c"));

    // **空元素照样参与，不跳过** —— 这一格最容易被"顺手优化"掉
    PkStringList withEmpties{"", "b", ""};
    PK_COMPARE(withEmpties.join(PkString("-")), PkString("-b-"));

    // 全是空串
    PkStringList allEmpty{"", ""};
    PK_COMPARE(allEmpty.join(PkString("-")), PkString("-"));

    // join 是 const 路径：不得 detach
    PkStringList a{"a", "b"};
    PkStringList b(a);
    (void)a.join(PkString(","));
    PK_VERIFY(a.PkIsSharedWith(b));
}

void PkStringListTest::joinCharSeparator()
{
    // 实测：join(QChar('/')) = 'a/b/c'  ← QChar 重载存在。
    // char16_t 是本仓库的 QChar 对应物（PkString::split(char16_t) 立的先例）。
    PkStringList l{"a", "b", "c"};
    PK_COMPARE(l.join(u'/'), PkString("a/b/c"));

    PkStringList one{"a"};
    PK_COMPARE(one.join(u'/'), PkString("a"));

    PkStringList empty;
    PK_COMPARE(empty.join(u'/'), PkString(""));

    // 非 ASCII 分隔符：pkCharToString 的 UTF-8 编码路径（2 字节与 3 字节）
    // 要真的编对，否则这里会得到乱码。
    PK_COMPARE(l.join(u'·'), PkString("a·b·c"));    // · U+00B7，2 字节
    PK_COMPARE(l.join(u'、'), PkString("a、b、c"));    // 、U+3001，3 字节

    // 两个 join 重载在同样的分隔符上必须给出同样的结果
    PK_COMPARE(l.join(u'/'), l.join(PkString("/")));
}

// ---------------------------------------------------------------------------
// filter
// ---------------------------------------------------------------------------

void PkStringListTest::filterIsSubstringMatch()
{
    // 实测：{"apple","banana","cherry"}.filter("an") → 1 个命中: banana
    //       ← **子串包含，非前缀**
    PkStringList l{"apple", "banana", "cherry"};

    const PkStringList hit = l.filter(PkString("an"));
    PK_COMPARE(hit.size(), 1);
    PK_COMPARE(hit.at(0), PkString("banana"));

    // 前缀匹配的话 "an" 一个都不该命中——这条把"写成 startsWith"挡在门外
    PK_VERIFY(hit.size() != 0);

    // 多命中，保持原顺序
    const PkStringList multi = l.filter(PkString("a"));
    PK_COMPARE(multi.size(), 2);
    PK_COMPARE(multi.at(0), PkString("apple"));
    PK_COMPARE(multi.at(1), PkString("banana"));

    // 无命中 → 空表
    PK_COMPARE(l.filter(PkString("zzz")).size(), 0);

    // 空 needle → 全部命中（与 QString::contains("") 以及 PkString::contains("")
    // 一致，都是真）
    PK_COMPARE(l.filter(PkString("")).size(), 3);

    // 整串相等也是子串
    PK_COMPARE(l.filter(PkString("cherry")).size(), 1);

    // needle 比元素长 → 不命中
    PK_COMPARE(l.filter(PkString("applepie")).size(), 0);

    // 返回的是 PkStringList，能接着调专有方法（类型没退化）
    PK_COMPARE(l.filter(PkString("a")).join(PkString("|")), PkString("apple|banana"));

    // filter 是 const 路径：不得 detach
    PkStringList a{"apple", "banana"};
    PkStringList b(a);
    (void)a.filter(PkString("a"));
    PK_VERIFY(a.PkIsSharedWith(b));

    // 默认是区分大小写（实参只传一个时走 PkCaseSensitive）
    //
    // **别写成 `PK_COMPARE(mixed.filter(...).at(0), ...)`**：PK_COMPARE 的第一件事
    // 是 `const auto &pkCompareActual_ = (actual);`，而 at() 返回的是**引用**——
    // filter 返回的临时列表在那条语句结束时就没了，后面几行再用这个引用就是读
    // 已释放的内存（实测：段错误在 PkString::operator== 里，stdout 全缓冲还把
    // 崩溃前的输出一起吞掉，现场极难认）。返回值先落到具名变量上再断言。
    // 返回**值**的方法（size()/join()）没有这个问题，prvalue 会被延长寿命。
    PkStringList mixed{"Apple", "apple"};
    const PkStringList cased = mixed.filter(PkString("App"));
    PK_COMPARE(cased.size(), 1);
    PK_COMPARE(cased.at(0), PkString("Apple"));
}

void PkStringListTest::filterCaseInsensitive()
{
    // 8 个真实调用点里有 5 个显式传 CaseInsensitive（资源/标签搜索框），
    // 所以第二参不是摆设。
    PkStringList l{"Apple", "BANANA", "cherry"};

    const PkStringList ci = l.filter(PkString("an"), PkCaseInsensitive);
    PK_COMPARE(ci.size(), 1);
    PK_COMPARE(ci.at(0), PkString("BANANA"));

    // 大小写两个方向都要能匹配上
    PK_COMPARE(l.filter(PkString("APP"), PkCaseInsensitive).size(), 1);
    PK_COMPARE(l.filter(PkString("app"), PkCaseInsensitive).size(), 1);
    PK_COMPARE(l.filter(PkString("CHERRY"), PkCaseInsensitive).size(), 1);

    // 正向对照：同样的输入在区分大小写模式下**不**命中——
    // 没有这一条，"cs 参数根本没被读"也会全绿
    PK_COMPARE(l.filter(PkString("APP"), PkCaseSensitive).size(), 0);
    PK_COMPARE(l.filter(PkString("banana"), PkCaseSensitive).size(), 0);
    PK_COMPARE(l.filter(PkString("banana"), PkCaseInsensitive).size(), 1);

    // 已知的能力缺口：**大小写折叠只覆盖 ASCII**。非 ASCII 在不区分大小写模式
    // 下退回逐码元精确比较——同样的字符仍然匹配得上（只是大小写不同的变体匹配
    // 不上）。这里钉住的是"退化方向安全"：不会误匹配，只会漏匹配。
    PkStringList cjk{"中文", "Äpfel"};
    PK_COMPARE(cjk.filter(PkString("中"), PkCaseInsensitive).size(), 1);
    // Ä(U+00C4) 与 ä(U+00E4) 折叠不了 —— 记录现状，R-13 补上 PkString 的
    // 大小写折叠之后这一条应当改成命中 1
    PK_COMPARE(cjk.filter(PkString("äpfel"), PkCaseInsensitive).size(), 0);
}

// ---------------------------------------------------------------------------
// removeDuplicates / replaceInStrings / sort
// ---------------------------------------------------------------------------

void PkStringListTest::removeDuplicatesReturnsCountAndKeepsFirstOrder()
{
    // 实测：{"b","a","b","c","a"} → 返回 2，结果 = b,a,c
    //       ← 返回删除个数，**保留首次出现顺序**（不是排序后去重）
    PkStringList l{"b", "a", "b", "c", "a"};
    PK_COMPARE(l.removeDuplicates(), 2);
    PK_COMPARE(l.size(), 3);
    PK_COMPARE(l.at(0), PkString("b"));
    PK_COMPARE(l.at(1), PkString("a"));
    PK_COMPARE(l.at(2), PkString("c"));

    // 无重复 → 返回 0，内容不变
    PkStringList uniq{"x", "y", "z"};
    PK_COMPARE(uniq.removeDuplicates(), 0);
    PK_COMPARE(uniq.size(), 3);
    PK_COMPARE(uniq.at(0), PkString("x"));

    // 全同 → 只剩 1 个
    PkStringList same{"q", "q", "q", "q"};
    PK_COMPARE(same.removeDuplicates(), 3);
    PK_COMPARE(same.size(), 1);
    PK_COMPARE(same.at(0), PkString("q"));

    // 空表
    PkStringList empty;
    PK_COMPARE(empty.removeDuplicates(), 0);
    PK_COMPARE(empty.size(), 0);

    // 单元素
    PkStringList one{"a"};
    PK_COMPARE(one.removeDuplicates(), 0);
    PK_COMPARE(one.size(), 1);

    // 空串也是一个正常的值，参与去重
    PkStringList withEmpty{"", "a", ""};
    PK_COMPARE(withEmpty.removeDuplicates(), 1);
    PK_COMPARE(withEmpty.size(), 2);
    PK_COMPARE(withEmpty.at(0), PkString(""));
    PK_COMPARE(withEmpty.at(1), PkString("a"));

    // 区分大小写（Qt 的 removeDuplicates 没有 cs 参数，一律精确比较）
    PkStringList cased{"A", "a"};
    PK_COMPARE(cased.removeDuplicates(), 0);
    PK_COMPARE(cased.size(), 2);

    // **没有重复时不 detach**（与 Qt 的实现同构：它只在 j != i 时才写）
    PkStringList shared{"x", "y"};
    PkStringList sharer(shared);
    PK_COMPARE(sharer.removeDuplicates(), 0);
    PK_VERIFY(shared.PkIsSharedWith(sharer));

    // 有重复时照常 detach，另一边一个字节不变
    PkStringList d{"x", "x"};
    PkStringList e(d);
    PK_COMPARE(e.removeDuplicates(), 1);
    PK_VERIFY(!d.PkIsSharedWith(e));
    PK_COMPARE(d.size(), 2);
    PK_COMPARE(e.size(), 1);
}

void PkStringListTest::replaceInStringsRewritesInPlace()
{
    // 唯一调用点 libs/ui/animation/KisDlgImportVideoAnimation.cpp:246 的形态：
    //   frameFileList.replaceInStrings("output_", <绝对路径前缀> + "output_");
    PkStringList l{"output_0001.png", "output_0002.png"};
    l.replaceInStrings(PkString("output_"), PkString("/tmp/dir/output_"));
    PK_COMPARE(l.at(0), PkString("/tmp/dir/output_0001.png"));
    PK_COMPARE(l.at(1), PkString("/tmp/dir/output_0002.png"));

    // 原地修改 + 返回自身引用（调用点丢弃返回值，但语义要对）
    PkStringList r{"aXa"};
    PkStringList &ref = r.replaceInStrings(PkString("X"), PkString("Y"));
    PK_VERIFY(&ref == &r);
    PK_COMPARE(r.at(0), PkString("aYa"));

    // 一个元素里多处命中，全都换
    PkStringList multi{"abab"};
    multi.replaceInStrings(PkString("ab"), PkString("Z"));
    PK_COMPARE(multi.at(0), PkString("ZZ"));

    // 不命中的元素原样保留
    PkStringList mixed{"hit_x", "miss"};
    mixed.replaceInStrings(PkString("hit"), PkString("HIT"));
    PK_COMPARE(mixed.at(0), PkString("HIT_x"));
    PK_COMPARE(mixed.at(1), PkString("miss"));

    // after 为空串 = 删掉 before
    PkStringList del{"a-b-c"};
    del.replaceInStrings(PkString("-"), PkString(""));
    PK_COMPARE(del.at(0), PkString("abc"));

    // 首尾命中都要覆盖到（边界最容易写漏）
    PkStringList edges{"xxAxx"};
    edges.replaceInStrings(PkString("xx"), PkString("-"));
    PK_COMPARE(edges.at(0), PkString("-A-"));

    // before 比元素长 / 元素为空 → 原样
    PkStringList shortEl{"a", ""};
    shortEl.replaceInStrings(PkString("abcdef"), PkString("Z"));
    PK_COMPARE(shortEl.at(0), PkString("a"));
    PK_COMPARE(shortEl.at(1), PkString(""));

    // before 为空串：本实现定成 no-op（Qt 在这里会把 after 插到每个字符之间，
    // 但那是个没有调用点的畸形边界，不猜它的确切规则）。钉住现状。
    PkStringList emptyBefore{"abc"};
    emptyBefore.replaceInStrings(PkString(""), PkString("Z"));
    PK_COMPARE(emptyBefore.at(0), PkString("abc"));

    // 不区分大小写
    PkStringList ci{"HeLLo"};
    ci.replaceInStrings(PkString("hello"), PkString("bye"), PkCaseInsensitive);
    PK_COMPARE(ci.at(0), PkString("bye"));

    // 正向对照：区分大小写时同样的输入不该被替换
    PkStringList cs{"HeLLo"};
    cs.replaceInStrings(PkString("hello"), PkString("bye"), PkCaseSensitive);
    PK_COMPARE(cs.at(0), PkString("HeLLo"));

    // after 里含 before 时不得无限展开（一趟扫描，不回头重扫）
    PkStringList grow{"a"};
    grow.replaceInStrings(PkString("a"), PkString("aa"));
    PK_COMPARE(grow.at(0), PkString("aa"));
}

void PkStringListTest::sortOrders()
{
    // 区分大小写（默认）：按 PkString::operator< 的逐码元序，
    // 所以大写字母（U+0041..）排在小写字母（U+0061..）前面
    PkStringList l{"banana", "Apple", "cherry"};
    l.sort();
    PK_COMPARE(l.at(0), PkString("Apple"));
    PK_COMPARE(l.at(1), PkString("banana"));
    PK_COMPARE(l.at(2), PkString("cherry"));

    // 不区分大小写：Apple / banana / cherry 按字母序
    PkStringList ci{"cherry", "Apple", "banana"};
    ci.sort(PkCaseInsensitive);
    PK_COMPARE(ci.at(0), PkString("Apple"));
    PK_COMPARE(ci.at(1), PkString("banana"));
    PK_COMPARE(ci.at(2), PkString("cherry"));

    // 正向对照：同样的输入在区分大小写下顺序不同——
    // 没有这一条，"cs 参数没被读"也会全绿
    PkStringList cs{"apple", "Banana"};
    cs.sort(PkCaseSensitive);
    PK_COMPARE(cs.at(0), PkString("Banana"));   // 'B'(0x42) < 'a'(0x61)
    PkStringList cs2{"apple", "Banana"};
    cs2.sort(PkCaseInsensitive);
    PK_COMPARE(cs2.at(0), PkString("apple"));

    // 前缀关系：短的在前
    PkStringList pre{"abc", "ab"};
    pre.sort();
    PK_COMPARE(pre.at(0), PkString("ab"));

    // 空表与单元素不崩
    PkStringList empty;
    empty.sort();
    PK_COMPARE(empty.size(), 0);
    PkStringList one{"z"};
    one.sort();
    PK_COMPARE(one.size(), 1);

    // 已排序的表再排一次不变（幂等）
    PkStringList idem{"a", "b", "c"};
    idem.sort();
    PK_COMPARE(idem.join(PkString(",")), PkString("a,b,c"));
}

// ---------------------------------------------------------------------------
// COW —— 派生类新增的写方法是新的漏洞面
// ---------------------------------------------------------------------------

void PkStringListTest::cowIsolation()
{
    PkStringList a{"1", "2", "3"};
    PkStringList b(a);

    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);

    b.append(PkString("4"));
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.size(), 3);
    PK_COMPARE(b.size(), 4);
    PK_COMPARE(a.join(PkString(",")), PkString("1,2,3"));
    PK_COMPARE(b.join(PkString(",")), PkString("1,2,3,4"));
}

void PkStringListTest::stringListWritersDetach()
{
    // **本任务最容易漏的一格**：基类的写方法 test_pklist 已经验过了；
    // sort / removeDuplicates / replaceInStrings 是本类新增的，各自直接碰
    // this->m_d，每一个都必须经 PkMut()。漏一个就是共享的两个列表互相污染。
    //
    // 逐个「共享 → 调一次 → 不再共享 且 另一边一个字节不变」。

    // sort
    {
        PkStringList a{"c", "a", "b"};
        PkStringList b(a);
        b.sort();
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.join(PkString(",")), PkString("c,a,b"));   // 另一边没被排序
        PK_COMPARE(b.join(PkString(",")), PkString("a,b,c"));
    }

    // sort(PkCaseInsensitive) 走的是另一条分支，单独压一遍
    {
        PkStringList a{"C", "a", "B"};
        PkStringList b(a);
        b.sort(PkCaseInsensitive);
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.join(PkString(",")), PkString("C,a,B"));
        PK_COMPARE(b.join(PkString(",")), PkString("a,B,C"));
    }

    // removeDuplicates（有重复 → 必须 detach）
    {
        PkStringList a{"x", "x", "y"};
        PkStringList b(a);
        PK_COMPARE(b.removeDuplicates(), 1);
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.join(PkString(",")), PkString("x,x,y"));
        PK_COMPARE(b.join(PkString(",")), PkString("x,y"));
    }

    // replaceInStrings
    {
        PkStringList a{"foo", "bar"};
        PkStringList b(a);
        b.replaceInStrings(PkString("o"), PkString("0"));
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.join(PkString(",")), PkString("foo,bar"));
        PK_COMPARE(b.join(PkString(",")), PkString("f00,bar"));
    }

    // operator<< / operator+=（重新声明的那几个，同样要走基类的 append → PkMut）
    {
        PkStringList a{"a"};
        PkStringList b(a);
        b << PkString("z");
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.size(), 1);
        PK_COMPARE(b.size(), 2);
    }
    {
        PkStringList a{"a"};
        PkStringList b(a);
        b += PkString("z");
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.size(), 1);
        PK_COMPARE(b.size(), 2);
    }
    {
        PkStringList a{"a"};
        PkStringList b(a);
        PkStringList more{"y", "z"};
        b << more;
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.size(), 1);
        PK_COMPARE(b.size(), 3);
    }
    {
        PkStringList a{"a"};
        PkStringList b(a);
        PkStringList more{"y", "z"};
        b += more;
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.size(), 1);
        PK_COMPARE(b.size(), 3);
    }
}

void PkStringListTest::constMethodsDoNotDetach()
{
    // 读路径一条都不许 detach（join / filter 各调多次，共享状态原样保持）
    PkStringList a{"apple", "banana"};
    PkStringList b(a);
    PK_COMPARE(a.PkUseCount(), 2L);

    for (int i = 0; i < 10; ++i) {
        (void)a.join(PkString(","));
        (void)a.join(u'/');
        (void)a.filter(PkString("a"));
        (void)a.filter(PkString("A"), PkCaseInsensitive);
        (void)a.size();
        (void)a.at(0);
    }

    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_COMPARE(a.size(), 2);
}

// ---------------------------------------------------------------------------
// 链式操作符：类型不得退化
// ---------------------------------------------------------------------------

void PkStringListTest::chainedOperatorsKeepDerivedType()
{
    // 实测：(c << "p" << "q").join("|") = 'p|q'
    //       ← 链式后仍是 QStringList，能直接 join
    //
    // **编得过就是证明**：不重新声明 operator<< 的话，第一个 << 就返回
    // PkList<PkString>&，而 PkList<PkString> 没有 join —— 这一行编不过。
    PkStringList c;
    PK_COMPARE((c << PkString("p") << PkString("q")).join(PkString("|")), PkString("p|q"));
    PK_COMPARE(c.size(), 2);

    // 字面量形态（调用点最常见）
    PkStringList lit;
    PK_COMPARE((lit << "a" << "b").join(PkString(", ")), PkString("a, b"));

    // += 同样
    PkStringList plus;
    PK_COMPARE((plus += PkString("m")).join(PkString("|")), PkString("m"));

    // 列表版重载也要保持类型
    PkStringList base{"a"};
    PkStringList more{"b", "c"};
    PK_COMPARE((base << more).join(PkString("-")), PkString("a-b-c"));

    PkStringList base2{"a"};
    PK_COMPARE((base2 += more).join(PkString("-")), PkString("a-b-c"));

    // 链式之后接别的专有方法也行（不是只有 join 一个能用）
    PkStringList dup;
    dup << "x" << "x" << "y";
    PK_COMPARE(dup.removeDuplicates(), 1);
    PK_COMPARE(dup.join(PkString(",")), PkString("x,y"));

    // 链式的结果能继续参与 filter，且 filter 的返回值也没退化
    PkStringList f;
    PK_COMPARE((f << "apple" << "banana").filter(PkString("an")).join(PkString(",")),
               PkString("banana"));
}

void PkStringListTest::selfAssignmentIsSafe()
{
    PkStringList a{"1", "2", "3"};
    // 经引用绕一道：直接写 a = a 会被 -Wself-assign-overloaded 拦下，
    // 而真实调用点里的自赋值本来就是通过别名/引用发生的。
    PkStringList &alias = a;
    a = alias;

    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.size(), 3);
    PK_COMPARE(a.join(PkString(",")), PkString("1,2,3"));

    // 自赋值之后照常可写、专有方法照常可用
    a << PkString("4");
    PK_COMPARE(a.join(PkString(",")), PkString("1,2,3,4"));

    // 共享状态下的自赋值
    PkStringList b{"5", "6"};
    PkStringList c(b);
    PkStringList &bAlias = b;
    b = bAlias;
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_VERIFY(b.PkIsSharedWith(c));
    PK_COMPARE(b.join(PkString(",")), PkString("5,6"));

    // 自赋值之后 COW 仍然生效
    b.sort();
    PK_VERIFY(!b.PkIsSharedWith(c));
    PK_COMPARE(c.join(PkString(",")), PkString("5,6"));
}

PK_TEST_MAIN(PkStringListTest)
