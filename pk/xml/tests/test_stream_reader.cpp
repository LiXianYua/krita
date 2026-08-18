#include "test_stream_reader.h"

#include "../PkXmlStreamReader.h"

void TestStreamReader::tokenSequenceMatchesProbeP11()
{
    // 探针 P11 的原始输出，逐行断言（token 数值、name、text 三项都对齐）：
    //   token=2(StartDocument) name="" text=""
    //   token=4(StartElement)  name="root"  text=""
    //   token=4(StartElement)  name="child" text=""
    //   token=6(Characters)    name=""      text="text"
    //   token=5(EndElement)    name="child" text=""
    //   token=5(EndElement)    name="root"  text=""
    //   token=3(EndDocument)   name=""      text=""
    //   hasError=0
    PkXmlStreamReader xml(PkString("<root a=\"1\"><child>text</child></root>"));
    PK_VERIFY(!xml.hasError());

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartDocument));
    PK_COMPARE(xml.name(), PkString(""));
    PK_COMPARE(xml.text(), PkString(""));

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartElement));
    PK_COMPARE(xml.name(), PkString("root"));
    PK_COMPARE(xml.text(), PkString(""));

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartElement));
    PK_COMPARE(xml.name(), PkString("child"));
    PK_COMPARE(xml.text(), PkString(""));

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::Characters));
    PK_COMPARE(xml.name(), PkString(""));
    PK_COMPARE(xml.text(), PkString("text"));

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::EndElement));
    PK_COMPARE(xml.name(), PkString("child"));
    PK_COMPARE(xml.text(), PkString(""));

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::EndElement));
    PK_COMPARE(xml.name(), PkString("root"));
    PK_COMPARE(xml.text(), PkString(""));

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::EndDocument));
    PK_COMPARE(xml.name(), PkString(""));
    PK_COMPARE(xml.text(), PkString(""));

    PK_VERIFY(xml.atEnd());
    PK_VERIFY(!xml.hasError());

    // 枚举数值本身也要逐一对齐 P11 记录的 Qt 实测值，不能重排。
    PK_COMPARE(static_cast<int>(PkXmlStreamReader::NoToken), 0);
    PK_COMPARE(static_cast<int>(PkXmlStreamReader::Invalid), 1);
    PK_COMPARE(static_cast<int>(PkXmlStreamReader::StartDocument), 2);
    PK_COMPARE(static_cast<int>(PkXmlStreamReader::EndDocument), 3);
    PK_COMPARE(static_cast<int>(PkXmlStreamReader::StartElement), 4);
    PK_COMPARE(static_cast<int>(PkXmlStreamReader::EndElement), 5);
    PK_COMPARE(static_cast<int>(PkXmlStreamReader::Characters), 6);
}

void TestStreamReader::attributesValueAndHasAttribute()
{
    // 真实调用点用量表实测的两个方法：value(name)/hasAttribute(name)（用量
    // 复核时发现 hasAttribute 是 brief 接口清单外的缺口，见
    // PkXmlStreamAttributes.h 类注释）。
    PkXmlStreamReader xml(PkString("<root a=\"1\"><child/></root>"));
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartDocument));
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartElement)); // root
    PK_VERIFY(xml.attributes().hasAttribute("a"));
    PK_COMPARE(xml.attributes().value("a"), PkString("1"));
    PK_VERIFY(!xml.attributes().hasAttribute("nope"));
    PK_COMPARE(xml.attributes().count(), 1);
}

void TestStreamReader::malformedXmlReportsError()
{
    // 标签不匹配（Task 1 用同一个例子实测过 DOM 侧的 setContent，见
    // pk/xml/README.md §2.3）：流式 reader 侧同样应该报错，而不是死循环。
    PkXmlStreamReader xml(PkString("<root><a></root>"));
    PK_VERIFY(xml.hasError());
    PK_VERIFY(!xml.errorString().isEmpty());
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::Invalid));
    PK_VERIFY(xml.atEnd());
}

