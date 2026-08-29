#include "test_config_group.h"
#include "../PkConfigStore.h"
#include "../PkConfigGroup.h"
#include "../PkSharedConfig.h"
#include "../color/PkColor.h"

#include <atomic>
#include <thread>
#include <type_traits>
#include <vector>

void TestConfigGroup::storeBasicGetSet()
{
    PkConfigStore &store = PkConfigStore::instance();
    store.set("grid", "gridmainstyle", "1");
    PK_COMPARE(store.get("grid", "gridmainstyle", "0"), PkString("1"));
    PK_COMPARE(store.get("grid", "missingKey", "fallback"), PkString("fallback"));
    PK_VERIFY(store.has("grid", "gridmainstyle"));
    PK_VERIFY(!store.has("grid", "missingKey"));
    store.remove("grid", "gridmainstyle");
    PK_VERIFY(!store.has("grid", "gridmainstyle"));
}

void TestConfigGroup::readWriteAllTypes()
{
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("grid");

    PK_COMPARE(g.readEntry("style", 0), 0);           // 缺省值
    g.writeEntry("style", 2);
    PK_COMPARE(g.readEntry("style", 0), 2);

    PK_COMPARE(g.readEntry("enabled", true), true);
    g.writeEntry("enabled", false);
    PK_COMPARE(g.readEntry("enabled", true), false);

    PK_COMPARE(g.readEntry("thickness", 12.5), 12.5);
    g.writeEntry("thickness", 3.25);
    PK_COMPARE(g.readEntry("thickness", 12.5), 3.25);

    PK_COMPARE(g.readEntry("name", PkString("default")), PkString("default"));
    g.writeEntry("name", PkString("custom"));
    PK_COMPARE(g.readEntry("name", PkString("default")), PkString("custom"));

    PkColor fallbackColor(99, 99, 99);
    PkColor readColor = g.readEntry("color", fallbackColor);
    PK_VERIFY(readColor == fallbackColor);
    PkColor customColor(10, 20, 30, 255);
    g.writeEntry("color", customColor);
    PK_VERIFY(g.readEntry("color", fallbackColor) == customColor);

    PkPoint fallbackPoint(16, 16);
    PK_VERIFY(g.readEntry("spacing", fallbackPoint).x() == 16);
    g.writeEntry("spacing", PkPoint(4, 8));
    PkPoint readPoint = g.readEntry("spacing", fallbackPoint);
    PK_COMPARE(readPoint.x(), 4);
    PK_COMPARE(readPoint.y(), 8);

    // PkList/PkArrayContainer 的方法是 size()，不是本任务 brief 草稿假定的
    // PkSize()——已按 pk/container/PkArrayContainer.h 的真实签名改写。
    PkStringList fallbackList;
    PK_VERIFY(g.readEntry("blacklist", fallbackList).size() == 0);
    PkStringList list;
    list.append(PkString("a"));
    list.append(PkString("b"));
    g.writeEntry("blacklist", list);
    PK_COMPARE(g.readEntry("blacklist", fallbackList).size(), 2);
}

void TestConfigGroup::hasKeyDeleteEntrySync()
{
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("toolgroup");
    PK_VERIFY(!g.hasKey("someKey"));
    g.writeEntry("someKey", 42);
    PK_VERIFY(g.hasKey("someKey"));
    g.deleteEntry("someKey");
    PK_VERIFY(!g.hasKey("someKey"));
    g.sync();   // 不落盘，只保证不抛异常/不崩，见 Global Constraints「不做真实磁盘持久化」
}

void TestConfigGroup::sameGroupNameSharesStorage()
{
    // KSharedConfig::openConfig()->group(name) 每次调用返回的是同一份底层存储的句柄——
    // 两次不同的 group() 调用、同一个 name，必须能看到彼此写入的值。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    cfg->group("shared").writeEntry("x", 7);
    PK_COMPARE(cfg->group("shared").readEntry("x", 0), 7);
}

