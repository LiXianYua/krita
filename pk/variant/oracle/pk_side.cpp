#include "PkVariant.h"
#include "pk_side.h"

#include <cstring>
#include <string>
#include <vector>

// Thread-local storage for toString results
static thread_local std::string tl_string;

// ── 构造 / 析构 ──────────────────────────────────────────────────────────

void* pkvar_new() { return new PkVariant(); }
void pkvar_delete(void* v) { delete static_cast<PkVariant*>(v); }

void* pkvar_from_bool(int b) { return new PkVariant(b != 0); }
void* pkvar_from_int(int i) { return new PkVariant(i); }
void* pkvar_from_uint(unsigned int ui) { return new PkVariant(ui); }
void* pkvar_from_ll(long long ll) { return new PkVariant(ll); }
void* pkvar_from_ull(unsigned long long ull) { return new PkVariant(ull); }
void* pkvar_from_double(double d) { return new PkVariant(d); }
void* pkvar_from_float(float f) { return new PkVariant(f); }
void* pkvar_from_string(const char* s) { return new PkVariant(s); }
void* pkvar_copy(void* v) { return new PkVariant(*static_cast<PkVariant*>(v)); }

// ── 查询 ──────────────────────────────────────────────────────────────────

int pkvar_isNull(void* v) { return static_cast<PkVariant*>(v)->isNull() ? 1 : 0; }
int pkvar_isValid(void* v) { return static_cast<PkVariant*>(v)->isValid() ? 1 : 0; }
int pkvar_type(void* v) { return static_cast<int>(static_cast<PkVariant*>(v)->type()); }
int pkvar_userType(void* v) { return static_cast<PkVariant*>(v)->userType(); }
const char* pkvar_typeName(void* v) { return static_cast<PkVariant*>(v)->typeName(); }

// ── 转换 ──────────────────────────────────────────────────────────────────

int pkvar_toBool(void* v) { return static_cast<PkVariant*>(v)->toBool() ? 1 : 0; }
int pkvar_toInt(void* v) { return static_cast<PkVariant*>(v)->toInt(); }
unsigned int pkvar_toUInt(void* v) { return static_cast<PkVariant*>(v)->toUInt(); }
long long pkvar_toLongLong(void* v) { return static_cast<PkVariant*>(v)->toLongLong(); }
unsigned long long pkvar_toULongLong(void* v) { return static_cast<PkVariant*>(v)->toULongLong(); }
double pkvar_toDouble(void* v) { return static_cast<PkVariant*>(v)->toDouble(); }
float pkvar_toFloat(void* v) { return static_cast<PkVariant*>(v)->toFloat(); }
double pkvar_toReal(void* v) { return static_cast<PkVariant*>(v)->toReal(); }

const char* pkvar_toString(void* v)
{
    PkString s = static_cast<PkVariant*>(v)->toString();
    tl_string = s.PkToUtf8();
    return tl_string.c_str();
}

// ── 集合 ──────────────────────────────────────────────────────────────────

int pkvar_toListSize(void* v)
{
    PkVariantList lst = static_cast<PkVariant*>(v)->toList();
    return static_cast<int>(lst.size());
}

int pkvar_toHashSize(void* v)
{
    PkVariantHash h = static_cast<PkVariant*>(v)->toHash();
    return static_cast<int>(h.size());
}

int pkvar_toMapSize(void* v)
{
    PkVariantMap m = static_cast<PkVariant*>(v)->toMap();
    return static_cast<int>(m.size());
}

// ── data/constData ────────────────────────────────────────────────────────

void* pkvar_data(void* v) { return static_cast<PkVariant*>(v)->data(); }
const void* pkvar_constData(void* v) { return static_cast<PkVariant*>(v)->constData(); }

// ── canConvert ────────────────────────────────────────────────────────────

int pkvar_canConvertInt(void* v) { return static_cast<PkVariant*>(v)->canConvert<int>() ? 1 : 0; }
int pkvar_canConvertDouble(void* v) { return static_cast<PkVariant*>(v)->canConvert<double>() ? 1 : 0; }
int pkvar_canConvertBool(void* v) { return static_cast<PkVariant*>(v)->canConvert<bool>() ? 1 : 0; }
int pkvar_canConvertString(void* v) { return static_cast<PkVariant*>(v)->canConvert<PkString>() ? 1 : 0; }
int pkvar_canConvertFloat(void* v) { return static_cast<PkVariant*>(v)->canConvert<float>() ? 1 : 0; }

// ── setValue/clear ────────────────────────────────────────────────────────

void pkvar_setValueInt(void* v, int i) { static_cast<PkVariant*>(v)->setValue(i); }
void pkvar_setValueString(void* v, const char* s) { static_cast<PkVariant*>(v)->setValue(PkString(s)); }
void pkvar_clear(void* v) { static_cast<PkVariant*>(v)->clear(); }

// ── 比较 ──────────────────────────────────────────────────────────────────

int pkvar_equals(void* a, void* b)
{
    return (*static_cast<PkVariant*>(a) == *static_cast<PkVariant*>(b)) ? 1 : 0;
}