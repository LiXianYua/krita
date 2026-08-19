// R-17 Task 3 判据②：候选 1 —— libs/resources/KisDatabaseTransactionLock.{h,cpp}
// 零改动编译试接成功（见 graft_run_candidate1.sh），本文件是驱动真实、未修改
// 的 `KisDatabaseTransactionLockAdapter`/`KisDatabaseTransactionLock` 的探针
// main()——**不是** driver 降级路径（R-17 plan Task 3 描述里"全部候选撞墙才
// 写 driver 逐行复刻"那条），这里链接、调用的都是候选 1 本身真实、未修改的
// .o；这份 .cpp 只是补一个真实调用点没有的"驱动它跑起来的 main()"（乙类判据
// ②允许的形态，同 pk/config/tests/graft/graft_run.sh 对 KisCumulativeUndoData
// 的先例——那份没有专门的 kis_add_test 单测类，也是探针驱动直接调用）。
//
// 覆盖 KisDatabaseTransactionLock.h 类注释描述的完整 RAII 契约：
//   [1] 构造即 lock()（内部走 std::unique_lock 的构造语义），显式 commit()
//       之后事务提交，数据保留
//   [2] 构造后不调用 commit()、离开作用域析构 —— 自动 unlock() 回滚，数据
//       不落库
//   [3] 显式调用 KisDatabaseTransactionLock::rollback()（内部转发到
//       unlock()）—— 效果与 [2] 相同，且不会在随后析构时重复 unlock()
//       （std::unique_lock 自己记的 owns_lock 状态防重复）
#include "KisDatabaseTransactionLock.h"

#include "PkSqlDatabase.h"
#include "PkSqlQuery.h"

#include <cstdio>
#include <sqlite3.h>

namespace {

int countWhere(const char *whereName)
{
    PkSqlQuery check;
    check.prepare("SELECT COUNT(*) FROM t WHERE name = :name");
    check.bindValue(":name", PkVariant(whereName));
    if (!check.exec() || !check.next()) {
        return -1;
    }
    return check.value(0).toInt();
}

bool insertOneRow(const char *name)
{
    PkSqlQuery q;
    q.prepare("INSERT INTO t (name) VALUES (:name)");
    q.bindValue(":name", PkVariant(name));
    return q.exec();
}

} // namespace

int main()
{
    PkSqlDatabase db = PkSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    if (!db.open()) {
        std::printf("FAIL: open() failed\n");
        return 1;
    }
    sqlite3_exec(db.PkHandle(), "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)", nullptr,
                 nullptr, nullptr);

    int failures = 0;

    // [1] 显式 commit() —— 数据保留。
    {
        KisDatabaseTransactionLock lock(db);
        if (!insertOneRow("committed")) {
            std::printf("FAIL: [1] insert failed inside commit-path transaction\n");
            return 1;
        }
        lock.commit();
    }
    {
        const int c = countWhere("committed");
        std::printf("[1] after explicit commit(): count(name='committed')=%d (expect 1)\n", c);
        if (c != 1) {
            ++failures;
        }
    }

    // [2] 不调用 commit()，离开作用域 —— 自动回滚。
    {
        KisDatabaseTransactionLock lock(db);
        if (!insertOneRow("rolledback")) {
            std::printf("FAIL: [2] insert failed inside rollback-path transaction\n");
            return 1;
        }
        // 故意不调用 lock.commit()；lock 在这里离开作用域析构。
    }
    {
        const int c = countWhere("rolledback");
        std::printf(
            "[2] after leaving scope without commit(): count(name='rolledback')=%d (expect 0)\n",
            c);
        if (c != 0) {
            ++failures;
        }
    }

    // [3] 显式 rollback()（KisDatabaseTransactionLock::rollback() 转发到
    //     unlock()），效果同 [2]，且随后析构不应重复调用 unlock()。
    {
        KisDatabaseTransactionLock lock(db);
        if (!insertOneRow("explicit_rollback")) {
            std::printf("FAIL: [3] insert failed inside explicit-rollback transaction\n");
            return 1;
        }
        lock.rollback();
    }
    {
        const int c = countWhere("explicit_rollback");
        std::printf("[3] after explicit rollback(): count(name='explicit_rollback')=%d (expect 0)\n",
                    c);
        if (c != 0) {
            ++failures;
        }
    }

    // [4] 两次独立事务不冲突（每次都 commit 干净收尾，验证 PkSqlDatabase 的
    //     "已有进行中事务" 防御没有被状态残留误伤——§0 P3 的嵌套事务防御是
    //     PkSqlDatabase 自己的责任，这里验证 KisDatabaseTransactionLockAdapter
    //     的 m_transactionStarted 与它配合正常，不会跨实例互相干扰）。
    {
        KisDatabaseTransactionLock lockA(db);
        insertOneRow("sequential_a");
        lockA.commit();
    }
    {
        KisDatabaseTransactionLock lockB(db);
        insertOneRow("sequential_b");
        lockB.commit();
    }
    {
        const int a = countWhere("sequential_a");
        const int b = countWhere("sequential_b");
        std::printf("[4] sequential transactions: count(a)=%d count(b)=%d (expect 1,1)\n", a, b);
        if (a != 1 || b != 1) {
            ++failures;
        }
    }

    if (failures > 0) {
        std::printf("FAIL: %d assertion(s) failed\n", failures);
        return 1;
    }
    std::printf("PASS: all KisDatabaseTransactionLock graft assertions held\n");
    return 0;
}
