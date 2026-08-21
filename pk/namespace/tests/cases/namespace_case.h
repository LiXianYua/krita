#pragma once

// PkNamespace.h（Qt 命名空间枚举族）的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。这里走 pk/global
// 的等价展开（局部 #define），不走 pk/test/compat —— 本模块没有 compat 垫片，
// -I 编译行不含 pk/test/compat。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"
#include "../../PkNamespace.h"

class PkNamespaceCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void keyboardModifierValues();
    void keyboardModifiersFlags();
    void modifierShortNames();
    void mouseButtonValues();
    void mouseButtonsFlags();
    void orientationValues();
    void focusPolicyValues();
    void sortOrderValues();
    void splitBehaviorValues();
    void alignmentValues();
    void textFlagValues();
    void imageConversionFlagValues();
    void keyValues();
    void penValues();
    void brushStyleValues();
    void cursorShapeValues();
    void textFormatValues();
    void timeSpecValues();
    void scrollBarPolicyValues();
    void caseSensitivityValues();
    void fillRuleValues();
    void clipOperationValues();
    void transformationModeValues();
    void layoutDirectionValues();
    void checkStateValues();
    void itemDataRoleValues();
    void itemFlagsValues();
    void timerTypeValues();
    void globalColorValues();
    void coexistWithGlobalEnums();
    void coexistAxisEnum();
};

#undef Q_SLOTS
#undef Q_OBJECT
