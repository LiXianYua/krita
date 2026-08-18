#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestElement : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void attributeDefaultAndEmptySemantics();
    void setAttributeHasAttributeRemoveAttribute();
    void firstLastChildElement();
    void elementsByTagNameRecursesFullSubtree();
    void textRecursesFullSubtree();
};
