#include "PkSqlQuery.h"

#include "PkSqlDatabase.h"

#include <sqlite3.h>

#include <cstring>
#include <string>

namespace {

PkString utf8ToPk(const char *s, int nbytes)
{
    if (!s || nbytes <= 0) {
        return PkString();
    }
    return PkString::PkFromUtf8(s, nbytes);
}

// 读一列的值，按 sqlite3 report 的运行期类型分派（`sqlite3_column_type`）——
// 与 sqlite3 的动态类型系统一致，不看 CREATE TABLE 声明的静态列类型。
PkVariant columnValue(sqlite3_stmt *stmt, int col)
{
    switch (sqlite3_column_type(stmt, col)) {
        case SQLITE_INTEGER:
            return PkVariant(static_cast<long long>(sqlite3_column_int64(stmt, col)));
        case SQLITE_FLOAT:
            return PkVariant(sqlite3_column_double(stmt, col));
        case SQLITE_TEXT: {
            const unsigned char *txt = sqlite3_column_text(stmt, col);
            const int nbytes = sqlite3_column_bytes(stmt, col);
            return PkVariant(utf8ToPk(reinterpret_cast<const char *>(txt), nbytes));
        }
        case SQLITE_BLOB: {
            const void *blob = sqlite3_column_blob(stmt, col);
            const int nbytes = sqlite3_column_bytes(stmt, col);
            return PkVariant(PkByteArray(reinterpret_cast<const char *>(blob), nbytes));
        }
        case SQLITE_NULL:
        default:
            return PkVariant(); // Invalid/null——与 §0 P2 的"未定位取值"共用同一份
                                 // "空 PkVariant"表示，调用方用 isNull()/isValid() 都能分辨。
    }
}

// 把一个 PkVariant 绑到 sqlite3_stmt 的第 idx 个参数位（1-indexed，sqlite3 的
// 编号从 1 开始）。idx<=0 代表"这个占位符名字/位置在语句里根本不存在"
// （`sqlite3_bind_parameter_index` 找不到时返回 0）——静默跳过，不报错：
// 真实调用点存在"绑了一个语句里没用到的名字"这种无害场景，不是本类要拦的错误。
bool bindVariantAtIndex(sqlite3_stmt *stmt, int idx, const PkVariant &value)
{
    if (idx <= 0) {
        return true;
    }
    if (!value.isValid() || value.isNull()) {
        return sqlite3_bind_null(stmt, idx) == SQLITE_OK;
    }
    switch (value.type()) {
        case PkVariant::Bool:
        case PkVariant::Int:
        case PkVariant::UInt:
            return sqlite3_bind_int(stmt, idx, value.toInt()) == SQLITE_OK;
        case PkVariant::LongLong:
        case PkVariant::ULongLong:
            return sqlite3_bind_int64(stmt, idx, static_cast<sqlite3_int64>(value.toLongLong()))
                == SQLITE_OK;
        case PkVariant::Double:
        case PkVariant::Float:
            return sqlite3_bind_double(stmt, idx, value.toDouble()) == SQLITE_OK;
        case PkVariant::ByteArray: {
            const PkByteArray ba = value.toByteArray();
            return sqlite3_bind_blob(stmt, idx, ba.data(), ba.size(), SQLITE_TRANSIENT) == SQLITE_OK;
        }
        default: {
            // 其余全部类型（String 及未特殊处理的类型）走 toString()——与 Qt
            // 驱动对"不认识的 QVariant 类型"落到字符串表示的兜底策略一致，
            // 真实调用点 171 处具名绑定绝大多数本来就是字符串/整数。
            const std::string utf8 = value.toString().PkToUtf8();
            return sqlite3_bind_text(stmt, idx, utf8.c_str(), static_cast<int>(utf8.size()),
                                      SQLITE_TRANSIENT)
                == SQLITE_OK;
        }
    }
}

} // namespace

PkSqlQuery::PkSqlQuery()
    : m_db(nullptr), m_stmt(nullptr), m_isSelect(false), m_forwardOnly(false),
      m_numRowsAffected(-1)
{
}

PkSqlQuery::~PkSqlQuery()
{
    releaseStatement();
}

void PkSqlQuery::releaseStatement()
{
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
        m_stmt = nullptr;
    }
}

bool PkSqlQuery::prepare(const PkString &sql)
{
    releaseStatement();
    m_namedBinds.clear();
    m_positionalBinds.clear();
    m_cursor.clear();
    m_isSelect = false;
    m_numRowsAffected = -1;
    m_lastInsertId = PkVariant();
    m_sql = sql;

    // 单一全局连接：见 PkSqlDatabase 类注释，open=false 只是探测"连接是否
    // 已存在"，不触发自动 open()——prepare() 需要的是已经 open 过的 handle，
    // 没 open 时 PkHandle() 返回 nullptr，sqlite3_prepare_v2 对 nullptr db
    // 的行为交给 sqlite3 自己处理（会失败，fromPrepareFailure 用 nullptr db
    // 取 errmsg 时同样有 null 防御，见 PkSqlError.cpp 的 utf8ToPk）。
    m_db = PkSqlDatabase::database(PkString(), false).PkHandle();

    const std::string utf8 = sql.PkToUtf8();
    const int rc = sqlite3_prepare_v2(m_db, utf8.c_str(), -1, &m_stmt, nullptr);
    if (rc != SQLITE_OK) {
        m_lastError = PkSqlErrorFactory::fromPrepareFailure(m_db, rc);
        m_stmt = nullptr;
        return false;
    }

    const int colCount = sqlite3_column_count(m_stmt);
    m_isSelect = colCount > 0;
    std::vector<PkString> colNames;
    colNames.reserve(static_cast<std::size_t>(colCount));
    for (int i = 0; i < colCount; ++i) {
        const char *cn = sqlite3_column_name(m_stmt, i);
        colNames.push_back(cn ? utf8ToPk(cn, static_cast<int>(std::strlen(cn))) : PkString());
    }
    m_cursor.setColumnNames(colNames);

    m_lastError = PkSqlError();
    return true;
}