void TestConfigGroup::emptyStringListDiffersFromMissingKey()
{
    // 自审要点：「key 不存在」与「key 存在但存的是空 PkStringList」必须能区分——
    // 都不能悄悄退化成同一种情况。fallback 用一个非空列表，这样两条分支即使
    // 都返回「大小为 0」也不会因为 fallback 恰好也是空列表而巧合蒙混过关。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("emptylist");

    PkStringList nonEmptyFallback;
    nonEmptyFallback.append(PkString("fallback-marker"));

    // key 不存在 → 必须原样返回 fallback（包括它的内容，不是随便一个空列表）。
    PkStringList missing = g.readEntry("neverWritten", nonEmptyFallback);
    PK_COMPARE(missing.size(), 1);
    PK_COMPARE(missing.at(0), PkString("fallback-marker"));

    // key 存在、显式写入空列表 → 必须返回空列表，不是 fallback。
    g.writeEntry("explicitlyEmpty", PkStringList());
    PK_VERIFY(g.hasKey("explicitlyEmpty"));
    PkStringList stored = g.readEntry("explicitlyEmpty", nonEmptyFallback);
    PK_COMPARE(stored.size(), 0);
}

void TestConfigGroup::doubleRoundTripsBeyondSixDecimals()
{
    // 评审 Important 项：旧实现用 std::to_string(double) 序列化，固定 6 位小数——
    // 需要超过 6 位精度的值会被截断，极小的值会被直接截成 "0.000000"（读回来是
    // 0.0，跟真的写入 0 完全分不出来，属于静默数据损坏）。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("doubleprecision");

    // 需要 9 位小数才能精确表示的值：6 位精度会截成 0.123457（丢最后两位）。
    const double highPrecision = 0.123456789;
    g.writeEntry("highPrecision", highPrecision);
    PK_COMPARE(g.readEntry("highPrecision", 0.0), highPrecision);

    // std::to_string(0.0000001) == "0.000000"——旧实现下这个值会静默变成 0，
    // 且与「真的存了 0」无法区分。
    const double verySmall = 0.0000001;
    g.writeEntry("verySmall", verySmall);
    double readBack = g.readEntry("verySmall", -1.0);
    PK_VERIFY(readBack != 0.0);
    PK_COMPARE(readBack, verySmall);

    // 顺带确认「真的写 0」依然读回 0，不受上面那条修复影响。
    g.writeEntry("trueZero", 0.0);
    PK_COMPARE(g.readEntry("trueZero", -1.0), 0.0);
}

void TestConfigGroup::explicitTemplateReadEntryForms()
{
    // 最终评审 I-1(a)：真实调用点大量使用 `g.readEntry<T>("k", def)` 这种显式
    // 模板实参形式（现有 7 个非模板重载不支持在成员函数名后面接 <T>），
    // 分别覆盖 bool / int / PkString（对应真实调用点里的 readEntry<QString>，
    // 经 pk/string/compat/QString 的 #define QString PkString 之后落到这里）。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("templateread");

    PK_COMPARE(g.readEntry<bool>("flag", true), true);
    g.writeEntry("flag", false);
    PK_COMPARE(g.readEntry<bool>("flag", true), false);

    PK_COMPARE(g.readEntry<int>("count", 7), 7);
    g.writeEntry("count", 42);
    PK_COMPARE(g.readEntry<int>("count", 7), 42);

    PK_COMPARE(g.readEntry<PkString>("label", PkString("fallback")), PkString("fallback"));
    g.writeEntry("label", PkString("actual"));
    PK_COMPARE(g.readEntry<PkString>("label", PkString("fallback")), PkString("actual"));
}

void TestConfigGroup::writeEntryQuint32IsUnambiguousAndRoundTrips()
{
    // 最终评审 I-1(c)：`writeEntry("k", quint32(v))`（unsigned int → bool/int/
    // double 三个同等排名的标准转换）在只有非模板重载时是 ambiguous call。
    // quint32 本身在本测试文件里不是裸类型（未拉 Qt 兼容垫片），用同底层类型的
    // unsigned int 直接验证——覆盖的是同一处重载决议路径。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("quint32write");

    unsigned int enumLikeValue = 2;
    g.writeEntry("gridmainstyle", enumLikeValue);
    // 真实调用点写入后固定走 int 重载读回（枚举值转 quint32 只是写入路径的中间态）。
    PK_COMPARE(g.readEntry("gridmainstyle", 0), 2);
}

