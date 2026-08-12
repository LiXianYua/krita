#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class PkLoggingCategoryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testThreeArgFormSetsMinimumLevel();
    void testDisabledCategoryDoesNotEvaluateArguments();
    void testEnabledCategoryEvaluatesArgumentsOnce();
    void testCategoryNameReachesSink();
};
