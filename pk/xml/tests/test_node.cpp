#include "test_node.h"

#include "../PkXmlDocument.h"
#include "../PkXmlElement.h"

void TestNode::childNodesDropsPureWhitespaceText()
{
    // 探针 P1：元素间纯空白文本节点——默认不保留。用探针原始输入构造断言，
    // root.childNodes().count() 实测 = 2（不是天真假设的 5）。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root>\n  <a>1</a>\n  <b>2</b>\n</root>")));
    PkXmlElement root = doc.documentElement();
    PkXmlNodeList children = root.childNodes();
    PK_COMPARE(children.count(), 2);
    PK_COMPARE(children.at(0).toElement().tagName(), PkString("a"));
    PK_COMPARE(children.at(1).toElement().tagName(), PkString("b"));
}

void TestNode::firstChildNextSiblingTraversal()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root><a/><b/><c/></root>")));
    PkXmlElement root = doc.documentElement();

    PkXmlNode first = root.firstChild();
    PK_VERIFY(!first.isNull());
    PK_COMPARE(first.toElement().tagName(), PkString("a"));

    PkXmlNode second = first.nextSibling();
    PK_COMPARE(second.toElement().tagName(), PkString("b"));
    PK_COMPARE(second.previousSibling().toElement().tagName(), PkString("a"));

    PkXmlNode last = root.lastChild();
    PK_COMPARE(last.toElement().tagName(), PkString("c"));
    PK_VERIFY(last.nextSibling().isNull());

    PK_VERIFY(root.hasChildNodes());
    PK_VERIFY(!last.hasChildNodes());
    PK_COMPARE(second.parentNode().toElement().tagName(), PkString("root"));
}

void TestNode::toElementToTextIsNullDiscriminate()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<e>hello<![CDATA[cd]]></e>")));
    PkXmlElement e = doc.documentElement();
    PkXmlNodeList kids = e.childNodes();
    PK_COMPARE(kids.count(), 2);

    PkXmlNode textNode = kids.at(0);
    PK_VERIFY(textNode.isText());
    PK_VERIFY(!textNode.isElement());
    PK_VERIFY(!textNode.isCDATASection());
    PK_VERIFY(textNode.toElement().isNull()); // toElement() 在非 Element 节点上返回 null
    PK_VERIFY(!textNode.toText().isNull());
    PK_COMPARE(textNode.toText().data(), PkString("hello"));

    PkXmlNode cdataNode = kids.at(1);
    PK_VERIFY(cdataNode.isCDATASection());
    PK_VERIFY(!cdataNode.isText()); // isText() 只匹配真正的 Text，不含 CDATASection
    PK_VERIFY(!cdataNode.toCDATASection().isNull());
    PK_COMPARE(cdataNode.toCDATASection().data(), PkString("cd"));
    PK_VERIFY(cdataNode.toText().isNull()); // toText() 在 CDATA 节点上返回 null

    PkXmlNode nullNode;
    PK_VERIFY(nullNode.isNull());
    PK_VERIFY(!nullNode.isElement());
    PK_VERIFY(nullNode.toElement().isNull());
}

void TestNode::insertBeforeAndRemoveChild()
{
    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("root");
    doc.appendChild(root);

    PkXmlElement a = doc.createElement("a");
    PkXmlElement c = doc.createElement("c");
    root.appendChild(a);
    root.appendChild(c);

    PkXmlElement b = doc.createElement("b");
    root.insertBefore(b, c);

    PkXmlNodeList kids = root.childNodes();
    PK_COMPARE(kids.count(), 3);
    PK_COMPARE(kids.at(0).toElement().tagName(), PkString("a"));
    PK_COMPARE(kids.at(1).toElement().tagName(), PkString("b"));
    PK_COMPARE(kids.at(2).toElement().tagName(), PkString("c"));

    // refChild 为 null 时 insertBefore 等价于 appendChild（Qt 语义）。
    PkXmlElement d = doc.createElement("d");
    root.insertBefore(d, PkXmlNode());
    PK_COMPARE(root.childNodes().count(), 4);
    PK_COMPARE(root.lastChild().toElement().tagName(), PkString("d"));

    PK_VERIFY(root.removeChild(b));
    PkXmlNodeList afterRemove = root.childNodes();
    PK_COMPARE(afterRemove.count(), 3);
    PK_COMPARE(afterRemove.at(0).toElement().tagName(), PkString("a"));
    PK_COMPARE(afterRemove.at(1).toElement().tagName(), PkString("c"));
    PK_COMPARE(afterRemove.at(2).toElement().tagName(), PkString("d"));
}

