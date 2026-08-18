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

void TestDocument::orphanElementNotAppendedDoesNotPolluteToString()
{
    // R-07 全分支终审 I1：createElement() 之后不 appendChild 就直接
    // toString()——早先"挂文档根"的实现会把孤儿一起序列化出来，产出两个
    // 根元素的非法 XML。本任务自己的试接目标
    // `kis_distance_information_test.cpp` 的四个 `doc.createElement("TestN")`
    // 就是这个真实场景：只用来接住 `toXML()` 写进去的属性，从不 appendChild。
    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("root");
    doc.appendChild(root);

    PkXmlElement orphan = doc.createElement("Orphan");
    orphan.setAttribute("x", "1"); // 即便继续操作孤儿本身，也不该泄漏到输出里

    // toString() 只能看到真正挂树的 root，看不到 orphan——不是两个根元素的
    // 非法 XML。
    PK_COMPARE(doc.toString(-1), PkString("<root/>"));
    PK_VERIFY(!doc.toString(-1).contains(PkString("Orphan")));

    // 对照组：orphan 真正 appendChild 之后必须出现在输出里，证明"从 limbo
    // 搬到目标位置"的迁移逻辑没有破坏正常路径。
    root.appendChild(orphan);
    PK_COMPARE(doc.toString(-1), PkString("<root><Orphan x=\"1\"/></root>"));
}

void TestDocument::orphanBeforeAppendedRootDoesNotBreakDocumentElement()
{
    // R-07 全分支终审 I1：孤儿节点先于 root 创建时，documentElement() 不该
    // 被误导——早先"挂文档根"的实现里，孤儿会被 documentElement() 当成"第一个
    // 元素子节点"错误返回。
    PkXmlDocument doc;
    PkXmlElement orphan = doc.createElement("Orphan"); // 先创建，不 append
    PK_VERIFY(doc.documentElement().isNull()); // 还没有真正的根，不是 orphan

    PkXmlElement root = doc.createElement("root");
    doc.appendChild(root);
    PK_COMPARE(doc.documentElement().tagName(), PkString("root"));

    // 文档级 childNodes() 也只看得到真正挂树的 root，看不到 limbo/orphan。
    PK_COMPARE(doc.childNodes().count(), 1);

    // orphan 的 parentNode() 规整化报告成文档本身（不是 Qt 的 null，也不暴露
    // limbo 这个内部实现节点——见 PkXmlNode.h 顶部类注释）。
    PK_VERIFY(!orphan.parentNode().isNull());
    PK_VERIFY(!orphan.parentNode().isElement());
}

void TestDocument::setContentPreservingWhitespaceKeepsBlankTextNodes()
{
    // R-07 全分支终审 I2：普通 setContent()（P1 探针对齐 Qt）丢弃纯空白文本
    // 节点；setContentPreservingWhitespace() 对应 pugixml 的 parse_ws_pcdata，
    // 保留它们——真实调用点 `SvgParser.cpp` 依赖这个（SVG 文本里空白有语义，
    // 见 README 已知偏离清单 I2）。
    const PkString xml("<root>\n  <a>1</a>\n  <b>2</b>\n</root>");

    PkXmlDocument plain;
    PK_VERIFY(plain.setContent(xml));
    PK_COMPARE(plain.documentElement().childNodes().count(), 2); // P1：纯空白节点被丢弃

    PkXmlDocument preserved;
    PK_VERIFY(preserved.setContentPreservingWhitespace(xml));
    // text,a,text,b,text —— P1 探针注释提到的"天真假设"的 5 个子节点，
    // parse_ws_pcdata 打开之后真的产出这 5 个。
    PK_COMPARE(preserved.documentElement().childNodes().count(), 5);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_document.inc"

int run_document_tests(int argc, char **argv)
{
    TestDocument tc;
    return PkTest::qExec(&tc, argc, argv);
}
