#pragma once
// PkPainterPath 的 T1 单测用例。
// 覆盖：元素存储 + 构建核心 + 基础查询 + 变换 + 简单形状附加 + 比较。
//
// 函数定义在 test_painterpath.cpp。

#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkPainterPath;

class PkPainterPathCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // 构造与基本状态
    void defaultCtor();
    void startPointCtor();
    void copyAndAssignment();

    // Element 类型
    void elementTypeEnum();
    void elementAccessors();

    // 构建核心
    void moveTo();
    void lineTo();
    void cubicTo();
    void quadTo();
    void closeSubpath();
    void currentPosition();

    // 清理与预留
    void clearAndReserve();

    // 查询
    void isEmpty();
    void boundingRect();
    void controlPointRect();
    void isClosed();
    void elementCountAndAt();
    void setElementPositionAt();

    // 填充规则
    void fillRule();

    // 简单形状附加
    void addRect();
    void addPolygon();
    void addPath();

    // 形状辅助（T2）
    void addEllipse();
    void arcTo();
    void addRoundedRect();

    // 变换
    void translate();
    void translated();

    // 比较
    void equality();
};