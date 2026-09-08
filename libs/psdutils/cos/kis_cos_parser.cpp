/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_cos_parser.h"

#include "PkCosMemoryStream.h"
#include <PkMessageLogger.h>

#include <string>

enum {
    Null = 0x00,
    Space = 0x20,
    Tab = 0x09,
    LineFeed = 0x0a,
    FormFeed = 0x0c,
    Return = 0x0d,
    BeginArray = 0x5b, // [
    BeginObject = 0x3c, // <
    EndArray = 0x5d, // ]
    EndObject = 0x3e, // >
    BeginName = 0x2f, // /
    BeginString = 0x28, // (
    EndString = 0x29, // )
    ByteOrderMark = 0xfe
};

bool isWhiteSpace(char c) {
    switch (c) {
    case Null:
    case Tab:
    case LineFeed:
    case FormFeed:
    case Return:
    case Space:
        return true;
    default:
        return false;
    }
}

// 逐字符转成 UTF-16 码元（Latin-1：码元值 = 字节值），用 PkVariant 的
// PkFromStringCodeUnits 建 PkString。等价于 Qt 字符串的 fromLatin1。
static PkString pkFromLatin1(const std::string &s) {
    std::u16string u16;
    u16.reserve(s.size());
    for (char ch : s) {
        u16.push_back(static_cast<char16_t>(static_cast<unsigned char>(ch)));
    }
    return PkVariant::PkFromStringCodeUnits(u16).toString();
}

// UTF-16BE 解码：Qt 文本编码的 UTF-16BE 解码器会剥掉
// 开头的 FE FF BOM，这里对齐（前两字节是 FE FF 则跳过）。
static std::u16string pkUtf16beDecode(const char *bytes, int len) {
    std::u16string out;
    int i = 0;
    if (len >= 2
        && static_cast<unsigned char>(bytes[0]) == 0xFE
        && static_cast<unsigned char>(bytes[1]) == 0xFF) {
        i = 2;
    }
    for (; i + 1 < len; i += 2) {
        out.push_back(static_cast<char16_t>(
            (static_cast<unsigned char>(bytes[i]) << 8)
            | static_cast<unsigned char>(bytes[i + 1])));
    }
    return out;
}

// PDF 的 \ddd 八进制：至少一位有效八进制数字(0-7)才算 ok（strtoll base-8 语义）。
static int pkOctalToInt(const std::string &s, bool &ok) {
    long long val = 0;
    bool any = false;
    for (char ch : s) {
        if (ch >= '0' && ch <= '7') {
            val = val * 8 + (ch - '0');
            any = true;
        } else {
            break;
        }
    }
    ok = any;
    return static_cast<int>(val);
}

void eatSpace(PkStream &dev) {
    char c = 0;
    dev.peek(&c, 1);
    while (isWhiteSpace(c) && !dev.atEnd()) {
        dev.skip(1);
        dev.peek(&c, 1);
    }
}

bool parseName(PkStream &dev, PkVariant &val) {
    char c;
    dev.getChar(&c);
    std::string name = "/"; // prepending with / so that we know this is a name.

    while (c >= 0x21 && c<0x7e && !isWhiteSpace(c) && c != BeginName && !dev.atEnd()) {
        name.push_back(c);
        dev.getChar(&c);
    }

    if (c == BeginName) {
        dev.ungetChar(c);
    }
    val = PkVariant(pkFromLatin1(name));
    return true;
}

// 对应原 Qt 的 escaped 查找表 —— 用查找表函数代替。
static int parserEscapedChar(char c) {
    switch (c) {
    case 'n': return LineFeed;
    case 'r': return Return;
    case 't': return Tab;
    case 'b': return 0x08; // backspace
    case 'f': return FormFeed;
    case '(': return BeginString;
    case ')': return EndString;
    case '\\': return 0x5c; // reverse solidus/backslash
    default: return -1;
    }
}

bool parseString(PkStream &dev, PkVariant &val) {
    char c;
    dev.getChar(&c);
    std::string text;

    while(c != EndString && !dev.atEnd()) {
        if (c == '\\') {

            // Try to parse PDF \ddd notation.
            char c2;
            dev.peek(&c2, 1);

            const int esc = parserEscapedChar(c2);
            if (esc >= 0) {
                text.push_back(static_cast<char>(esc));
                dev.skip(1);
            } else {
                std::string octal;
                for (int i=0; i<3; i++) {
                    char c3;
                    dev.peek(&c3, 1);
                    if (c3 >= '0' && c3 <= '9') {
                        octal.push_back(c3);
                        dev.skip(1);
                    }
                }
                bool ok = false;
                const int val2 = pkOctalToInt(octal, ok);
                if (ok) {
                    qWarning() << "escaped octal" << val2;
                    // don't know how to actually interpret this as a char...
                } else {
                    text.push_back(c);
                    text += octal;
                }
            }
        } else {
            text.push_back(c);
        }
        dev.getChar(&c);
    }


    if (text.size() >= 1 && static_cast<unsigned char>(text[0]) == ByteOrderMark) {
        val = PkVariant::PkFromStringCodeUnits(pkUtf16beDecode(text.data(), static_cast<int>(text.size())));
    } else {
        val = PkVariant(pkFromLatin1(text));
    }

    return true;
}

