#include "test_element.h"

#include "../PkXmlDocument.h"
#include "../PkXmlElement.h"

void TestElement::attributeDefaultAndEmptySemantics()
{
    // 探针 P5：attribute(name, default) 与 hasAttribute(name) 语义独立，
    // 不能用"返回值是否等于默认值"判断 key 存在与否。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<e a=\"\"/>")));
    PkXmlElement e = doc.documentElement();

    PK_COMPARE(e.attribute("missing", "DEF"), PkString("DEF"));
    PK_COMPARE(e.attribute("a"), PkString("")); // key 存在、值为空串、不传默认值
    PK_VERIFY(!e.hasAttribute("missing"));
    PK_VERIFY(e.hasAttribute("a"));
}

void TestElement::setAttributeHasAttributeRemoveAttribute()
{
    PkXmlDocument doc;
    PkXmlElement e = doc.createElement("e");
    doc.appendChild(e);

    PK_VERIFY(!e.hasAttribute("x"));
    e.setAttribute("x", "1");
    PK_VERIFY(e.hasAttribute("x"));
    PK_COMPARE(e.attribute("x"), PkString("1"));

    e.setAttribute("x", "2"); // 覆盖已有属性，不是追加第二个同名属性
    PK_COMPARE(e.attribute("x"), PkString("2"));

    PkXmlAttr node = e.attributeNode("x");
    PK_VERIFY(!node.isNull());
    PK_COMPARE(node.name(), PkString("x"));
    PK_COMPARE(node.value(), PkString("2"));

    e.removeAttribute("x");
    PK_VERIFY(!e.hasAttribute("x"));
    PK_VERIFY(e.attributeNode("x").isNull());
}

void TestElement::firstLastChildElement()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root><a/><b/><a/></root>")));
    PkXmlElement root = doc.documentElement();

    PK_COMPARE(root.firstChildElement().tagName(), PkString("a"));
    PK_COMPARE(root.firstChildElement("b").tagName(), PkString("b"));
    PK_COMPARE(root.lastChildElement().tagName(), PkString("a"));
    PK_COMPARE(root.lastChildElement("b").tagName(), PkString("b"));

    PkXmlElement firstA = root.firstChildElement("a");
    PK_COMPARE(firstA.nextSiblingElement().tagName(), PkString("b"));
    PK_COMPARE(firstA.nextSiblingElement("a").tagName(), PkString("a"));

    PkXmlElement lastA = root.lastChildElement("a");
    PK_COMPARE(lastA.previousSiblingElement().tagName(), PkString("b"));
    PK_COMPARE(lastA.previousSiblingElement("a").tagName(), PkString("a"));

    PK_VERIFY(root.firstChildElement("nope").isNull());
}

void TestElement::elementsByTagNameRecursesFullSubtree()
{
    // 探针 P7：elementsByTagName 是递归全子树查找，三个 <x/> 全部命中
    // （嵌套在 <a>/<b> 里的两个 + 顶层一个），返回顺序是文档序。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root><a><x/></a><b><x/></b><x/></root>")));
    PkXmlNodeList list = doc.documentElement().elementsByTagName("x");
    PK_COMPARE(list.count(), 3);
    PK_COMPARE(list.size(), 3);   // size() 起别名转发
    PK_COMPARE(list.length(), 3); // length() 起别名转发
    PK_VERIFY(list.at(0).toElement().parentNode().toElement().tagName() == PkString("a"));
    PK_VERIFY(list.item(1).toElement().parentNode().toElement().tagName() == PkString("b"));
    PK_VERIFY(list.at(2).toElement().parentNode().toElement().tagName() == PkString("root"));

    // 三层嵌套：命中元素自己的子树里还有同名元素，不能提前剪枝。
    PkXmlDocument nested;
    PK_VERIFY(nested.setContent(PkString("<r><y><y><y/></y></y></r>")));
    PK_COMPARE(nested.documentElement().elementsByTagName("y").count(), 3);
}

void TestElement::textRecursesFullSubtree()
{
    // 探针 P8：text() 递归拼接全部后代文本，不只是直接子文本。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<e>foo<sub>ignored</sub>bar</e>")));
    PK_COMPARE(doc.documentElement().text(), PkString("fooignoredbar"));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_element.inc"

int run_element_tests(int argc, char **argv)
{
    TestElement tc;
    return PkTest::qExec(&tc, argc, argv);
}
