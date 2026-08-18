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
};
