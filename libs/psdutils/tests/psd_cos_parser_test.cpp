/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "psd_cos_parser_test.h"
#include "cos/kis_cos_parser.h"
#include "cos/kis_cos_writer.h"

#include <PkMessageLogger.h>

#include <cstring>

// 字节数组(const char*) 的 Pk 对应：PkByteArray 构造需要长度，这里按 strlen 包一层。
static PkByteArray pba(const char *s)
{
    return PkByteArray(s, static_cast<int>(std::strlen(s)));
}

void psd_cos_parser_test::test_roundtrip_cos_struct_data()
{
    PkTest::addColumn<PkVariantHash>("doc");

    PkTest::addRow("empty") << PkVariantHash();
    PkTest::addRow("map") << PkVariantHash({{PkString("/Map"), PkVariantHash({{PkString("/A"), "a"}, {PkString("/B"), "b"}})}});
    PkTest::addRow("array") << PkVariantHash({{PkString("/List"), PkVariantList({1, 2, 3, 4})}});
    PkTest::addRow("string") << PkVariantHash({{PkString("/A"), "the (quick) \r brown fox"}, {PkString("/B"), "jumps over \rthe la\\zy dog."}});
    PkTest::addRow("int") << PkVariantHash({{PkString("/A"), 2}, {PkString("/B"), 12}});
    PkTest::addRow("double") << PkVariantHash({{PkString("/A"), 0.5}, {PkString("/B"), 0.001}});
    PkTest::addRow("bool") << PkVariantHash({{PkString("/A"), true}, {PkString("/B"), false}});
}

void psd_cos_parser_test::test_roundtrip_cos_struct()
{
    PK_FETCH(PkVariantHash, doc);
    KisCosWriter w;
    PkByteArray data = w.writeCosFromVariantHash(doc);

    KisCosParser p;
    PkVariantHash newDoc = p.parseCosToJson(&data);
    if (doc != newDoc) {
        qDebug() << doc;
        qDebug() << newDoc;
    }
    PK_VERIFY2(doc == newDoc, "Roundtrip failed");
}

void psd_cos_parser_test::test_parse_cos_struct_data()
{
    PkTest::addColumn<PkByteArray>("data");
    PkTest::addColumn<PkVariantHash>("reference");

    PkTest::addRow("empty") << pba("<<>>") << PkVariantHash();
    PkTest::addRow("bool") << pba("<< /Ligatures true /DLigatures false >>")
                                      << PkVariantHash({{PkString("/Ligatures"), true}, {PkString("/DLigatures"), false}});
    PkTest::addRow("null") << pba("<< /StyleSheetData null /Ligatures true >>")
                                      << PkVariantHash({{PkString("/StyleSheetData"), PkVariant()}, {PkString("/Ligatures"), true}});
    PkTest::addRow("int") << pba("<< /FigureStyle 0 /PreHyphen 2 >>")
                                      << PkVariantHash({{PkString("/FigureStyle"), 0}, {PkString("/PreHyphen"), 2}});
    PkTest::addRow("string") << pba("\n\n<<\n\t/CloseDoubleQuote (\xFE\xFF \x1D)\n>>\n")
                         << PkVariantHash({{PkString("/CloseDoubleQuote"), "”"}});
    PkTest::addRow("array of floats") << pba("<< /Zone 36.0 /WordSpacing [ .8 1.0 1.33 ] /After -.5 >>")
                                      << PkVariantHash({{PkString("/Zone"), 36.0}, {PkString("/WordSpacing"), PkVariantList({0.8, 1.0, 1.33})}, {PkString("/After"), -0.5}});
}

void psd_cos_parser_test::test_parse_cos_struct()
{
    PK_FETCH(PkByteArray, data);
    PK_FETCH(PkVariantHash, reference);

    KisCosParser p;
    PkVariantHash newDoc = p.parseCosToJson(&data);
    if (reference != newDoc) {
        qDebug() << reference;
        qDebug() << newDoc;
    }
    PK_VERIFY2(reference == newDoc, "Parsing failed");
}

#ifdef PK_SHELL_MOC_BINDER
#include "pk_binder_psd_cos_parser_test.inc"
#endif

PK_TEST_GUILESS_MAIN(psd_cos_parser_test)
