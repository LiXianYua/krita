#include "test_document.h"

#include "../PkXmlDocument.h"
#include "../PkXmlElement.h"
#include "../PkXmlStreamReader.h"
#include "../../port/PkStream.h"

#include <cstring>

namespace {

// R-25 Task 2：最小的内存 PkStream 子类，只为驱动 setContent(PkStream*, ...)
// 两个重载的测试——同 pk/port/tests/test_stream.cpp 的 MemoryStream（那份
// 测试不在本任务 locks 范围内，不能直接复用它的 .cpp，这里照抄同一个模式）。
// 非顺序设备：readData() 按 pos() 索引自己的 std::string。
class MemoryStream : public PkStream
{
public:
    explicit MemoryStream(std::string data) : m_data(std::move(data)) {}

    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 avail = static_cast<pk_int64>(m_data.size()) - pos();
        if (avail <= 0) {
            return 0; // EOF，不是错误。
        }
        const pk_int64 n = maxSize < avail ? maxSize : avail;
        std::memcpy(data, m_data.data() + pos(), static_cast<std::size_t>(n));
        return n;
    }

    pk_int64 writeData(const char *, pk_int64) override { return -1; } // 只读测试用途

private:
    std::string m_data;
};

} // namespace

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

void TestDocument::importNodeDeepCopiesSubtreeAndAttributesAcrossDocuments()
{
    // 探针 P13：deep=true 是深拷贝——完整复制子树 + 属性，源节点不受影响
    // （不是"移动"）。跨 document 场景：src/dst 是两棵完全独立的树。
    PkXmlDocument src;
    PK_VERIFY(src.setContent(PkString("<root><a x=\"1\"><b>text</b></a></root>")));
    PkXmlElement srcA = src.documentElement().firstChildElement("a");
    PK_VERIFY(!srcA.isNull());

    PkXmlDocument dst;
    PkXmlElement dstRoot = dst.createElement("root");
    dst.appendChild(dstRoot);

    PkXmlNode imported = dst.importNode(srcA, true);
    PK_VERIFY(!imported.isNull());
    PkXmlElement importedEl = imported.toElement();
    PK_VERIFY(!importedEl.isNull());
    PK_COMPARE(importedEl.tagName(), PkString("a"));
    PK_COMPARE(importedEl.attribute("x"), PkString("1"));
    PK_COMPARE(importedEl.childNodes().count(), 1); // 深拷贝：子节点 b 也被复制
    PK_COMPARE(importedEl.firstChildElement("b").text(), PkString("text"));

    // 源节点不受影响——探针 P13 的"src still has 'a' child after import: 1"。
    PK_COMPARE(src.documentElement().firstChildElement("a").childNodes().count(), 1);
    PK_COMPARE(src.documentElement().firstChildElement("a").attribute("x"), PkString("1"));

    // 返回值未挂树——appendChild() 之前不出现在 toString() 里，之后才出现。
    PK_VERIFY(!dst.toString(-1).contains(PkString("<a ")));
    dstRoot.appendChild(importedEl);
    PK_COMPARE(dst.toString(-1), PkString("<root><a x=\"1\"><b>text</b></a></root>"));
}

void TestDocument::importNodeShallowCopiesOnlyAttributesNotChildren()
{
    // 探针 P13：deep=false 只拷贝元素自身 + 属性，不拷贝子节点
    // （"deep=false: isNull=0 nodeName=a hasChildNodes=0 childCount=0 attr-x=1"）。
    PkXmlDocument src;
    PK_VERIFY(src.setContent(PkString("<root><a x=\"1\"><b>text</b></a></root>")));
    PkXmlElement srcA = src.documentElement().firstChildElement("a");

    PkXmlDocument dst;
    PkXmlNode imported = dst.importNode(srcA, false);
    PK_VERIFY(!imported.isNull());
    PkXmlElement importedEl = imported.toElement();
    PK_VERIFY(!importedEl.isNull());
    PK_COMPARE(importedEl.tagName(), PkString("a"));
    PK_COMPARE(importedEl.attribute("x"), PkString("1"));
    PK_VERIFY(!importedEl.hasChildNodes());
    PK_COMPARE(importedEl.childNodes().count(), 0);
}

void TestDocument::importNodeOfNullNodeReturnsNull()
{
    // 探针 P13："import null node: isNull=1"。
    PkXmlDocument src;
    PK_VERIFY(src.setContent(PkString("<root/>")));
    PkXmlNode missing = src.documentElement().firstChildElement("doesNotExist");
    PK_VERIFY(missing.isNull());

    PkXmlDocument dst;
    PkXmlNode imported = dst.importNode(missing, true);
    PK_VERIFY(imported.isNull());
}

