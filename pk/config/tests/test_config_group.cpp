#include "test_config_group.h"
#include "../PkConfigStore.h"
#include "../PkConfigGroup.h"
#include "../PkSharedConfig.h"
#include "../PkConfigColor.h"

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

    PkConfigColor fallbackColor(99, 99, 99);
    PkConfigColor readColor = g.readEntry("color", fallbackColor);
    PK_VERIFY(readColor == fallbackColor);
    PkConfigColor customColor(10, 20, 30, 255);
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

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_config_group.inc"

int run_config_group_tests(int argc, char **argv)
{
    TestConfigGroup tc;
    return PkTest::qExec(&tc, argc, argv);
}
