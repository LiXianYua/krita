/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDebug>

#include "kis_cos_writer.h"

#include "PkCosMemoryStream.h"
#include <PkMessageLogger.h>
#include <PkStringList.h>

#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

// 对应原 Qt 的 escape 查找表 —— 按字节查表。Adobe text engine data
// 实际只转义 ( ) 和 \（原文件里 0x0a/0x0d/0x09/0x08/0x0c 那几行被注释掉了）。
static int writerEscapedChar(char c) {
    switch (c) {
    case 0x28: return '(';
    case 0x29: return ')';
    case 0x5c: return '\\'; // reverse solidus/backslash
    default: return -1;
    }
}

// 因为区分不了 name 与普通字符串，这里列出「值也是 name」的 key。
static PkStringList nameKeys {
    "/StreamTag",
    "/ListStyle",
    "/MojiKumiTable",
    "/Kinsoku"
};

// UTF-16BE 编码：对齐 Qt 的 UTF-16BE 编码器 fromUnicode()
// 的实测行为——输出前缀 FE FF BOM，然后每个码元按大端两字节（parser 端的
// pkUtf16beDecode 会剥掉这个 BOM，往返自洽）。
static PkByteArray pkUtf16beEncode(const std::u16string &u16) {
    PkByteArray out;
    out.resize(2 + static_cast<int>(u16.size()) * 2);
    char *p = out.data();
    p[0] = static_cast<char>(0xFE);
    p[1] = static_cast<char>(0xFF);
    int i = 2;
    for (char16_t ch : u16) {
        p[i++] = static_cast<char>((ch >> 8) & 0xFF);
        p[i++] = static_cast<char>(ch & 0xFF);
    }
    return out;
}

// LF → CR：对齐原 newString.replace(换行字符 0x0a, 回车字符 0x0d)。PkString 无
// replace，用 u16string 逐码元替换。
static PkString pkStringReplaceLFtoCR(const PkString &s) {
    const std::u16string u16 = s.PkToU16();
    std::u16string out;
    out.reserve(u16.size());
    for (char16_t ch : u16) {
        out.push_back(ch == 0x0a ? 0x0d : ch);
    }
    return PkVariant::PkFromStringCodeUnits(out).toString();
}

// double → 定点 5 位小数：对齐 Qt 数字转字符串 number(v, 'f', 5)（number
// 用 C locale，这里 imbufe classic 对齐）。
static std::string pkFormatDouble(double v) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::fixed << std::setprecision(5) << v;
    return os.str();
}

void writeString(PkStream &dev, const PkVariant val, const PkString name) {
    PkString newString = val.toString();
    if (nameKeys.contains(name)) {
        const std::string s = (name + PkString(" ") + newString).PkToUtf8();
        dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
    } else {
        newString = pkStringReplaceLFtoCR(newString);
        const std::string nameAndOpen = (name + PkString(" (")).PkToUtf8();
        dev.write(nameAndOpen.data(), static_cast<PkStream::pk_int64>(nameAndOpen.size()));
        const PkByteArray unicode = pkUtf16beEncode(newString.PkToU16());
        std::string escaped;
        escaped.reserve(static_cast<size_t>(unicode.size()));
        const char *c = unicode.constData();
        for (int i = 0; i < unicode.size(); i++) {
            const int esc = writerEscapedChar(c[i]);
            if (esc >= 0) {
                escaped.push_back('\\');
                escaped.push_back(static_cast<char>(esc));
            } else {
                escaped.push_back(c[i]);
            }
        }
        if (!escaped.empty()) {
            dev.write(escaped.data(), static_cast<PkStream::pk_int64>(escaped.size()));
        }
        dev.write(")", 1);
    }
}

/**
 * @brief writeVariant
 * @param prettyPrint -- whether to allow newlines and indentation or just a single space between values.
 * @param writeBrackets -- for some reason PSD stores the txt2 section without the outermost << and >>...
 */