void TestNode::lineNumberColumnNumberMatchesProbeP15ThreeLevelNesting()
{
    // 探针 P15 原始输入与原始输出（docs/superpowers/plans/R-25.md）：
    //   <root>          → root: line=1 col=6（落在闭合 '>' 上）
    //     <a>           → a:    line=2 col=5（同上，落在闭合 '>' 上）
    //       <b x='1'/>  → b:    line=3 col=13（自闭合，落在 '/' 上，不是 '>'）
    //     </a>
    //   </root>
    // 逐字对照，不是"非负就算过"。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root>\n  <a>\n    <b x='1'/>\n  </a>\n</root>")));

    PkXmlElement root = doc.documentElement();
    PK_COMPARE(root.lineNumber(), 1);
    PK_COMPARE(root.columnNumber(), 6);

    PkXmlElement a = root.firstChildElement();
    PK_COMPARE(a.tagName(), PkString("a"));
    PK_COMPARE(a.lineNumber(), 2);
    PK_COMPARE(a.columnNumber(), 5);

    PkXmlElement b = a.firstChildElement();
    PK_COMPARE(b.tagName(), PkString("b"));
    PK_COMPARE(b.lineNumber(), 3);
    PK_COMPARE(b.columnNumber(), 13);
}

void TestNode::lineNumberColumnNumberOfTextNodeWithEmbeddedNewline()
{
    // 探针 P15：`<r>hello\nworld</r>`（真实换行在文本内容里）——
    // "text node: line=2 col=6"，落在"文本内容读完"那一刻（"world" 的 'd'
    // 之后一格），不是文本起始位置。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<r>hello\nworld</r>")));

    PkXmlElement r = doc.documentElement();
    PkXmlNode textNode = r.firstChild();
    PK_VERIFY(textNode.isText());
    PK_COMPARE(textNode.toText().data(), PkString("hello\nworld"));
    PK_COMPARE(textNode.lineNumber(), 2);
    PK_COMPARE(textNode.columnNumber(), 6);
}

void TestNode::lineNumberColumnNumberOfUnparsedOrphanNodeIsNegativeOne()
{
    // 探针 P15：程序 create* 出来、从没经过 setContent() 解析的节点——恒 -1/-1，
    // 不需要真的 appendChild()（哪怕未挂树，limbo 容器里的孤儿节点同样适用，
    // 见 PkXmlNode.h 顶部"创建即挂树"注释）。
    PkXmlDocument doc;
    PkXmlElement orphan = doc.createElement("x");
    PK_COMPARE(orphan.lineNumber(), -1);
    PK_COMPARE(orphan.columnNumber(), -1);
}

void TestNode::lineNumberColumnNumberOfIsNullElementIsNegativeOne()
{
    // 探针 P15："missing element: isNull=1 line=-1 col=-1"。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root/>")));
    PkXmlElement missing = doc.documentElement().firstChildElement("nope");
    PK_VERIFY(missing.isNull());
    PK_COMPARE(missing.lineNumber(), -1);
    PK_COMPARE(missing.columnNumber(), -1);
}

void TestNode::lineNumberColumnNumberOfAttrNodeIsNegativeOne()
{
    // 探针 P15："attr x: isNull=0 line=-1 col=-1"——即便属性真实存在、来自
    // 成功解析的文档，Qt DOM 也不为 QDomAttr 记录位置信息，恒返 -1。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root x='1'/>")));
    PkXmlElement root = doc.documentElement();
    PkXmlAttr attr = root.attributeNode("x");
    PK_VERIFY(!attr.isNull());
    PK_COMPARE(attr.lineNumber(), -1);
    PK_COMPARE(attr.columnNumber(), -1);
}

void TestNode::lineNumberColumnNumberOfTextNodeWithEntityIsNotDecodedLengthOffByThree()
{
    // 全分支终审发现的回归：pugixml parse_default 默认解码实体引用
    // （parse_escapes），PkXmlText::data() 拿到的是解码后内容——文本节点的
    // lineNumber()/columnNumber() 必须按原始字节长度换算，不能用
    // std::strlen(解码后内容)。"&amp;" 在原始缓冲区占 5 字节，解码后只剩 1
    // 个 '&' 字符——如果误用解码后长度，算出的 offset 会少 4，落在文本内容
    // 中间而不是真正的结尾。
    //
    // 输入 <r>x&amp;y</r>：文本内容原始字节是 x,&,a,m,p,;,y 共 7 字节，从
    // offset=3（'x' 起始）开始，结尾（</r> 的 '<'）落在 offset=10，1-based
    // col = 10 + 1 = 11。解码后内容是 x,&,y 共 3 字节，若误用解码后长度会
    // 算出 offset=3+3=6（col=7），与正确值相差 4——正好是 "&amp;" 比单个
    // '&' 多出的字节数，本测试专门钉住这条。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<r>x&amp;y</r>")));

    PkXmlElement r = doc.documentElement();
    PkXmlNode textNode = r.firstChild();
    PK_VERIFY(textNode.isText());
    PK_COMPARE(textNode.toText().data(), PkString("x&y")); // 解码后内容
    PK_COMPARE(textNode.lineNumber(), 1);
    PK_COMPARE(textNode.columnNumber(), 11); // 原始字节结尾，不是 7
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_node.inc"

int run_node_tests(int argc, char **argv)
{
    TestNode tc;
    return PkTest::qExec(&tc, argc, argv);
}
