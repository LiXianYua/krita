// pointer_difftest.cpp —— pk/pointer 与真 Qt5 的逐输入对拍。
//
// **这份文件被编译两次**（run_oracle.sh 分别带 -DORACLE_QT_SIDE 与不带），产出两个
// 目标文件，链进同一个可执行文件；main.cpp 是第三个 TU（裁决 R2：main() 必须待在
// 独立 TU 里，否则同一份 pointer_difftest.cpp 编两次会撞出两个 main 的重复定义）。
//
// ── 为什么两侧的辅助类型（Payload 等）要塞进匿名 namespace ──────────────
// 这份源码在两次编译里，宏分支之外的部分（Payload/PayloadDerived/…/观测与解释器
// 函数）逐字节相同。如果它们留在全局作用域，类内定义的成员函数是隐式 inline，
// 链接器会把两个目标文件里"看起来一样"的符号当同一个 vague-linkage/COMDAT 符号
// 折叠成一份——两份同名 `struct Base` 定义会被折叠成一份，其中一侧的计数器
// 读到另一侧头上。这里的后果会更隐蔽：折叠后
// Qt 侧与 Pk 侧会共享同一份 Payload::live/nextId，跑起来大概率不是编译错误，
// 而是安静地把两侧观测量搅在一起，产出看似合理实则无意义的 mismatch 数字。
// 塞进匿名 namespace 给每个 TU 一份内部链接的私有副本，彻底堵死这条折叠路径。
//
// ── 输出契约（main.cpp 读这两种行，别的都是给人看的）──────────────────
//     DIFF total=<N> mismatch=<M>      恰好一行，main.cpp 末尾打
//     DIFFTAG <api> <tag> <count>      一类差异一行
// **退出码必须是 0，即使 M>0**——已声明的偏离不算失败，退出码只表示"跑完没崩"。
//
// ── 形态契约的两条自证 ──────────────────────────────────────────────
// static_assert(!is_same<QSharedPointer<X>,PkSharedPointer<X>>) 与 ldd 看到
// libQt5Core.so.5，两条都放在 main.cpp（那是唯一同时看得到两侧真实类型名字、且
// 只编译一次的 TU；这份文件任一次编译都只看得到其中一侧，没法在本文件里比较两个
// 类型是不是同一个）——细节见 main.cpp 顶部注释。

#ifdef ORACLE_QT_SIDE
#  include <QSharedPointer>
// compat 垫片被拉进 -I 时，QSharedPointer 会先被 #define 成 PkSharedPointer，
// 下面这个 #ifdef 直接在预处理期抓现行，比任何运行期比较都早、都死。
#  ifdef QSharedPointer
#    error "QSharedPointer 已被宏定义——compat 垫片被拉进了 -I，检查 run_oracle.sh 的 INCS"
#  endif
#  include <QScopedPointer>
#  include <QWeakPointer>
#  include <QScopedArrayPointer>
#  define SP QSharedPointer
#  define WP QWeakPointer
#  define CP QScopedPointer
#  define AP QScopedArrayPointer
#else
#  include "PkSharedPointer.h"
#  include "PkScopedPointer.h"
#  define SP PkSharedPointer
#  define WP PkWeakPointer
#  define CP PkScopedPointer
#  define AP PkScopedArrayPointer
#endif
#include "TraceOps.h"

#include <cstdio>
#include <string>

// ── 两侧函数用不同名字导出（裁决 R2）────────────────────────────────────
// main.cpp 要同时链到两侧各一份 runSharedScript/runScopedScript，两个目标文件
// 又是同一份源码编出来的，函数名不区分侧就是重复定义。用宏做 token-paste 生成
// 两个不同的外部符号：runSharedScriptQt / runSharedScriptPk。
#ifdef ORACLE_QT_SIDE
#  define ORACLE_SIDE_SUFFIX Qt
#else
#  define ORACLE_SIDE_SUFFIX Pk
#endif
#define ORACLE_CAT2(a, b) a##b
#define ORACLE_CAT(a, b) ORACLE_CAT2(a, b)
#define ORACLE_SIDE_FN(name) ORACLE_CAT(name, ORACLE_SIDE_SUFFIX)

