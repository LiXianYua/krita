#pragma once

#include "../string/PkString.h"

// sqlite3.h 只在 .cpp 里 include；公开头文件只前置声明，避免把
// <sqlite3.h> 拖进 pksql 的下游使用者（PkSqlDatabase.h/PkSqlQuery.h 同理）。
struct sqlite3;

// PkSqlError —— QSqlError 的零 Qt 对应物（R-17 plan §1「QSqlError → PkSqlError」）。
//
// ErrorType 的取值与 text() 的拼接规则**都是 R-17 plan §0 P1 探针实测钉死的
// Qt SQLite 驱动内部行为**，不是 SQL 语义本身——**这条分类不能凭直觉猜**：
//   - SQLITE_ERROR（prepare 阶段失败：语法错误 / 表不存在）→ StatementError
//   - SQLITE_CONSTRAINT（step 阶段失败：UNIQUE/PK/NOT NULL 约束冲突）
//     → **ConnectionError**（反直觉但探针实测如此，Qt 驱动把"step 拿不到行"
//     的失败路径统一归 ConnectionError，不区分是不是约束冲突）
//   - 事务 BEGIN/COMMIT/ROLLBACK 失败 → TransactionError
//   - `text()` = `databaseText()` + （`driverText()` 非空则前面加一个空格拼接），
//     不是分号/冒号拼接
// 枚举数值对齐 Qt 头文件声明顺序（探针确认前三个的实际取值）：
// NoError=0 / ConnectionError=1 / StatementError=2 / TransactionError=3 /
// UnknownError=4。UnknownError 本任务没有产生它的调用路径（sqlite3 的失败
// 都能归到前三类之一），保留数值占位供未来扩展，不代表已实现的分支会用到它。
class PkSqlError
{
public:
    enum ErrorType : int {
        NoError = 0,
        ConnectionError = 1,
        StatementError = 2,
        TransactionError = 3,
        UnknownError = 4,
    };

    // 默认构造 = "无错误"：type()==NoError，isValid()==false。
    // 对应 §0 P2 [8] "耗尽后 next()" 场景：lastError() 保持 NoError，不设错误。
    PkSqlError();

    PkSqlError(const PkString &databaseText, const PkString &driverText, ErrorType type,
               const PkString &nativeErrorCode = PkString());

    ErrorType type() const;
    // isValid() == (type() != NoError)，与探针每一条"isValid()=true"的结果
    // （只要 type() 非 NoError 就是 true）逐条对齐，§0 P1 全部 7 条样例、
    // P3 [13] 均是这个规则。
    bool isValid() const;
    PkString text() const;
    PkString databaseText() const;
    PkString driverText() const;
    PkString nativeErrorCode() const;

private:
    PkString m_databaseText;
    PkString m_driverText;
    ErrorType m_type = NoError;
    PkString m_nativeErrorCode;
};

// ── 内部工厂：从 sqlite3 失败结果构造分类正确的 PkSqlError ──────────────────
//
// 独立 namespace + 全部函数名不带 Pk 前缀但整体挂在 PkSqlErrorFactory 下：
// 这是内部管道（互操作），不是 QSqlError 的 Qt 兼容表面本身——同 PkString.h
// "互操作 Pk 前缀不计入清单外 API" 的先例，只是这里用 namespace 分组而不是
// 单个前缀，因为函数不止一个。PkSqlDatabase 的 transaction()/commit()/
// rollback()（本任务范围）与 Task 2 的 PkSqlQuery 的 prepare()/exec() 失败
// 分类都要用它，分类逻辑只写一份、不抄两遍。
namespace PkSqlErrorFactory {

// prepare 阶段失败（sqlite3_prepare_v2 返回非 SQLITE_OK）→ StatementError。
// §0 P1 [2][3][7]：语法错误、表不存在、prepare 期语法错误都走这条。
PkSqlError fromPrepareFailure(sqlite3 *db, int rc);

// step 阶段失败（sqlite3_step 返回非 SQLITE_ROW/SQLITE_DONE）。
// SQLITE_CONSTRAINT（约束冲突）→ ConnectionError，是 §0 P1 的反直觉结论
// （[4][5][6] UNIQUE/PK/NOT NULL 三种冲突逐一探针实测）。非 SQLITE_CONSTRAINT
// 的 step 失败探针没有单独覆盖过，本实现按同一分支处理（与 Qt 驱动源码里
// step 失败统一走 "Unable to fetch row" 的实现一致），标注为推断，供
// Task 2/S-02-b 用真实语句核对。
PkSqlError fromStepFailure(sqlite3 *db, int rc);

// 事务 BEGIN/COMMIT/ROLLBACK 失败（真的调用了 sqlite3_exec 但失败）
// → TransactionError，driverText 由 verb 决定
// （"begin"/"commit"/"rollback" → "Unable to begin/commit/rollback transaction"，
// §0 P1 结论第三条列出的三个已知取值）。
PkSqlError fromTransactionFailure(sqlite3 *db, int rc, const char *verb);

// §0 P3 [13] 特例：嵌套 transaction()（已有一个进行中的事务时再调一次）是
// Qt 驱动自己用内部布尔标志拦的，**没有真的调用 sqlite3**——
// nativeErrorCode() 必须是空串，不能塞一个假的 sqlite 错误码。
PkSqlError nestedTransactionError();

// open() 失败（sqlite3_open_v2 返回非 SQLITE_OK）→ ConnectionError。
// **§0 P1–P6 没有探针覆盖 QSqlDatabase::open() 失败这条路径**（§1 用量表
// open() 只数了调用次数，没有对应失败场景的探针）——driverText 用
// "Error opening database" 是本任务按 Qt 惯例（ConnectionError 语义上就是
// "连不上/打不开连接"）做的合理推断，不是探针实测值，Task 4 收尾/S-02-b
// 试接时如发现真实 Qt 输出不同，以实测为准订正。
PkSqlError fromOpenFailure(sqlite3 *db, int rc);

PkSqlError fromCloseFailure(sqlite3 *db, int rc);

} // namespace PkSqlErrorFactory
