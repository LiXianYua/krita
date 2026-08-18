#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestDocument : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void createElementAndAppendChild();
    void setContentPlain();
    void setContentWithNamespaceProcessingFlag();
    void setContentReportsErrorOnMalformedXml();
    void toStringIndentModes();
    void documentElementAndDoctype();
};
