// M-3（最终评审，非必做，顺手做）：5 个 compat 垫片里只有
// compat/kconfiggroup.h（经 Task 2 的 graft_run.sh）与 compat/KisMimeDatabase.h
// （经本目录新增的 shim_check.sh）被真正编译过。剩下 3 个
// （compat/KConfigGroup、compat/KSharedConfig、compat/ksharedconfig.h）
// 从没有被任何测试/试接触碰过。本文件只做 -fsyntax-only 的编译期存在性检查
// ——确认三个垫片各自都能被 #include 并解析出预期符号，不做运行时验证
// （PkConfigGroup/PkSharedConfig 本身的行为已经被 test_pkconfig 与
// graft_run.sh 覆盖，这里只补"垫片这层间接自己没炸"）。
#include <KConfigGroup>
#include <KSharedConfig>
#include <ksharedconfig.h>

// 三个垫片的 #define 都指向同一对真实类型，写一次就同时验证了三条 #include。
static_assert(sizeof(KConfigGroup) == sizeof(PkConfigGroup), "KConfigGroup 应解析到 PkConfigGroup");
static_assert(sizeof(KSharedConfig) == sizeof(PkSharedConfig), "KSharedConfig 应解析到 PkSharedConfig");

int compat_shims_probe_unused_entry_point()
{
    KSharedConfigPtr cfg = KSharedConfig::openConfig();
    KConfigGroup group(cfg, "probe");
    return group.hasKey("x") ? 1 : 0;
}