bool parseHexString(PkStream &dev, PkVariant &val) {
    char c;
    dev.getChar(&c);
    std::string hex;

    while (c!= EndObject && !dev.atEnd()) {
        hex.push_back(c);
        dev.getChar(&c);
    }

    val = PkVariant(PkString("<") + pkFromLatin1(hex) + PkString(">"));
    return true;
}

bool parseNumber(PkStream &dev, PkVariant &val) {
    char c;
    bool isDouble = false;
    dev.getChar(&c);
    std::string number;
    if (c == '-' || c == '+') {
        number.push_back(c);
        dev.getChar(&c);
    }
    while (c >= '0' && c <= '9') {
        number.push_back(c);
        dev.getChar(&c);
    }
    if (c == '.') {
        isDouble = true;
        number.push_back(c);
        dev.getChar(&c);
        while (c >= '0' && c <= '9') {
            number.push_back(c);
            dev.getChar(&c);
        }
    }

    bool ok;
    if (isDouble) {
        val = PkVariant(pkFromLatin1(number).toDouble(&ok));
    } else {
        val = PkVariant(pkFromLatin1(number).toInt(&ok));
    }
    return ok;
}

bool KisCosParser::parseObject(PkStream &dev, PkVariantHash &object, bool checkEnd) {
    eatSpace(dev);

    PkVariant key;
    PkVariant val;
    while (parseValue(dev, key)) {
        const PkString keyStr = key.toString();
        object[keyStr] = PkVariant();
        if (key.type() == PkVariant::String && parseValue(dev, val)) {
            object[keyStr] = val;
        } else {
            return false;
        }
    }
    char c;
    dev.getChar(&c);
    if (c == EndObject) {
        dev.skip(1);
        return true;
    } else if (checkEnd) {
        return false;
    }

    return true;
}

bool KisCosParser::parseArray(PkStream &dev, PkVariantList &array)
{
    eatSpace(dev);

    PkVariant val;
    while (parseValue(dev, val)) {
        array.push_back(val);
    }
    char c;
    dev.getChar(&c);
    if (c == EndArray) {
        return true;
    } else {
        return false;
    }

    return true;
}

bool KisCosParser::parseValue(PkStream &dev, PkVariant &val) {

    eatSpace(dev);
    char c;
    dev.getChar(&c);

    if (c == BeginObject) {
        char c2;
        dev.peek(&c2, 1);
        if (c2 == BeginObject) {
            PkVariantHash object = PkVariantHash();
            dev.skip(1);
            if (!parseObject(dev, object)) {
                return false;
            }
            val = object;
        } else {
            if (!parseHexString(dev, val)) {
                return false;
            }
        }
    } else if (c == BeginArray) {
        PkVariantList array = PkVariantList();
        if (!parseArray(dev, array)) {
            return false;
        }
        val = array;
    } else if (c == BeginName) {
        if (!parseName(dev, val)) {
            return false;
        }
    } else if (c == BeginString) {
        if (!parseString(dev, val)) {
            return false;
        }
    } else if (c == EndObject || c == EndArray) {
        dev.ungetChar(c);
        return false;
    } else if (c == 't') {
        char tbuf[3] = {0, 0, 0};
        const PkStream::pk_int64 n = dev.read(tbuf, 3);
        if (n == 3 && tbuf[0] == 'r' && tbuf[1] == 'u' && tbuf[2] == 'e') {
            val = true;
        } else {
            return false;
        }

    } else if (c == 'f') {
        char tbuf[4] = {0, 0, 0, 0};
        const PkStream::pk_int64 n = dev.read(tbuf, 4);
        if (n == 4 && tbuf[0] == 'a' && tbuf[1] == 'l' && tbuf[2] == 's' && tbuf[3] == 'e') {
            val = false;
        } else {
            return false;
        }
    } else if (c == 'n') {
        char tbuf[3] = {0, 0, 0};
        const PkStream::pk_int64 n = dev.read(tbuf, 3);
        if (n == 3 && tbuf[0] == 'u' && tbuf[1] == 'l' && tbuf[2] == 'l') {
            val = PkVariant();
        } else {
            return false;
        }
    } else {
        dev.ungetChar(c);
        if (!parseNumber(dev, val)) {
            return false;
        }
    }

    return true;
}

PkVariantHash KisCosParser::parseCosToJson(PkByteArray *ba)
{
    PkVariant root;
    PkCosMemoryStream dev(ba);
    if (dev.open(PkStream::ReadOnly)) {

        eatSpace(dev);
        char c = 0;
        dev.peek(&c, 1);
        if (c == BeginObject) {
            if (!parseValue(dev, root)) {
                qWarning() << "dev not at end";
            }
        } else {
            PkVariantHash b;
            if (!parseObject(dev, b, false)) {
                qWarning() << "txt2 dev not at end";
            }
            root = b;
        }
        dev.close();
    }
    return root.toHash();
}
