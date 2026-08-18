#include "test_stream_writer.h"

#include "../PkXmlStreamWriter.h"

void TestStreamWriter::compactOutputMatchesProbeP10()
{
    // 探针 P10：无 autoFormatting——紧凑单行，结尾恰好一个换行。逐字节抄。
    PkString out;
    PkXmlStreamWriter w(&out);
    w.writeStartDocument();
    w.writeStartElement("root");
    w.writeAttribute("a", "1");
    w.writeStartElement("child");
    w.writeCharacters("hi");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();

    PK_COMPARE(out,
               PkString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                         "<root a=\"1\"><child>hi</child></root>\n"));
}

void TestStreamWriter::autoFormattingOutputMatchesProbeP10()
{
    // 探针 P10：autoFormatting(true)——默认缩进 4 空格；只含文本的元素起止
    // 标签仍在同一行，含子元素的元素起止标签各占一行。逐字节抄。
    PkString out;
    PkXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    PK_VERIFY(w.autoFormatting());
    w.writeStartDocument();
    w.writeStartElement("root");
    w.writeStartElement("child");
    w.writeCharacters("hi");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();

    PK_COMPARE(out,
               PkString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         "<root>\n"
                         "    <child>hi</child>\n"
                         "</root>\n"));
}

void TestStreamWriter::emptyElementHasNoSelfClosingTag()
{
    // Qt 默认不会把没有子内容的元素自折叠成 `<tag/>`（那是 writeEmptyElement()
    // 的行为，真实调用点用量表实测 0 处，不实现）——writeStartElement 后立刻
    // writeEndElement 要得到 `<tag></tag>`。
    PkString out;
    PkXmlStreamWriter w(&out);
    w.writeStartDocument();
    w.writeStartElement("root");
    w.writeEndElement();
    w.writeEndDocument();

    PK_COMPARE(out,
               PkString("<?xml version=\"1.0\" encoding=\"UTF-8\"?><root></root>\n"));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_stream_writer.inc"

int run_stream_writer_tests(int argc, char **argv)
{
    TestStreamWriter tc;
    return PkTest::qExec(&tc, argc, argv);
}
