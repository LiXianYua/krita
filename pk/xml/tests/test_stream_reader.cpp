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
    PkXmlStreamReader xml(PkString("<root/>"));
    PK_VERIFY(!xml.hasError());
    PK_COMPARE(static_cast<int>(xml.readNext()),
               static_cast<int>(PkXmlStreamReader::StartDocument));
    PK_VERIFY(!xml.atEnd());

    xml.raiseError("boom");
    PK_VERIFY(xml.hasError());
    PK_COMPARE(xml.errorString(), PkString("boom"));
    PK_VERIFY(xml.atEnd());
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_stream_reader.inc"

int run_stream_reader_tests(int argc, char **argv)
{
    TestStreamReader tc;
    return PkTest::qExec(&tc, argc, argv);
}
