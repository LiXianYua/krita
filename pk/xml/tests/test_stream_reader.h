#pragma once
#include <QObject> // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestStreamReader : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void tokenSequenceMatchesProbeP11();
    void attributesValueAndHasAttribute();
    void malformedXmlReportsError();
    void raiseErrorSetsErrorStateAndAtEnd();
    void readNextStartElementSkipsUnknownChildren();
    void attributesAtIndexNameAndValue();

    // R-25 Task 3：lineNumber()/columnNumber()，逐字对照探针 P16 原始输出
    // （docs/superpowers/plans/R-25.md）的数值。
    void lineNumberColumnNumberAfterConstructionFailureReusesDomOffsetAlgorithm();
    void lineNumberColumnNumberAfterReadNextStartElementMatchesProbeP16();
    void lineNumberColumnNumberUnaffectedByRaiseError();
};
