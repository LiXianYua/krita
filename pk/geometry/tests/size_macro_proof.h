#pragma once

// 「共存路径下 PkSizeF::operator== 的语义没有被宏改写」这条断言的探针。
//
// 与 point_macro_proof.h 同形，但**期望值不同**：QSizeF 的 == 是对两个分量各做
// 一次 qFuzzyCompare（没有 QPointF 那个"任一侧为 0 就走 fuzzyIsNull"的分支），
// 所以零侧的期望是**不相等**。实现见 size_macro_proof.cpp。
struct PkSizeMacroProof {
    bool sabotagedFuzzyWasVisible;  // 宏真的在本 TU 里生效了（否则整个探针是空转）
    bool nearIsEqual;               // (1,1) == (1+1e-13,1)     真 Qt: true
    bool farIsNotEqual;             // (1,1) == (1+1e-11,1)     真 Qt: false
    bool zeroSideIsNotEqual;        // (0,0) == (1e-300,0)      真 Qt: **false**（与 QPointF 相反）
    bool bothZeroIsEqual;           // (0,0) == (0,0)           真 Qt: true
    bool infVsInfIsNotEqual;        // (inf,1) == (inf,1)       真 Qt: false
    bool infVsNegInfIsEqual;        // (inf,1) == (-inf,1)      真 Qt: true
};

PkSizeMacroProof pkSizeMacroProbe();
