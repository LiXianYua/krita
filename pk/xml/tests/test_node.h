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
};
