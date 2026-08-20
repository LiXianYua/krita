#pragma once

#include "../container/PkStringList.h"
#include "../string/PkString.h"
#include "PkSqlError.h"

struct sqlite3; // 见 PkSqlError.h 顶部同一条理由：公开头文件不 include <sqlite3.h>

// PkSqlDatabase —— QSqlDatabase 的零 Qt 对应物（R-17 plan §1
// 「QSqlDatabase → PkSqlDatabase」）。
//
// **单一全局默认连接的门面**：R-17 plan Architecture 一节实测——16 个生产
// 文件里没有一处传显式 connection name。本类不做 Qt 那种"多个具名连接各自
// 独立"的设计，全部方法操作同一个进程级单例连接状态（PkSqlDatabase 对象
// 本身只是这个单例的一个轻量句柄，拷贝开销是一个 bool）。
//
// `database(connectionName, open)` 的 connectionName 形参**保留只是为了让
// 真实调用点 `QSqlDatabase::database(QSqlDatabase::defaultConnection, false)`
// 这种写法在 Task 3 试接时零改动可编译**——单连接模型下它不参与任何判断，
// 只有 `open` 真正起作用（R-17 plan §1 表原话："connName 参数忽略，只保留
// open 参数"）。
class PkSqlDatabase
{
public:
    // 默认构造 = "无效连接对象"：isValid()==false。对应 Qt 语义——默认构造的
    // QSqlDatabase 没有绑定任何驱动。
    PkSqlDatabase();

    // Qt `QSqlDatabase::defaultConnection` 的对应物（真实调用点里的
    // `QSqlDatabase::database(QSqlDatabase::defaultConnection, false)` 这种
    // 写法要能在零改动编译，就需要这个符号存在）。Qt 里的值是字面量
    // "qt_sql_default_connection"，本类沿用同一字面量（单连接模型下这个值
    // 本身不参与任何比较逻辑，只是为了让调用点的表达式合法）。
    static const char *defaultConnection;

    // driverType/connectionName 两个参数都忽略——固定假设 SQLite（唯一驱动名
    // "QSQLITE"），单连接模型下没有第二个连接可选。
    static PkSqlDatabase addDatabase(const PkString &driverType,
                                      const PkString &connectionName = PkString());

    // connectionName 忽略，只有 open 起作用（见类注释）。connection 不存在
    // （从未调用过 addDatabase）时返回的对象 isValid()==false；存在时
    // isValid()==true，且当 open==true 且当前未打开时会尝试自动 open()
    // （对应 KisResourceCacheDb::createDatabase() 用 open=false 避免这个
    // 自动 open 副作用、只做存在性探测的真实调用形态）。
    static PkSqlDatabase database(const PkString &connectionName = PkString(), bool open = true);

    static PkStringList connectionNames();

    void setDatabaseName(const PkString &name);
    bool open();
    void close();
    bool PkClose();
    bool isOpen() const;
    bool isValid() const;

    // 只列用户表，排除 sqlite 内部表（sqlite_master 等，name 前缀
    // "sqlite_"）——与 Qt SQLite 驱动 `QSqlDatabase::tables()` 默认参数
    // `QSql::Tables` 的实际口径一致。
    PkStringList tables() const;

    // 直接包 "BEGIN"/"COMMIT"/"ROLLBACK"（R-17 plan §2 Task 1 原话）。
    // transaction() 对嵌套调用（已有一个进行中的事务时再调一次）做防御，
    // 直接失败并把 lastError() 设成 §0 P3 [13] 那个反直觉结果（Qt 驱动自己
    // 拦的，不是真的调用 sqlite3——见 PkSqlErrorFactory::nestedTransactionError()）。
    bool transaction();
    bool commit();
    bool rollback();

    PkSqlError lastError() const;

    // ── 互操作（Pk 前缀 → 不计入清单外 API，同 PkString.h 的先例）──────────
    // Task 2 的 PkSqlQuery 需要在同一个连接上 prepare/step，必须能拿到底层
    // sqlite3*——这不是 QSqlDatabase 的 Qt 兼容表面，是 pk/sql 内部各类型
    // 之间的管道。本任务（Task 1）自己不消费它，只是提前把接口开好，
    // 避免 Task 2 再回来改 PkSqlDatabase 的公开签名。
    sqlite3 *PkHandle() const;

private:
    explicit PkSqlDatabase(bool valid);
    bool m_valid = false;
};