void TestDocument::importNodeWithinSameDocumentIsCopyNotMove()
{
    // 探针 P13："即便是同文档内调用也一样是拷贝，不是原地复用"。
    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(PkString("<root><a x=\"1\"><b>text</b></a></root>")));
    PkXmlElement a = doc.documentElement().firstChildElement("a");

    PkXmlNode imported = doc.importNode(a, true);
    PK_VERIFY(!imported.isNull());
    PkXmlElement importedEl = imported.toElement();
    PK_COMPARE(importedEl.tagName(), PkString("a"));
    PK_COMPARE(importedEl.attribute("x"), PkString("1"));

    // 原节点还在原位，没有被"移走"。
    PK_COMPARE(doc.documentElement().firstChildElement("a").childNodes().count(), 1);

    // appendChild 之后，root 下有两个 "a"（原来那个 + 新拷贝的这个）。
    doc.documentElement().appendChild(importedEl);
    int aCount = 0;
    PkXmlNode n = doc.documentElement().firstChild();
    while (!n.isNull()) {
        if (n.isElement() && n.toElement().tagName() == PkString("a")) {
            ++aCount;
        }
        n = n.nextSibling();
    }
    PK_COMPARE(aCount, 2);
}

void TestDocument::setContentFromStreamPlain()
{
    // 正常路径：设备从头到尾就是一份完整合法 XML。
    MemoryStream stream(std::string("<root><child k=\"v\">hello</child></root>"));
    stream.open(PkStream::ReadOnly);

    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(&stream));
    PK_COMPARE(doc.documentElement().tagName(), PkString("root"));
    PK_COMPARE(doc.documentElement().firstChildElement().text(), PkString("hello"));
}

void TestDocument::setContentFromStreamWithNamespaceProcessingFlag()
{
    // bool 重载：跟 PkString 版本一样，namespaceProcessing 不改变基本解析结果
    // （pugixml 本身不做命名空间解析）。
    MemoryStream stream(std::string("<root><child/></root>"));
    stream.open(PkStream::ReadOnly);

    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(&stream, true));
    PK_COMPARE(doc.documentElement().tagName(), PkString("root"));
}

void TestDocument::setContentFromStreamReadsFromCurrentPositionNotFromStart()
{
    // 复刻探针 P14："从设备当前位置开始读，不会自己 seek(0)"。前缀 4 个字节
    // 不是合法 XML（"JUNK"），seek(4) 跳过它们之后剩下的才是合法文档——若实现
    // 错误地自己 seek(0)，会把 "JUNK<root>..." 一起喂给解析器，直接失败。
    const std::string data = "JUNK<root><a>1</a></root>";
    MemoryStream stream(data);
    stream.open(PkStream::ReadOnly);
    PK_VERIFY(stream.seek(4));

    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(&stream));
    PK_COMPARE(doc.documentElement().tagName(), PkString("root"));
    PK_COMPARE(doc.documentElement().firstChildElement("a").text(), PkString("1"));

    // 读到 EOF 为止——探针 P14 的 "devicePosAfter" 等于设备总长度。
    PK_COMPARE(stream.pos(), static_cast<PkStream::pk_int64>(data.size()));
}

void TestDocument::setContentFromStreamPreservesNonAsciiEncodingWithoutPkStringRoundTrip()
{
    // 复刻探针 P14 第三条："with-xml-decl setContent: ok=1 attr=é"——带
    // `encoding="UTF-8"` 声明、属性值含多字节 UTF-8 字符（é = 0xC3 0xA9）
    // 的文档必须正确解出。设计要求字节导向 setContentImpl 直接把原始字节喂给
    // pugi::xml_document::load_buffer，不经过 PkString 中转（PkString::
    // PkFromUtf8 是"假定输入已经是 UTF-8"的转换，不做编码探测）。
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root attr=\"";
    xml += "\xC3\xA9"; // é 的 UTF-8 字节序列
    xml += "\"/>";

    MemoryStream stream(xml);
    stream.open(PkStream::ReadOnly);

    PkXmlDocument doc;
    PK_VERIFY(doc.setContent(&stream));
    const PkString expected = PkString::PkFromUtf8("\xC3\xA9", 2);
    PK_COMPARE(doc.documentElement().attribute("attr"), expected);
}

void TestDocument::setContentFromStreamReaderRoundTripsSameTreeAsPkStringSetContent()
{
    // 跟真实调用点 SvgParser.cpp:201 的调用形状对齐：
    // `doc.setContent(&reader, false, errorMsg, errorLine, errorColumn)`。
    // 往返验证：PkXmlStreamReader 驱动的"流转 DOM"构建器产出的树，跟直接
    // setContent(PkString) 解析同一份文本得到的树，toString() 输出一致。
    const PkString xml("<root><a x=\"1\">hello</a><b/></root>");

    PkXmlStreamReader reader(xml);
    PkXmlDocument fromStream;
    PkString errorMsg;
    int errorLine = -1;
    int errorColumn = -1;
    PK_VERIFY(fromStream.setContent(&reader, false, &errorMsg, &errorLine, &errorColumn));

    PkXmlDocument fromPkString;
    PK_VERIFY(fromPkString.setContent(xml));

    PK_COMPARE(fromStream.toString(-1), fromPkString.toString(-1));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_document.inc"

int run_document_tests(int argc, char **argv)
{
    TestDocument tc;
    return PkTest::qExec(&tc, argc, argv);
}
