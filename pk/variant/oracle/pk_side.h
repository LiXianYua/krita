#pragma once

// pk_side.h —— Opaque C bridge to PkVariant for oracle.cpp
// This header avoids including PkVariant.h, so oracle.cpp's TU never sees
// PkGlobal.h's qAbs/qRound etc. that would collide with Qt's versions.

#ifdef __cplusplus
extern "C" {
#endif

void* pkvar_new();
void pkvar_delete(void* v);

void* pkvar_from_int(int i);
void* pkvar_from_uint(unsigned int ui);
void* pkvar_from_ll(long long ll);
void* pkvar_from_ull(unsigned long long ull);
void* pkvar_from_double(double d);
void* pkvar_from_float(float f);
void* pkvar_from_bool(int b);
void* pkvar_from_string(const char* s);
void* pkvar_copy(void* v);

int pkvar_isNull(void* v);
int pkvar_isValid(void* v);
int pkvar_type(void* v);
int pkvar_userType(void* v);
const char* pkvar_typeName(void* v);

int pkvar_toBool(void* v);
int pkvar_toInt(void* v);
unsigned int pkvar_toUInt(void* v);
long long pkvar_toLongLong(void* v);
unsigned long long pkvar_toULongLong(void* v);
double pkvar_toDouble(void* v);
float pkvar_toFloat(void* v);
double pkvar_toReal(void* v);
const char* pkvar_toString(void* v);

int pkvar_canConvertInt(void* v);
int pkvar_canConvertDouble(void* v);
int pkvar_canConvertBool(void* v);
int pkvar_canConvertString(void* v);
int pkvar_canConvertFloat(void* v);

void pkvar_setValueInt(void* v, int i);
void pkvar_setValueString(void* v, const char* s);
void pkvar_clear(void* v);

void* pkvar_data(void* v);
const void* pkvar_constData(void* v);

int pkvar_equals(void* a, void* b);

#ifdef __cplusplus
}
#endif