void TestStreamReader::raiseErrorSetsErrorStateAndAtEnd()
{
    // 真实调用点用法（`libs/pigment/resources/KoColorSet.cpp`）：解析回调发现
    // 数据语义非法时主动 raiseError()，之后 hasError()/errorString()/atEnd()
    // 都要反映出来——不在 brief 的 Interfaces 枚举里，是用量复核发现的缺口，
    // 见 PkXmlStreamReader.h 类注释。
    //
    // 断言值来自本机 Qt 5.15.7（`libQt5Core.so.5.15.7`）的实测探针（评审
    // 发现的 Important，见 task-2-report.md「raiseError 语义核实」一节）：
    // raiseError() 之后 tokenType() 立刻变 Invalid、atEnd()/hasError() 立刻
    // 变 true，但 name()/attributes() 不清空——保留报错前最后一次成功
    // readNext() 留下的值；之后任意多次 readNext() 都恒返回 Invalid，状态
    // 冻结不再变化。
    PkXmlStreamReader xml(PkString("<root a=\"1\"><child/></root>"));
    PK_VERIFY(!xml.hasError());
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartDocument));
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartElement)); // root, a="1"
    PK_VERIFY(!xml.atEnd());

    xml.raiseError("boom");

    // raiseError() 之后立刻生效，不需要再调用 readNext()。
    PK_VERIFY(xml.hasError());
    PK_COMPARE(xml.errorString(), PkString("boom"));
    PK_VERIFY(xml.atEnd());
    PK_COMPARE(static_cast<int>(xml.tokenType()),
               static_cast<int>(PkXmlStreamReader::Invalid));
    // name()/attributes() 保留报错前最后一次成功 readNext()（root 的
    // StartElement）留下的值，不清空。
    PK_COMPARE(xml.name(), PkString("root"));
    PK_COMPARE(xml.attributes().value("a"), PkString("1"));

    // 继续调用 readNext()——不严格遵守 while(!atEnd()) 惯用法、报错后仍然
    // 往下读的真实调用点写法（评审指出 KoColorSet.cpp 的 6 处 raiseError
    // 都是这种"报错后继续走后续逻辑"），状态必须保持冻结，不能静默恢复
    // 正常遍历。
    for (int i = 0; i < 3; ++i) {
        PK_COMPARE(static_cast<int>(xml.readNext()),
                   static_cast<int>(PkXmlStreamReader::Invalid));
        PK_COMPARE(static_cast<int>(xml.tokenType()),
                   static_cast<int>(PkXmlStreamReader::Invalid));
        PK_VERIFY(xml.atEnd());
        PK_VERIFY(xml.hasError());
        PK_COMPARE(xml.errorString(), PkString("boom"));
        PK_COMPARE(xml.name(), PkString("root")); // 仍然冻结，没有被清空或推进
    }
}

void TestStreamReader::readNextStartElementSkipsUnknownChildren()
{
    // R-07 全分支终审 C1：readNextStartElement()/skipCurrentElement() 此前
    // 被线级 plan 用量表误记成"0 调用点，不实现"，实测
    // `libs/pigment/resources/KoColorSet.cpp` 有真实调用点（都是
    // `xml->readNextStartElement()`/`xml->skipCurrentElement()` 这种指针
    // 接收者写法）。真实用法：外层 while(readNextStartElement()) 遍历子
    // 元素，遇到认识的元素处理，遇到不认识的调 skipCurrentElement() 跳过整棵
    // 子树（含它自己的嵌套子元素），最终仍然落在外层元素的 EndElement 上。
    PkXmlStreamReader xml(
        PkString("<root><known>1</known><skip><nested/><nested2>x</nested2></skip>"
                  "<known2>2</known2></root>"));

    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartDocument));
    PK_VERIFY(xml.readNextStartElement()); // root
    PK_COMPARE(xml.name(), PkString("root"));

    PK_VERIFY(xml.readNextStartElement()); // known
    PK_COMPARE(xml.name(), PkString("known"));
    xml.skipCurrentElement(); // 消费掉它自己的 Characters + EndElement

    PK_VERIFY(xml.readNextStartElement()); // skip
    PK_COMPARE(xml.name(), PkString("skip"));
    xml.skipCurrentElement(); // 跳过 <nested/><nested2>x</nested2> 整棵子树

    PK_VERIFY(xml.readNextStartElement()); // known2——跳过 <skip> 之后必须落在这
    PK_COMPARE(xml.name(), PkString("known2"));
    xml.skipCurrentElement();

    // root 自己的子元素已经遍历完——readNextStartElement() 遇到 root 的
    // EndElement，返回 false。这一步只消费到 root 的 EndElement，还没到
    // EndDocument（atEnd() 由 readNext() 在下一次检测到栈已空时才置位，与
    // 真 Qt「readNextStartElement() 返回 false 不等于 atEnd()」的语义一致）。
    PK_VERIFY(!xml.readNextStartElement());
    PK_VERIFY(!xml.atEnd());
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::EndDocument));
    PK_VERIFY(xml.atEnd());
    PK_VERIFY(!xml.hasError());
}

