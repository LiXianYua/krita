#ifndef PK_ORACLE_TRACE_OPS_H
#define PK_ORACLE_TRACE_OPS_H
// 操作码定义 + 脚本 tag 生成器。两侧共用同一份，**不含任何指针类型**——
// 这样 pointer_difftest.cpp 里两侧真的各自 include 自己的类型，这份头不参与
// 「谁被谁替换」的风险。逐字来自 R-04 Task 2 简报 Step 1。
#include <string>
#include <vector>

enum SharedOp {
    OpMakeNew, OpMakeNullRaw, OpMakeNullDeleter, OpMakeCreate, OpMakeDefault,
    OpMakeDerived,                 // 造 Derived，用来压派生→基类与 dynamicCast
    OpCopy, OpAssign, OpAssignNullptr, OpSelfAssign,
    OpReset, OpResetNew, OpResetDeleter,
    OpClear, OpDynamicCastToDerived, OpDynamicCastToUnrelated, OpStaticCastToDerived,
    OpWeakFrom, OpStrongFromWeak, OpWeakAssignNullptr,
    OpSharedOpCount
};
inline const char *sharedOpName(int op)
{
    static const char *n[] = {
        "makeNew", "makeNullRaw", "makeNullDeleter", "makeCreate", "makeDefault",
        "makeDerived", "copy", "assign", "assignNullptr", "selfAssign",
        "reset", "resetNew", "resetDeleter", "clear",
        "dynCastDerived", "dynCastUnrelated", "staticCastDerived",
        "weakFrom", "strongFromWeak", "weakAssignNullptr"
    };
    return n[op];
}

enum ScopedOp {
    SOpMakeNew, SOpMakeDefault, SOpReset, SOpResetNew, SOpResetSame,
    SOpTake, SOpTakeThenReset, SOpDeref, SOpArrayMake, SOpArrayReset, SOpArrayIndex,
    SOpScopedOpCount
};
inline const char *scopedOpName(int op)
{
    static const char *n[] = {
        "makeNew", "makeDefault", "reset", "resetNew", "resetSame",
        "take", "takeThenReset", "deref", "arrayMake", "arrayReset", "arrayIndex"
    };
    return n[op];
}

struct Step { int op; int a; int b; };          // op 与两个槽号
typedef std::vector<Step> Script;

// tag 由**触发差异的输入形态**参与构造（R线-spec 规则一）：
// 用整条脚本的操作码名 + 槽号拼出来，而不是每个 API 一个字面量常量。
inline std::string scriptTag(const Script &s, const char *(*name)(int))
{
    std::string t;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i) t += '>';
        t += name(s[i].op);
        t += '('; t += char('0' + s[i].a); t += ','; t += char('0' + s[i].b); t += ')';
    }
    return t;
}
#endif
