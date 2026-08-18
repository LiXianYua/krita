#include "test_namespace.h"

#include "../PkXmlDocument.h"
#include "../PkXmlElement.h"
#include "../PkXmlImplementation.h"

void TestNamespace::localNameAndNamespaceURI()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(
        PkString("<root xmlns:draw=\"http://ns.example/draw\"><draw:foo/></root>")));
    PkXmlElement foo = doc.documentElement().firstChildElement();
    PK_COMPARE(foo.tagName(), PkString("draw:foo"));
    PK_COMPARE(foo.localName(), PkString("foo"));
    PK_COMPARE(foo.namespaceURI(), PkString("http://ns.example/draw"));

    // 无前缀元素：本地名就是标签名本身，命名空间 URI 查不到就是空串
    // （没有默认 xmlns 声明）。
    PkXmlElement root = doc.documentElement();
    PK_COMPARE(root.localName(), PkString("root"));
    PK_COMPARE(root.namespaceURI(), PkString(""));
}

void TestNamespace::attributeNSResolvesPrefixedAttribute()
{
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString(
        "<root xmlns:xlink=\"http://www.w3.org/1999/xlink\"><a xlink:href=\"target\"/></root>")));
    PkXmlElement a = doc.documentElement().firstChildElement();
    PK_COMPARE(a.attributeNS("http://www.w3.org/1999/xlink", "href"), PkString("target"));
    PK_COMPARE(a.attributeNS("http://wrong.ns", "href", "DEF"), PkString("DEF"));
}

void TestNamespace::implementationCreateDocumentType()
{
    PkXmlImplementation impl;
    PkXmlDocumentType dt = impl.createDocumentType("mydoc", "", "");
    PK_VERIFY(!dt.isNull());
    PK_COMPARE(dt.name(), PkString("mydoc"));

    // 与 doc.doctype() 的路径一致：通过 setContent 解析出的 DOCTYPE 也要能
    // 拿到同样的 name() 语义。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<!DOCTYPE mydoc><mydoc/>")));
    PK_COMPARE(doc.doctype().name(), PkString("mydoc"));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_namespace.inc"

int run_namespace_tests(int argc, char **argv)
{
    TestNamespace tc;
    return PkTest::qExec(&tc, argc, argv);
}
