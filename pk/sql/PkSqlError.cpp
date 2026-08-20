#include "PkSqlError.h"

#include <sqlite3.h>

#include <cstring>
#include <string>

namespace {

// sqlite3_errmsg() 可能返回 nullptr（例如 db 句柄本身是 nullptr）——
// utf8ToPk 对此返回空 PkString，不是崩溃。
PkString utf8ToPk(const char *s)
{
    if (!s) {
        return PkString();
    }
    return PkString::PkFromUtf8(s, static_cast<int>(std::strlen(s)));
}

// nativeErrorCode() = sqlite3 原生 result code 的十进制字符串（§0 P1 结论
// 第二条：1=SQLITE_ERROR、19=SQLITE_CONSTRAINT）。用 std::to_string 而不是
// PkString::arg()，避免依赖 arg(int) 的占位符替换语义间接影响这里的转换。
PkString rcToPk(int rc)
{
    const std::string s = std::to_string(rc);
    return PkString::PkFromUtf8(s.c_str(), static_cast<int>(s.size()));
}

} // namespace

PkSqlError::PkSqlError() : m_type(NoError)
{
}

PkSqlError::PkSqlError(const PkString &databaseText, const PkString &driverText, ErrorType type,
                        const PkString &nativeErrorCode)
    : m_databaseText(databaseText), m_driverText(driverText), m_type(type),
      m_nativeErrorCode(nativeErrorCode)
{
}

PkSqlError::ErrorType PkSqlError::type() const
{
    return m_type;
}

bool PkSqlError::isValid() const
{
    return m_type != NoError;
}

PkString PkSqlError::text() const
{
    // §0 P1 结论第一条：text() = databaseText() + （driverText() 非空则加一个
    // 空格再拼上）——不是分号/冒号拼接。driverText 为空时就是纯 databaseText
    // （NoError 默认构造两者皆空，text() 也是空串，与"无错误"语义一致）。
    if (m_driverText.isEmpty()) {
        return m_databaseText;
    }
    PkString result = m_databaseText;
    result.append(PkString(" "));
    result.append(m_driverText);
    return result;
}

PkString PkSqlError::databaseText() const
{
    return m_databaseText;
}

PkString PkSqlError::driverText() const
{
    return m_driverText;
}

PkString PkSqlError::nativeErrorCode() const
{
    return m_nativeErrorCode;
}

namespace PkSqlErrorFactory {

PkSqlError fromPrepareFailure(sqlite3 *db, int rc)
{
    return PkSqlError(utf8ToPk(sqlite3_errmsg(db)), PkString("Unable to execute statement"),
                       PkSqlError::StatementError, rcToPk(rc));
}

PkSqlError fromStepFailure(sqlite3 *db, int rc)
{
    // §0 P1 [4][5][6]：UNIQUE / PRIMARY KEY / NOT NULL 三种约束冲突全部实测
    // 为 ConnectionError + "Unable to fetch row"。非 SQLITE_CONSTRAINT 的
    // step 失败没有独立探针，按同一分支处理（见头文件注释里的说明）。
    return PkSqlError(utf8ToPk(sqlite3_errmsg(db)), PkString("Unable to fetch row"),
                       PkSqlError::ConnectionError, rcToPk(rc));
}

PkSqlError fromTransactionFailure(sqlite3 *db, int rc, const char *verb)
{
    PkString driverText("Unable to ");
    driverText.append(PkString(verb));
    driverText.append(PkString(" transaction"));
    return PkSqlError(utf8ToPk(sqlite3_errmsg(db)), driverText, PkSqlError::TransactionError,
                       rcToPk(rc));
}

PkSqlError nestedTransactionError()
{
    // §0 P3 [13] 原始输出逐字复刻：databaseText/driverText 都是字面量，
    // nativeErrorCode 是空 PkString——没有真的调用 sqlite3，没有原生错误码。
    return PkSqlError(PkString("cannot start a transaction within a transaction"),
                       PkString("Unable to begin transaction"), PkSqlError::TransactionError,
                       PkString());
}

PkSqlError fromOpenFailure(sqlite3 *db, int rc)
{
    return PkSqlError(utf8ToPk(db ? sqlite3_errmsg(db) : "out of memory"),
                       PkString("Error opening database"), PkSqlError::ConnectionError,
                       rcToPk(rc));
}

PkSqlError fromCloseFailure(sqlite3 *db, int rc)
{
    return PkSqlError(utf8ToPk(db ? sqlite3_errmsg(db) : nullptr),
                       PkString("Error closing database"), PkSqlError::ConnectionError,
                       rcToPk(rc));
}

} // namespace PkSqlErrorFactory
