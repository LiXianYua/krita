#pragma once

// ba_pk_side.h —— Opaque C bridge to PkByteArray for ba_oracle.cpp
// ba_oracle.cpp（Qt 侧 TU）只通过本头调用 Pk 侧实现，永不同时看到 PkAuxTypes.h
// 与 <QByteArray>：PkGlobal.h（经 PkPoint.h 进入）的 qAbs/qRound/qMin 等与
// Qt qglobal.h 同名，混在一个 TU 会重定义冲突。

#ifdef __cplusplus
extern "C" {
#endif

void* pkb_new();
void* pkb_from_data(const char* data, int len);
void  pkb_delete(void* v);

int   pkb_size(void* v);
int   pkb_isEmpty(void* v);
void  pkb_resize(void* v, int n);
void  pkb_set_byte(void* v, int idx, char c);
const char* pkb_constData(void* v);
int   pkb_equals(void* a, void* b);
void* pkb_number(int n, int base);
void* pkb_number_uint(unsigned int n, int base);

#ifdef __cplusplus
}
#endif
