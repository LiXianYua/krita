#include "test_document.h"

#include "../PkXmlDocument.h"
#include "../PkXmlElement.h"

void TestDocument::createElementAndAppendChild()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.isNull() == false); // 文档节点本身不是 null

    PkXmlElement root = doc.createElement("root");
    PK_VERIFY(!root.isNull());
    doc.appendChild(root);

    PkXmlElement child = doc.createElement("child");
    child.setAttribute("k", "v");
    root.appendChild(child);
    // Krita 里最常见的写法：appendChild 之后继续用原变量设属性——验证
    // append_move 保住了节点身份（见 PkXmlNode.h 的设计说明）。
    child.setAttribute("k2", "v2");

    PK_COMPARE(doc.documentElement().tagName(), PkString("root"));
    PK_COMPARE(doc.documentElement().firstChildElement().tagName(), PkString("child"));
    PK_COMPARE(doc.documentElement().firstChildElement().attribute("k"), PkString("v"));
    PK_COMPARE(doc.documentElement().firstChildElement().attribute("k2"), PkString("v2"));
}

void TestDocument::setContentPlain()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root><child k=\"v\">hello</child></root>")));
    PK_COMPARE(doc.documentElement().tagName(), PkString("root"));
    PK_COMPARE(doc.documentElement().firstChildElement().text(), PkString("hello"));
}

void TestDocument::setContentWithNamespaceProcessingFlag()
{
    // 两个重载走同一套解析流程（关键实现说明 5）——namespaceProcessing=true
    // 不应该改变基本 setContent 的成功/失败结果。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root><child/></root>"), true));
    PK_COMPARE(doc.documentElement().tagName(), PkString("root"));
}

void TestDocument::setContentReportsErrorOnMalformedXml()
{
    // 探针 P4：`<root><a></root>` 是标签不匹配。errLine/errCol 是 1-based。
    // pugixml 的解析器与 Qt 不是同一套实现，errMsg 原文不追求逐字节对齐 Qt
    // （Qt: "tag mismatch"；pugixml 实测: "Start-end tags mismatch"，两者语义
    // 一致），但 errLine/errCol 必须是正确算出来的 1-based 值——已用本机
    // pugixml v1.16 实测：offset=11 时 line=1 col=12（README 记录了这条实测）。
    PkXmlDocument doc;
    PkString errMsg;
    int errLine = -1;
    int errCol = -1;
    const bool ok = doc.setContent(PkString("<root><a></root>"), &errMsg, &errLine, &errCol);
    PK_VERIFY(!ok);
    PK_VERIFY(!errMsg.isEmpty());
    PK_COMPARE(errLine, 1);
    PK_COMPARE(errCol, 12);
}

void TestDocument::toStringIndentModes()
{
    // 探针 P3 的三条原始输出，逐字节抄。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root><child k=\"v\">hello</child></root>")));

    PK_COMPARE(doc.toString(4),
               PkString("<root>\n    <child k=\"v\">hello</child>\n</root>\n"));
    PK_COMPARE(doc.toString(0), PkString("<root>\n<child k=\"v\">hello</child>\n</root>\n"));
    PK_COMPARE(doc.toString(-1), PkString("<root><child k=\"v\">hello</child></root>"));
}

void TestDocument::documentElementAndDoctype()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.doctype().isNull()); // 空文档没有 doctype

    PK_VERIFY(doc.setContent(PkString("<!DOCTYPE note SYSTEM \"Note.dtd\"><note>hi</note>")));
    PK_COMPARE(doc.doctype().name(), PkString("note"));
    PK_COMPARE(doc.documentElement().tagName(), PkString("note"));

    // QDomDocument(const QString &name) 构造函数：只设 doctype 的 name。
    PkXmlDocument named(PkString("mydoc"));
    PK_COMPARE(named.doctype().name(), PkString("mydoc"));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_document.inc"

int run_document_tests(int argc, char **argv)
{
    TestDocument tc;
    return PkTest::qExec(&tc, argc, argv);
}
