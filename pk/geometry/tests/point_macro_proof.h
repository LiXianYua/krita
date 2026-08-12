#pragma once

// 「共存路径下 PkPointF::operator== 的语义没有被宏改写」这条断言的探针。
//
// 为什么要单独一个翻译单元：要证明的事情是**预处理期**的，只能在
// `#include "../PkPoint.h"` 之前把 qFuzzyCompare / qFuzzyIsNull 定义成宏才测得到，
// 而 test_point.cpp 里 PkPoint.h 早就进来了。实现见 point_macro_proof.cpp。
struct PkPointMacroProof {
    bool sabotagedFuzzyWasVisible;  // 宏真的在本 TU 里生效了（否则整个探针是空转）
    bool nearIsEqual;               // (1,1) == (1+1e-13,1)      真 Qt: true
    bool farIsNotEqual;             // (1,1) == (1+1e-11,1)      真 Qt: false
    bool zeroSideIsEqual;           // (0,0) == (1e-300,0)       真 Qt: true
    bool infVsInfIsNotEqual;        // (inf,0) == (inf,0)        真 Qt: false
    bool infVsNegInfIsEqual;        // (inf,0) == (-inf,0)       真 Qt: true
};

PkPointMacroProof pkPointMacroProbe();
