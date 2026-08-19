// R-18 折叠：本文件曾是 geometry 侧 qIsNaN / qInf 的 out-of-line 定义所在
//（照真 Qt qnumeric.h 的非 inline 形态）。折叠后标量唯一权威在 pk/global，
// 那份 PkGlobal.h 已把它们做成 constexpr inline，geometry 经转发头命中同一份
// —— 这里的重复定义是死代码，删掉定义、只留转发。
//
// 文件本身**保留**：pk/sql/CMakeLists.txt 里手写的 pkgeometry 目标仍引用
// ${PKGEOMETRY_DIR}/PkGlobal.cpp（那份在 R-18 锁外不能动），保留同名文件让
// 所有引用都成立。编译产物是空 .o，不含任何符号。
#include "PkGlobal.h"