void writeVariant(PkStream &dev, const PkVariantHash object, int indent, bool prettyPrint, bool writeBrackets = true) {
    const std::string indentStringOld(static_cast<size_t>(indent), '\t');
    const std::string newLine = prettyPrint ? "\n" : " ";
    dev.write(indentStringOld.data(), static_cast<PkStream::pk_int64>(indentStringOld.size()));
    if (writeBrackets) {
        const std::string s = "<<" + newLine;
        dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
    }
    indent = prettyPrint ? indent + 1 : 0;
    const std::string indentString(static_cast<size_t>(indent), '\t');
    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        const std::string name = key.PkToUtf8();
        if (val.type() == PkVariant::Hash) {
            dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
            const std::string s = name + newLine;
            dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
            writeVariant(dev, val.toHash(), indent, prettyPrint);
        } else if (val.type() == PkVariant::List) {
            const PkVariantList array = val.toList();
            if (array.empty()) {
                dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
                const std::string s = name + " [ ]" + newLine;
                dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
            } else if (array.at(0).type() == PkVariant::Int) {
                dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
                const std::string s = name + " [";
                dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
                for (size_t i = 0; i < array.size(); i++) {
                    const std::string num = " " + std::to_string(array.at(static_cast<int>(i)).toInt());
                    dev.write(num.data(), static_cast<PkStream::pk_int64>(num.size()));
                }
                const std::string s2 = " ]" + newLine;
                dev.write(s2.data(), static_cast<PkStream::pk_int64>(s2.size()));
            } else if (array.at(0).type() == PkVariant::Double) {
                dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
                const std::string s = name + " [";
                dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
                for (size_t i = 0; i < array.size(); i++) {
                    const std::string num = " " + pkFormatDouble(array.at(static_cast<int>(i)).toDouble());
                    dev.write(num.data(), static_cast<PkStream::pk_int64>(num.size()));
                }
                const std::string s2 = " ]" + newLine;
                dev.write(s2.data(), static_cast<PkStream::pk_int64>(s2.size()));
            } else {
                dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
                const std::string s = name + " [" + newLine;
                dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
                for (size_t i = 0; i < val.toList().size(); i++) {
                    const PkVariant arrVal = val.toList().at(static_cast<int>(i));
                    if (arrVal.type() == PkVariant::Hash) {
                        writeVariant(dev, arrVal.toHash(), indent, prettyPrint);
                    }
                }
                dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
                const std::string s2 = "]" + newLine;
                dev.write(s2.data(), static_cast<PkStream::pk_int64>(s2.size()));
            }
        } else if (val.type() == PkVariant::String) {

            dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
            writeString(dev, val, key);
            const std::string s = newLine;
            dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
        } else if (val.type() == PkVariant::Bool) {
            const std::string boolVal = val.toBool() ? "true" : "false";
            dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
            const std::string s = name + " " + boolVal + newLine;
            dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
        } else {
            if (val.type() == PkVariant::Double) {
                dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
                const std::string s = name + " " + pkFormatDouble(val.toDouble()) + newLine;
                dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
            } else if (val.type() == PkVariant::Int) {
                dev.write(indentString.data(), static_cast<PkStream::pk_int64>(indentString.size()));
                const std::string s = name + " " + std::to_string(val.toInt()) + newLine;
                dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
            }
        }
    }
    dev.write(indentStringOld.data(), static_cast<PkStream::pk_int64>(indentStringOld.size()));
    if (writeBrackets) {
        const std::string s = ">>" + newLine;
        dev.write(s.data(), static_cast<PkStream::pk_int64>(s.size()));
    }
}

PkByteArray KisCosWriter::writeCosFromVariantHash(const PkVariantHash doc)
{
    PkByteArray ba;
    PkCosMemoryStream dev(&ba);
    if (dev.open(PkStream::WriteOnly)) {
        int indent = 0;
        dev.write("\n\n", 2);
        bool prettyPrint = true;
        writeVariant(dev, doc, indent, prettyPrint);
        dev.close();
    } else {
        qWarning() << dev.errorString().PkToUtf8().c_str();
    }
    return ba;
}

PkByteArray KisCosWriter::writeTxt2FromVariantHash(const PkVariantHash doc)
{
    PkByteArray ba;
    PkCosMemoryStream dev(&ba);
    if (dev.open(PkStream::WriteOnly)) {
        dev.write(" ", 1);
        writeVariant(dev, doc, 0, false, false);
        dev.close();
    } else {
        qWarning() << dev.errorString().PkToUtf8().c_str();
    }
    if (ba.size() > 0 && ba.constData()[ba.size() - 1] == ' ') {
        ba.resize(ba.size() - 1);
    }
    return ba;
}
