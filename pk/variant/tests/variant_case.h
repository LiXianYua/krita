#pragma once

#include <QObject>

// PkVariant 单测的数据驱动用例。
// 与 pk/test 的 harness 约定一致：Q_OBJECT + private Q_SLOTS。
class VariantCase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 基础状态 ────────────────────────────────────────────────────
    void defaultConstruction();
    void clearReset();
    void isValidAndIsNull();

    // ── 基础类型构造与转换 ──────────────────────────────────────────
    void intVariant();
    void uintVariant();
    void longLongVariant();
    void ulongLongVariant();
    void doubleVariant();
    void floatVariant();
    void boolVariant();
    void stringVariant();
    void byteArrayVariant();
    void stringListVariant();

    // ── 集合类型构造与转换 ──────────────────────────────────────────
    void variantListVariant();
    void variantHashVariant();
    void variantMapVariant();

    // ── 几何类型构造与转换 ──────────────────────────────────────────
    void pointVariant();
    void pointFVariant();
    void rectVariant();
    void rectFVariant();
    void sizeVariant();
    void sizeFVariant();
    void lineVariant();
    void lineFVariant();

    // ── 时间类型构造与转换 ──────────────────────────────────────────
    void dateVariant();
    void timeVariant();
    void dateTimeVariant();

    // ── 模板方法 ────────────────────────────────────────────────────
    void fromValueAndValue();
    void canConvert();
    void setValue();

    // ── 转换边角 ────────────────────────────────────────────────────
    void conversionEdgeCases();
    void nanAndInf();
    void copyAndMove();
    void dataPointer();
};