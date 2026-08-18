#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestNode : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void childNodesDropsPureWhitespaceText();
    void firstChildNextSiblingTraversal();
    void toElementToTextIsNullDiscriminate();
    void insertBeforeAndRemoveChild();

    // R-25 Task 3：lineNumber()/columnNumber()，逐字对照探针 P15 原始输出
    // （docs/superpowers/plans/R-25.md）的数值，不是"非负就算过"。
    void lineNumberColumnNumberMatchesProbeP15ThreeLevelNesting();
    void lineNumberColumnNumberOfTextNodeWithEmbeddedNewline();
    void lineNumberColumnNumberOfUnparsedOrphanNodeIsNegativeOne();
    void lineNumberColumnNumberOfIsNullElementIsNegativeOne();
    void lineNumberColumnNumberOfAttrNodeIsNegativeOne();
    void lineNumberColumnNumberOfTextNodeWithEntityIsNotDecodedLengthOffByThree();
};
