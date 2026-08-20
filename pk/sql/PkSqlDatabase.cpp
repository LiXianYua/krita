#include "PkSqlDatabase.h"

#include <sqlite3.h>

#include <cstring>

namespace {

// 进程级单例连接状态——PkSqlDatabase 架构上就是"单一全局默认连接的门面"
// （见头文件类注释），所以这里用一个 Meyer's singleton 存真正的状态，
// PkSqlDatabase 实例本身只是一个 bool（是否绑定到一个"存在"的连接）。
struct PkSqlConnectionState
{
    sqlite3 *handle = nullptr;
    bool created = false; // addDatabase() 是否被调用过（连接"存在"）
    PkString databaseName;
    PkSqlError lastError;
    // Qt SQLite 驱动自己维护的"是否有进行中事务"标志（§0 P3 [13]：嵌套
    // transaction() 是这个标志拦的，不是 sqlite3 报错）。
    bool inTransaction = false;
};

PkSqlConnectionState &state()
{
    static PkSqlConnectionState s;
    return s;
}

} // namespace

const char *PkSqlDatabase::defaultConnection = "qt_sql_default_connection";

PkSqlDatabase::PkSqlDatabase() : m_valid(false)
{
}

PkSqlDatabase::PkSqlDatabase(bool valid) : m_valid(valid)
{
}

PkSqlDatabase PkSqlDatabase::addDatabase(const PkString &driverType, const PkString &connectionName)
{
    // 单连接模型：两个参数都忽略（固定假设 SQLite，见类注释）。
    (void)driverType;
    (void)connectionName;
    state().created = true;
    return PkSqlDatabase(true);
}

PkSqlDatabase PkSqlDatabase::database(const PkString &connectionName, bool open)
{
    (void)connectionName; // 单连接模型：忽略，见类注释
    auto &st = state();
    if (!st.created) {
        return PkSqlDatabase(false);
    }
    if (open && !st.handle) {
        // 复用 open() 的实现：忽略返回值——按 Qt 语义，database() 的 open
        // 参数只是"要不要尝试自动打开"，打开失败不影响这里返回的对象本身
        // 是否 isValid()（isValid() 只反映"连接是否存在"，不是"是否已打开"，
        // 与 isOpen() 是两件事）。
        PkSqlDatabase(true).open();
    }
    return PkSqlDatabase(true);
}

PkStringList PkSqlDatabase::connectionNames()
{
    PkStringList result;
    if (state().created) {
        result.append(PkString(defaultConnection));
    }
    return result;
}

void PkSqlDatabase::setDatabaseName(const PkString &name)
{
    state().databaseName = name;
}

bool PkSqlDatabase::open()
{
    auto &st = state();
    if (st.handle) {
        return true; // 已经打开：re-open 是 no-op 成功
    }
    const std::string path = st.databaseName.PkToUtf8();
    sqlite3 *h = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &h, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                    nullptr);
    if (rc != SQLITE_OK) {
        st.lastError = PkSqlErrorFactory::fromOpenFailure(h, rc);
        if (h) {
            sqlite3_close(h); // sqlite3_open_v2 失败时仍可能返回一个可用于取错误信息的句柄
        }
        return false;
    }
    st.handle = h;
    st.lastError = PkSqlError();
    return true;
}

void PkSqlDatabase::close()
{
    (void)PkClose();
}

bool PkSqlDatabase::PkClose()
{
    auto &st = state();
    if (!st.handle) {
        st.lastError = PkSqlError();
        return true;
    }
    const int rc = sqlite3_close(st.handle);
    if (rc != SQLITE_OK) {
        st.lastError = PkSqlErrorFactory::fromCloseFailure(st.handle, rc);
        return false;
    }
    st.handle = nullptr;
    st.inTransaction = false;
    st.lastError = PkSqlError();
    return true;
}

bool PkSqlDatabase::isOpen() const
{
    return state().handle != nullptr;
}

bool PkSqlDatabase::isValid() const
{
    return m_valid;
}

PkStringList PkSqlDatabase::tables() const
{
    PkStringList result;
    auto &st = state();
    if (!st.handle) {
        return result;
    }
    sqlite3_stmt *stmt = nullptr;
    const char *sql =
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\'";
    if (sqlite3_prepare_v2(st.handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        const char *cname = name ? reinterpret_cast<const char *>(name) : "";
        result.append(PkString::PkFromUtf8(cname, static_cast<int>(std::strlen(cname))));
    }
    sqlite3_finalize(stmt);
    return result;
}

bool PkSqlDatabase::transaction()
{
    auto &st = state();
    if (st.inTransaction) {
        // §0 P3 [13]：Qt 驱动自己拦的，没有真的调用 sqlite3。
        st.lastError = PkSqlErrorFactory::nestedTransactionError();
        return false;
    }
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(st.handle, "BEGIN", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }
    if (rc != SQLITE_OK) {
        st.lastError = PkSqlErrorFactory::fromTransactionFailure(st.handle, rc, "begin");
        return false;
    }
    st.inTransaction = true;
    st.lastError = PkSqlError();
    return true;
}

bool PkSqlDatabase::commit()
{
    auto &st = state();
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(st.handle, "COMMIT", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }
    // 无论成功失败都清掉"进行中事务"标志——照抄
    // KisDatabaseTransactionLockAdapter::commit() 的模式（R-17 plan 附录源码：
    // `if (!m_database.commit()) { qWarning...; } m_transactionStarted = false;`
    // 调用方自己的标志在 commit 尝试后无条件清零）。§0 探针没有覆盖 commit
    // 失败场景，这条是按同一模式做的推断，不是探针实测值。
    st.inTransaction = false;
    if (rc != SQLITE_OK) {
        st.lastError = PkSqlErrorFactory::fromTransactionFailure(st.handle, rc, "commit");
        return false;
    }
    st.lastError = PkSqlError();
    return true;
}

bool PkSqlDatabase::rollback()
{
    auto &st = state();
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(st.handle, "ROLLBACK", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }
    // 同 commit()：无条件清零，理由同上。
    st.inTransaction = false;
    if (rc != SQLITE_OK) {
        st.lastError = PkSqlErrorFactory::fromTransactionFailure(st.handle, rc, "rollback");
        return false;
    }
    st.lastError = PkSqlError();
    return true;
}

PkSqlError PkSqlDatabase::lastError() const
{
    return state().lastError;
}

sqlite3 *PkSqlDatabase::PkHandle() const
{
    return state().handle;
}
