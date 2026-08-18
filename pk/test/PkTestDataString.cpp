#include "PkTestData.h"
#include "../string/PkString.h"

PkTestDataRow &operator<<(PkTestDataRow &row, const char *value)
{
    // Qt qtestdata.h:81-86 同款：字符串字面量自动转 QString（这里是 PkString）再存。
    // 重载决议：非模板优先于模板，const char* 参数（含数组→指针 decay）统一走
    // 这条重载，不再走上面的模板推导 const char[N]。
    PkString str(value);
    row.appendValue(std::any(std::move(str)));
    return row;
}