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
    void orphanElementNotAppendedDoesNotPolluteToString();
    void orphanBeforeAppendedRootDoesNotBreakDocumentElement();
    void setContentPreservingWhitespaceKeepsBlankTextNodes();
    void importNodeDeepCopiesSubtreeAndAttributesAcrossDocuments();
    void importNodeShallowCopiesOnlyAttributesNotChildren();
    void importNodeOfNullNodeReturnsNull();
    void importNodeWithinSameDocumentIsCopyNotMove();

    // R-25 Task 2：setContent(PkStream*, ...) 两个重载。
    void setContentFromStreamPlain();
    void setContentFromStreamWithNamespaceProcessingFlag();
    void setContentFromStreamReadsFromCurrentPositionNotFromStart();
    void setContentFromStreamPreservesNonAsciiEncodingWithoutPkStringRoundTrip();

    // R-25 Task 2：setContent(PkXmlStreamReader*, bool, ...)——跟 SvgParser.cpp:201
    // 调用形状对齐的往返测试。
    void setContentFromStreamReaderRoundTripsSameTreeAsPkStringSetContent();
};