void TestConfigGroup::readEntryWithStringLiteralDoesNotBindToBool()
{
    // 最终评审 I-1(d)，最严重的一项：`readEntry("k", "literal")` 在没有 const
    // char* 精确匹配重载时，会靠标准布尔转换悄悄绑定到 bool 重载——不报错，
    // 调用方以为拿到的是字符串，实际类型是 bool。static_assert 在编译期确认
    // decltype 落在 PkString 上，这条断言失败就说明回归又发生了。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("literal_test");

    auto v = g.readEntry("someKey", "some default text");
    static_assert(std::is_same<decltype(v), PkString>::value,
                  "readEntry(key, const char*) must bind to the PkString overload, not bool");
    PK_COMPARE(v, PkString("some default text"));
}

void TestConfigGroup::twoArgConstructorFromSharedConfigHandle()
{
    // 最终评审 I-1(b)：真实调用点里的构造形式是
    // `KConfigGroup cfg(KSharedConfig::openConfig(), "Group")`，不是经
    // `.group(name)` 拿到句柄。config 实参只做类型检查，不参与存储路径——
    // 用同一个 group 名字通过两种构造方式都应该看到彼此写入的值。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup viaCtor(cfg, "twoarg");
    viaCtor.writeEntry("x", 5);

    PkConfigGroup viaAccessor = cfg->group("twoarg");
    PK_COMPARE(viaAccessor.readEntry("x", 0), 5);
}

void TestConfigGroup::colorReadEntryRejectsOutOfRangeSegments()
{
    // M-1：越界分量（如 300、-5）必须视为格式错误退回 defaultValue，不能构造出
    // 越界颜色（PkConfigColor 时代会 static_cast<uint8_t> 悄悄环绕/截断成无关颜色，
    // PkColor 时代会构造出无效色）——两种"数据格式不对"的场景必须有一致的兜底行为。
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup g = cfg->group("colorrange");
    PkColor fallback(1, 2, 3, 4);

    PkConfigStore::instance().set("colorrange", "outOfRange", PkString("300,-5,0,255"));
    PK_VERIFY(g.readEntry("outOfRange", fallback) == fallback);

    // 边界值本身（0 与 255）仍然合法，不应被这条新检查误伤。
    g.writeEntry("boundary", PkColor(0, 255, 0, 255));
    PkColor boundary = g.readEntry("boundary", fallback);
    PK_VERIFY(boundary == PkColor(0, 255, 0, 255));
}

void TestConfigGroup::deleteGroupClearsEveryKeyAndPreservesOtherGroups()
{
    PkSharedConfig *cfg = PkSharedConfig::openConfig();
    PkConfigGroup root = cfg->group(PkString());
    PkConfigGroup other = cfg->group("unrelated-group");

    root.writeEntry("generic-root-key", 17);
    root.writeEntry("onionSkinOpacity_37", 203);
    root.writeEntry("ExportConfiguration-unseen", PkString("payload"));
    other.writeEntry("must-survive", 91);

    root.deleteGroup();

    PK_VERIFY(!root.hasKey("generic-root-key"));
    PK_VERIFY(!root.hasKey("onionSkinOpacity_37"));
    PK_VERIFY(!root.hasKey("ExportConfiguration-unseen"));
    PK_COMPARE(other.readEntry("must-survive", 0), 91);
    other.deleteGroup();
}

void TestConfigGroup::concurrentReadsAndGroupClearsAreSafe()
{
    PkConfigGroup group = PkSharedConfig::openConfig()->group("concurrent-clear");
    group.deleteGroup();
    group.writeEntry("value", 42);

    std::atomic<bool> start{false};
    std::atomic<bool> invalidRead{false};
    std::vector<std::thread> readers;
    for (int reader = 0; reader < 4; ++reader) {
        readers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
            }
            for (int iteration = 0; iteration < 4000; ++iteration) {
                const int value = group.readEntry("value", -1);
                if (value != -1 && value != 42) {
                    invalidRead.store(true, std::memory_order_relaxed);
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        group.deleteGroup();
        group.writeEntry("value", 42);
    }
    for (std::thread &reader : readers) {
        reader.join();
    }

    PK_VERIFY(!invalidRead.load(std::memory_order_relaxed));
    group.deleteGroup();
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_config_group.inc"

int run_config_group_tests(int argc, char **argv)
{
    TestConfigGroup tc;
    return PkTest::qExec(&tc, argc, argv);
}
