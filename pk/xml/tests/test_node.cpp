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

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_node.inc"

int run_node_tests(int argc, char **argv)
{
    TestNode tc;
    return PkTest::qExec(&tc, argc, argv);
}
