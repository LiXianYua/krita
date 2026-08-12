#pragma once

// 「共存路径下 PkRectF::operator== 的语义没有被宏改写」这条断言的探针。
//
// 与 point_macro_proof.h / size_macro_proof.h 同形，期望值取自真 Qt 5.15.7 实测
//（真 Qt 5.15.7 实测）。QRectF 的 == 是对**四个分量**各做一次 qFuzzyCompare，
// 与 QSizeF 同形（没有 QPointF 那个"任一侧为 0 就走 fuzzyIsNull"的分支），
// 所以零侧的期望是**不相等**。实现见 rectf_macro_proof.cpp。
struct PkRectFMacroProof {
    bool sabotagedFuzzyWasVisible;  // 宏真的在本 TU 里生效了（否则整个探针是空转）
    bool nearIsEqual;               // (1,1,1,1)==(1+1e-13,1,1,1)  真 Qt: true
    bool farIsNotEqual;             // (1,1,1,1)==(1+1e-11,1,1,1)  真 Qt: false
    bool zeroSideIsNotEqual;        // (0,0,1,1)==(1e-300,0,1,1)   真 Qt: **false**
    bool allZeroIsEqual;            // (0,0,0,0)==(0,0,0,0)        真 Qt: true
    bool infVsInfIsNotEqual;        // (inf,0,1,1)==(inf,0,1,1)    真 Qt: false
    bool infVsNegInfIsEqual;        // (inf,0,1,1)==(-inf,0,1,1)   真 Qt: true
    bool signedZeroIsEqual;         // (0,0,-0.0,1)==(0,0,0.0,1)   真 Qt: true
};

PkRectFMacroProof pkRectFMacroProbe();