void PkSqlQuery::bindValue(const PkString &name, const PkVariant &value)
{
    m_namedBinds[name] = value;
}

void PkSqlQuery::addBindValue(const PkVariant &value)
{
    m_positionalBinds.push_back(value);
}

bool PkSqlQuery::execInternal()
{
    if (!m_stmt) {
        return false;
    }

    // 支持"prepare 一次、循环 bindValue+exec"复用模式（`KisTagResourceModel::
    // untagResources` 的形态）：每次 exec() 自己 reset + 清绑定 + 按当前
    // m_namedBinds/m_positionalBinds 重新绑一遍，不依赖"sqlite3_reset 不清
    // 绑定"这条底层细节继续隐式生效——显式做，行为不随 sqlite3 版本/理解
    // 漂移。
    sqlite3_reset(m_stmt);
    sqlite3_clear_bindings(m_stmt);

    for (const auto &kv : m_namedBinds) {
        const std::string nameUtf8 = kv.first.PkToUtf8();
        const int idx = sqlite3_bind_parameter_index(m_stmt, nameUtf8.c_str());
        bindVariantAtIndex(m_stmt, idx, kv.second);
    }
    for (std::size_t i = 0; i < m_positionalBinds.size(); ++i) {
        bindVariantAtIndex(m_stmt, static_cast<int>(i) + 1, m_positionalBinds[i]);
    }

    // 只清行缓冲，保留 prepare() 时已经记好的列名（同一个 prepared statement
    // 重复执行，列结构不变）。
    m_cursor.clearRows();

    const int colCount = sqlite3_column_count(m_stmt);
    int rc = sqlite3_step(m_stmt);
    while (rc == SQLITE_ROW) {
        if (colCount > 0) {
            PkVariantList row;
            row.reserve(static_cast<std::size_t>(colCount));
            for (int i = 0; i < colCount; ++i) {
                row.push_back(columnValue(m_stmt, i));
            }
            m_cursor.appendRow(row);
        }
        rc = sqlite3_step(m_stmt);
    }

    const bool ok = (rc == SQLITE_DONE);
    if (!ok) {
        m_lastError = PkSqlErrorFactory::fromStepFailure(m_db, rc);
        m_numRowsAffected = -1;
    } else {
        m_lastError = PkSqlError();
        m_numRowsAffected = sqlite3_changes(m_db);
        m_lastInsertId = PkVariant(static_cast<long long>(sqlite3_last_insert_rowid(m_db)));
    }
    return ok;
}

bool PkSqlQuery::exec()
{
    return execInternal();
}

bool PkSqlQuery::exec(const PkString &sql)
{
    if (!prepare(sql)) {
        return false;
    }
    return execInternal();
}

bool PkSqlQuery::next()
{
    return m_cursor.next();
}

bool PkSqlQuery::first()
{
    return m_cursor.first();
}

bool PkSqlQuery::seek(int index)
{
    return m_cursor.seek(index);
}

int PkSqlQuery::at() const
{
    return m_cursor.at();
}

void PkSqlQuery::clear()
{
    releaseStatement();
    m_namedBinds.clear();
    m_positionalBinds.clear();
    m_cursor.clear();
    m_sql = PkString();
    m_isSelect = false;
    m_numRowsAffected = -1;
    m_lastInsertId = PkVariant();
    m_lastError = PkSqlError();
    m_db = nullptr;
}

bool PkSqlQuery::isValid() const
{
    return m_cursor.isValid();
}

bool PkSqlQuery::isSelect() const
{
    return m_isSelect;
}

void PkSqlQuery::setForwardOnly(bool forward)
{
    m_forwardOnly = forward;
}

int PkSqlQuery::size() const
{
    // §0 P5：这台机器的 Qt SQLite 驱动恒返回 -1，与 forwardOnly 无关；
    // m_forwardOnly 字段因此在这里确实用不上，符合类头注释"允许无操作"。
    return -1;
}

PkVariant PkSqlQuery::lastInsertId() const
{
    return m_lastInsertId;
}

int PkSqlQuery::numRowsAffected() const
{
    return m_numRowsAffected;
}

PkString PkSqlQuery::lastQuery() const
{
    return m_sql;
}

PkString PkSqlQuery::executedQuery() const
{
    // 与 lastQuery() 共用同一份存储——R-17 plan §1 用量表原话允许的做法。
    return m_sql;
}

PkSqlError PkSqlQuery::lastError() const
{
    return m_lastError;
}

PkVariant PkSqlQuery::value(int col) const
{
    return m_cursor.value(col);
}

PkVariant PkSqlQuery::value(const PkString &name) const
{
    const int idx = m_cursor.columnIndex(name);
    if (idx < 0) {
        return PkVariant();
    }
    return m_cursor.value(idx);
}
