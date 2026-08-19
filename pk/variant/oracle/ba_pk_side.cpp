// ba_pk_side.cpp —— Pk 侧 TU。只包含 Pk 头，不包含任何 Qt 头。
#include "PkAuxTypes.h"
#include "ba_pk_side.h"

// ── 构造 / 析构 ──────────────────────────────────────────────────────────

void* pkb_new() { return new PkByteArray(); }
void* pkb_from_data(const char* data, int len) { return new PkByteArray(data, len); }
void pkb_delete(void* v) { delete static_cast<PkByteArray*>(v); }

// ── 查询 / 变更 ──────────────────────────────────────────────────────────

int pkb_size(void* v) { return static_cast<PkByteArray*>(v)->size(); }
int pkb_isEmpty(void* v) { return static_cast<PkByteArray*>(v)->isEmpty() ? 1 : 0; }
void pkb_resize(void* v, int n) { static_cast<PkByteArray*>(v)->resize(n); }
void pkb_set_byte(void* v, int idx, char c) { static_cast<PkByteArray*>(v)->data()[idx] = c; }
const char* pkb_constData(void* v) { return static_cast<PkByteArray*>(v)->constData(); }
int pkb_equals(void* a, void* b)
{
    return (*static_cast<PkByteArray*>(a) == *static_cast<PkByteArray*>(b)) ? 1 : 0;
}
void* pkb_number(int n, int base) { return new PkByteArray(PkByteArray::number(n, base)); }
void* pkb_number_uint(unsigned int n, int base)
{
    return new PkByteArray(PkByteArray::number(n, base));
}