void TestStreamReader::attributesAtIndexNameAndValue()
{
    // R-07 全分支终审 I4：真实调用点
    // `libs/flake/text/KoSvgTextShapeMarkupConverter.cpp:879/881/883`
    // 写的是 `elementAttributes.at(a).name() != "style"`——下标 `at()` +
    // `Attribute::name()`/`value()` 方法调用形态。
    PkXmlStreamReader xml(PkString("<e a=\"1\" b=\"2\"/>"));
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartDocument));
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartElement));

    const PkXmlStreamAttributes attrs = xml.attributes();
    PK_COMPARE(attrs.size(), 2);
    PK_COMPARE(attrs.at(0).name(), PkString("a"));
    PK_COMPARE(attrs.at(0).value(), PkString("1"));
    PK_COMPARE(attrs.at(1).name(), PkString("b"));
    PK_COMPARE(attrs.at(1).value(), PkString("2"));
}

void TestStreamReader::lineNumberColumnNumberAfterConstructionFailureReusesDomOffsetAlgorithm()
{
    // 复用 malformedXmlReportsError 同一个探针输入（DOM 侧
    // TestDocument::setContentReportsErrorOnMalformedXml 已验证
    // offset=11 → line=1 col=12，见 pk/xml/README.md §2.3）——Stream 侧构造期
    // 失败复用同一套换算算法（PkXmlOffsetUtil.h），必须得到一致的数值。
    PkXmlStreamReader xml(PkString("<root><a></root>"));
    PK_VERIFY(xml.hasError());
    PK_COMPARE(xml.lineNumber(), 1);
    PK_COMPARE(xml.columnNumber(), 12);
}

void TestStreamReader::lineNumberColumnNumberAfterReadNextStartElementMatchesProbeP16()
{
    // 探针 P16 第二组：复刻 KoColorSet.cpp loadXml() 的确切场景——
    // readNextStartElement() 找到根元素名后立即查 line/col。输入
    // `<UNKNOWNROOT attr='1'>...`，22 恰好是 `<UNKNOWNROOT attr='1'>` 的
    // 字符数，Qt 实测 line=1 col=22（落在开始标签闭合的 '>' 上）。
    PkXmlStreamReader xml(PkString("<UNKNOWNROOT attr='1'></UNKNOWNROOT>"));
    PK_VERIFY(xml.readNextStartElement());
    PK_COMPARE(xml.name(), PkString("UNKNOWNROOT"));
    PK_COMPARE(xml.lineNumber(), 1);
    PK_COMPARE(xml.columnNumber(), 22);
}

void TestStreamReader::lineNumberColumnNumberUnaffectedByRaiseError()
{
    // 探针 P16：raiseError() 不改变 line/col——它只翻转
    // hasError()/atEnd()/tokenType()（见 raiseError() 实现），不触碰内部
    // 游标栈，line/col 依旧反映报错前最后一次成功 readNext() 的位置。
    PkXmlStreamReader xml(PkString("<root a=\"1\"><child/></root>"));
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartDocument));
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartElement)); // root, a="1"
    PK_COMPARE(xml.lineNumber(), 1);
    PK_COMPARE(xml.columnNumber(), 12);

    xml.raiseError("boom");
    PK_VERIFY(xml.hasError());
    PK_COMPARE(xml.lineNumber(), 1);
    PK_COMPARE(xml.columnNumber(), 12);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_stream_reader.inc"

int run_stream_reader_tests(int argc, char **argv)
{
    TestStreamReader tc;
    return PkTest::qExec(&tc, argc, argv);
}