namespace {

// ── 两侧共用同一份定义的可观测载荷类型 ─────────────────────────────────
// 全局存活计数 + 自增 id：live 覆盖引用计数导致的析构时机（智能指针唯一真正
// 可观测的语义），id 覆盖"指到哪个对象"。virtual 析构是 dynamicCast 要求的
// （dynamic_cast 要求多态类型）。
struct Payload {
    static int live;
    static int nextId;
    int id;
    int v;
    explicit Payload(int vv = 7) : id(++nextId), v(vv) { ++live; }
    virtual ~Payload() { --live; }
};
struct PayloadDerived : Payload {
    explicit PayloadDerived(int vv = 9) : Payload(vv) {}
};
struct PayloadUnrelated : Payload {};
int Payload::live = 0;
int Payload::nextId = 0;

// 空指针 + 自定义 deleter 时 Qt 依然调用 deleter（判据 D，探针 D3）；
// 这个计数器就是用来钉住这一点的可观测量。
int g_deleterCalls = 0;
struct CountingDeleter {
    void operator()(Payload *p) const
    {
        ++g_deleterCalls;
        delete p;
    }
};

// ── 观测辅助 ────────────────────────────────────────────────────────────
// 「布尔语境」统一走 static_cast<bool>：两侧都实现 operator RestrictedBool
// （safe-bool），static_cast<bool> 是唯一在两侧写法一致、又确实触发那条转换
// 路径的表达式（`if(x)`/`x?:` 没法直接拿到可打印的 0/1）。
template <class SPType> int boolCtx(const SPType &p) { return static_cast<bool>(p) ? 1 : 0; }

// 4 个强引用槽 + 2 个弱引用槽，脚本跑之间复位。
SP<Payload> g_strong[4];
WP<Payload> g_weak[2];

void resetSharedHarness()
{
    for (int i = 0; i < 4; ++i) g_strong[i] = SP<Payload>();
    for (int i = 0; i < 2; ++i) g_weak[i] = WP<Payload>();
    Payload::live = 0;
    Payload::nextId = 0;
    g_deleterCalls = 0;
}

// 每步一行：live + deleter 计数 + 4 个强引用槽（isNull/id/v/布尔语境）
// + 2 个弱引用槽（isNull/toStrongRef().isNull()/toStrongRef() 的 id/v）。
// 「为什么这些量足够」见简报 Step 2：isNull 覆盖判据 A，布尔语境覆盖判据 B，
// live 与 deleter 计数覆盖析构时机，id 覆盖"指到哪个对象"。
// v 字段必须记：只记 (isNull, id, 布尔语境) 会放过参数转发类缺陷——例如
// create() 把构造参数传错，两侧各自生成一个 id 相同的新对象（id 只看"分配
// 顺序"，与传了什么值无关），isNull/live/id 全部一致，只有 v 不同，缺了这一列
// 对拍就看不见。scoped 家族（observeScoped 的 C%d/A%d）同样记 v，两族对齐。
std::string observeShared()
{
    char buf[512];
    int off = 0;
    off += std::snprintf(buf + off, sizeof(buf) - off, "live=%d delc=%d", Payload::live, g_deleterCalls);
    for (int i = 0; i < 4; ++i) {
        const SP<Payload> &s = g_strong[i];
        int id = s.isNull() ? -1 : s->id;
        int v = s.isNull() ? -1 : s->v;
        off += std::snprintf(buf + off, sizeof(buf) - off, " S%d[n=%d id=%d v=%d b=%d]",
                              i, s.isNull() ? 1 : 0, id, v, boolCtx(s));
    }
    for (int i = 0; i < 2; ++i) {
        const WP<Payload> &w = g_weak[i];
        SP<Payload> strong = w.toStrongRef();
        int sid = strong.isNull() ? -1 : strong->id;
        int sv = strong.isNull() ? -1 : strong->v;
        off += std::snprintf(buf + off, sizeof(buf) - off, " W%d[n=%d sn=%d sid=%d sv=%d]",
                              i, w.isNull() ? 1 : 0, strong.isNull() ? 1 : 0, sid, sv);
    }
    return std::string(buf, off);
}

// ── 20 个共享/弱引用操作码的解释器 ─────────────────────────────────────
// a 恒定用作主槽（强引用槽 0..3）；b 在跨槽操作里当第二个强引用槽，
// 在牵涉弱引用的操作里用 b%2 选弱引用槽 0..1。
void interpretSharedStep(const Step &st)
{
    int a = st.a % 4;
    int b = st.b % 4;
    int wb = st.b % 2;
    switch (st.op) {
    case OpMakeNew:
        g_strong[a] = SP<Payload>(new Payload(1000 + a));
        break;
    case OpMakeNullRaw: {
        Payload *p = nullptr;
        g_strong[a] = SP<Payload>(p);
        break;
    }
    case OpMakeNullDeleter: {
        Payload *p = nullptr;
        g_strong[a] = SP<Payload>(p, CountingDeleter());
        break;
    }
    case OpMakeCreate:
        g_strong[a] = SP<Payload>::create(2000 + a);
        break;
    case OpMakeDefault:
        g_strong[a] = SP<Payload>();
        break;
    case OpMakeDerived: {
        SP<PayloadDerived> d(new PayloadDerived(3000 + a));
        g_strong[a] = SP<Payload>(d);
        break;
    }
    case OpCopy:
        // 显式拷贝构造出临时量再移动赋值进槽——练的是拷贝构造函数那条路径。
        g_strong[a] = SP<Payload>(g_strong[b]);
        break;
    case OpAssign:
        // 直接拷贝赋值——练的是 operator= 那条路径，与 OpCopy 不是同一条。
        g_strong[a] = g_strong[b];
        break;
    case OpAssignNullptr:
        g_strong[a] = nullptr;
        break;
    case OpSelfAssign:
        g_strong[a] = g_strong[a];
        break;
    case OpReset:
        g_strong[a].reset();
        break;
    case OpResetNew:
        g_strong[a].reset(new Payload(4000 + a));
        break;
    case OpResetDeleter:
        g_strong[a].reset(new Payload(5000 + a), CountingDeleter());
        break;
    case OpClear:
        g_strong[a].clear();
        break;
    case OpDynamicCastToDerived:
        g_strong[b] = SP<Payload>(g_strong[a].dynamicCast<PayloadDerived>());
        break;
    case OpDynamicCastToUnrelated:
        g_strong[b] = SP<Payload>(g_strong[a].dynamicCast<PayloadUnrelated>());
        break;
    case OpStaticCastToDerived:
        // 只经由 Payload 基类成员读 id/v，不触碰 Derived 专属状态——即使槽里
        // 实际不是 Derived 对象，也不构成访问越界。
        g_strong[b] = SP<Payload>(g_strong[a].staticCast<PayloadDerived>());
        break;
    case OpWeakFrom:
        g_weak[wb] = WP<Payload>(g_strong[a]);
        break;
    case OpStrongFromWeak:
        g_strong[a] = SP<Payload>(g_weak[wb]);
        break;
    case OpWeakAssignNullptr:
        // 不用字面量 nullptr：真 Qt5 的 QWeakPointer<T> 对非 QObject 的 T 没有
        // 任何能接 nullptr 的构造函数（唯一的裸指针构造被
        // `IfCompatible<X>`= X 派生自 QObject 的 SFINAE 挡在外面，
        // qsharedpointer_impl.h 的 QWeakPointer 类体里那条写在
        // `#ifndef QT_NO_QOBJECT` 分支里，实测确认过）。默认构造再赋值两侧都有，
        // 效果与"赋 null"等价，且不逼一侧编不过。
        g_weak[wb] = WP<Payload>();
        break;
    default:
        break;
    }
}

std::string runSharedScriptImpl(const Script &s)
{
    resetSharedHarness();
    std::string trace;
    for (size_t i = 0; i < s.size(); ++i) {
        interpretSharedStep(s[i]);
        if (i) trace += '\n';
        trace += observeShared();
    }
    return trace;
}

// ── PkScopedPointer / PkScopedArrayPointer 一族 ────────────────────────
// CP/AP 两侧都不可拷贝不可移动（判据 C）——槽数组只能存指针，"新建"就是
// delete 旧的、new 一个新的（练构造函数），"reset"是调用成员函数（练 reset()）。
// 这是与共享族的关键差异：共享族的槽可以直接赋值，标量族的槽物理上不能。
CP<Payload> *g_cp[2];
AP<Payload> *g_ap[2];
// 承接"上一次相关操作"的结果，脚本复位时清零；不是每步都更新，跨步保留上一个
// 观测值本身也是一种比对（两侧要在同样的步上更新、同样的步上保持不变）。
int g_lastTake = -1;
int g_lastDeref = -1;
int g_lastArr = -1;

void resetScopedHarness()
{
    for (int i = 0; i < 2; ++i) {
        delete g_cp[i];
        g_cp[i] = new CP<Payload>();
    }
    for (int i = 0; i < 2; ++i) {
        delete g_ap[i];
        g_ap[i] = new AP<Payload>();
    }
    Payload::live = 0;
    Payload::nextId = 0;
    g_lastTake = -1;
    g_lastDeref = -1;
    g_lastArr = -1;
}

// PkScopedArrayPointer 按用量表收窄过 API 面，没有 isNull()，只给了
// data()/operator[]/reset()——判空一律走 data()!=nullptr。
std::string observeScoped()
{
    char buf[512];
    int off = 0;
    off += std::snprintf(buf + off, sizeof(buf) - off, "live=%d", Payload::live);
    for (int i = 0; i < 2; ++i) {
        CP<Payload> &c = *g_cp[i];
        int id = c.isNull() ? -1 : c->id;
        int v = c.isNull() ? -1 : c->v;
        off += std::snprintf(buf + off, sizeof(buf) - off, " C%d[n=%d id=%d v=%d]",
                              i, c.isNull() ? 1 : 0, id, v);
    }
    for (int i = 0; i < 2; ++i) {
        AP<Payload> &ap = *g_ap[i];
        bool hasData = ap.data() != nullptr;
        int id0 = hasData ? ap[0].id : -1;
        int v0 = hasData ? ap[0].v : -1;
        off += std::snprintf(buf + off, sizeof(buf) - off, " A%d[n=%d id0=%d v0=%d]",
                              i, hasData ? 0 : 1, id0, v0);
    }
    off += std::snprintf(buf + off, sizeof(buf) - off, " lastTake=%d lastDeref=%d lastArr=%d",
                          g_lastTake, g_lastDeref, g_lastArr);
    return std::string(buf, off);
}

// ── 11 个标量/数组 scoped 操作码的解释器 ───────────────────────────────
// a/b 都在 0..1 里轮换（scoped 一族只有 2 个槽，见简报 Step 3）；标量族的 8 个
// 操作码用 a 索引 g_cp，数组族的 3 个操作码用 a 索引 g_ap，b 在需要"另一种取值"
// 时当参数（例如 arrayReset 要不要真给新数组、arrayIndex 取第几个下标）。
void interpretScopedStep(const Step &st)
{
    int a = st.a % 2;
    int b = st.b % 2;
    switch (st.op) {
    case SOpMakeNew:
        delete g_cp[a];
        g_cp[a] = new CP<Payload>(new Payload(6000 + a));
        break;
    case SOpMakeDefault:
        delete g_cp[a];
        g_cp[a] = new CP<Payload>();
        break;
    case SOpReset:
        g_cp[a]->reset();
        break;
    case SOpResetNew:
        g_cp[a]->reset(new Payload(7000 + a));
        break;
    case SOpResetSame:
        // 自赋值保护（Task 1 Step 5）：reset 成自己当前已经拿着的那个指针。
        g_cp[a]->reset(g_cp[a]->data());
        break;
    case SOpTake: {
        Payload *taken = g_cp[a]->take();
        g_lastTake = taken ? taken->id : -1;
        delete taken;  // 所有权已经转出，测试侧立刻收尾，避免泄漏
        break;
    }
    case SOpTakeThenReset: {
        Payload *taken = g_cp[a]->take();
        g_lastTake = taken ? taken->id : -1;
        g_cp[a]->reset(taken);  // 转出去又转回来（P9：所有权转出/转回）
        break;
    }
    case SOpDeref:
        // 判空之后再解引用——都不解引用空指针，两侧都是，UB 不进对拍。
        g_lastDeref = g_cp[a]->isNull() ? -1 : (**g_cp[a]).v;
        break;
    case SOpArrayMake: {
        delete g_ap[a];
        Payload *arr = new Payload[3];
        g_ap[a] = new AP<Payload>(arr);
        break;
    }
    case SOpArrayReset:
        // b 选变体：偶数给一个新的 3 元素数组，奇数 reset() 到空（无参默认值）。
        if (b == 0) g_ap[a]->reset(new Payload[3]);
        else g_ap[a]->reset();
        break;
    case SOpArrayIndex: {
        // 下标必须用未经二值收窄的 `st.a`/`st.b` 组合出来：本函数顶部的局部
        // `b` 已被 `st.b % 2` 收窄到 {0,1}，若直接 `b % 3` 会恒等于 `b` 自身，
        // `arrayMake` 造的 3 元素数组的下标 2 永远不会被观测到。改用
        // `st.a`/`st.b`（两者各自 0/1，共 4 种组合）算出 `(st.a*2+st.b)%3`，
        // 覆盖 0/1/2 全部三个下标；槽选择仍然用上面已收窄的 `a`。
        int idx = (st.a * 2 + st.b) % 3;
        g_lastArr = (g_ap[a]->data() != nullptr) ? (*g_ap[a])[idx].v : -1;
        break;
    }
    default:
        break;
    }
}

std::string runScopedScriptImpl(const Script &s)
{
    resetScopedHarness();
    std::string trace;
    for (size_t i = 0; i < s.size(); ++i) {
        interpretScopedStep(s[i]);
        if (i) trace += '\n';
        trace += observeScoped();
    }
    return trace;
}

}  // namespace

// ── 唯一从这个 TU 导出的两个符号（外部链接，main.cpp 要 extern 它们）────
std::string ORACLE_SIDE_FN(runSharedScript)(const Script &s) { return runSharedScriptImpl(s); }
std::string ORACLE_SIDE_FN(runScopedScript)(const Script &s) { return runScopedScriptImpl(s); }
