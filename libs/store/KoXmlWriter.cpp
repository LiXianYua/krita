/* This file is part of the KDE projectz
   SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>
   SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoXmlWriter.h"

#include <StoreDebug.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static const int s_indentBufferLength = 100;
static const int s_escapeBufferLen = 10000;

// Qt 版的原实现调用 KisDomUtils::toString（流式输出的 realNumberPrecision）。
// S-01 剥 Qt 后本文件内自备等价的 printf 族格式化（语义对齐见任务报告的数字
// 格式化核对）：double 用 %.15g（= realNumberPrecision(15)，DBL_DIG），float 用
// %.6g（= realNumberPrecision(FLT_DIG)，FLT_DIG 实测=6）。
static void appendDouble(char* buf, size_t n, double v)
{
    std::snprintf(buf, n, "%.15g", v);
}

static void appendFloat(char* buf, size_t n, float v)
{
    std::snprintf(buf, n, "%.6g", v);
}

class KoXmlWriter::Private
{
public:
    Private(PkStream* dev_, int indentLevel = 0)
        : dev(dev_)
        , baseIndentLevel(indentLevel)
    {}

    ~Private() {
        delete[] indentBuffer;
        delete[] escapeBuffer;
        //TODO: look at if we must delete "dev". For me we must delete it otherwise we will leak it
    }

    PkStream* dev;
    PkStack<Tag> tags;
    int baseIndentLevel;

    char* indentBuffer; // maybe make it static, but then it needs a K_GLOBAL_STATIC
    // and would eat 1K all the time... Maybe refcount it :)
    char* escapeBuffer; // can't really be static if we want to be thread-safe
};

KoXmlWriter::KoXmlWriter(PkStream* dev, int indentLevel)
        : d(new Private(dev, indentLevel))
{
    d->indentBuffer = new char[ s_indentBufferLength ];
    memset(d->indentBuffer, ' ', s_indentBufferLength);
    *d->indentBuffer = '\n'; // write newline before indentation, in one go

    d->escapeBuffer = new char[s_escapeBufferLen];
    if (!d->dev->isOpen())
        d->dev->open(PkStream::WriteOnly);

}

KoXmlWriter::~KoXmlWriter()
{
    delete d;
}

void KoXmlWriter::startDocument(const char* rootElemName, const char* publicId, const char* systemId)
{
    assert(d->tags.isEmpty());
    writeCString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    // There isn't much point in a doctype if there's no DTD to refer to
    // (I'm told that files that are validated by a RelaxNG schema cannot refer to the schema)
    if (publicId) {
        writeCString("<!DOCTYPE ");
        writeCString(rootElemName);
        writeCString(" PUBLIC \"");
        writeCString(publicId);
        writeCString("\" \"");
        writeCString(systemId);
        writeCString("\"");
        writeCString(">\n");
    }
}

void KoXmlWriter::endDocument()
{
    // newline at end of file, like a DOM writer does.
    writeChar('\n');
    assert(d->tags.isEmpty());
}

// returns the value of indentInside of the parent
bool KoXmlWriter::prepareForChild(bool indentInside)
{
    if (!d->tags.isEmpty()) {
        Tag& parent = d->tags.top();
        if (!parent.hasChildren) {
            closeStartElement(parent);
            parent.hasChildren = true;
            parent.lastChildIsText = false;
        }
        if (parent.indentInside && indentInside) {
            writeIndent();
        }
        return parent.indentInside && indentInside;
    }
    return indentInside;
}

void KoXmlWriter::prepareForTextNode()
{
    if (d->tags.isEmpty())
        return;
    Tag& parent = d->tags.top();
    if (!parent.hasChildren) {
        closeStartElement(parent);
        parent.hasChildren = true;
        parent.lastChildIsText = true;
    }
}

void KoXmlWriter::startElement(const char* tagName, bool indentInside)
{
    assert(tagName != 0);

    // Tell parent that it has children
    indentInside = prepareForChild(indentInside);

    d->tags.push(Tag(tagName, indentInside));
    writeChar('<');
    writeCString(tagName);
    //kDebug(s_area) << tagName;
}

void KoXmlWriter::addCompleteElement(PkStream* indev)
{
    prepareForChild();
    const bool wasOpen = indev->isOpen();
    // Always (re)open the device in readonly mode, it might be
    // already open but for writing, and we need to rewind.
    const bool openOk = indev->open(PkStream::ReadOnly);
    assert(openOk);
    if (!openOk) {
        warnStore << "Failed to re-open the device! wasOpen=" << wasOpen;
        return;
    }

    // PkString 没有 fill()：用 std::string 定长构造替代原
    // indentString.fill(' ', n) 写法。
    std::string indentBuf(d->tags.size() + d->baseIndentLevel, ' ');

    // PkStream 无参 readLine() 只声明不定义：用已定义的
    // readLine(char*, pk_int64) 逐行读（行内含 '\n'，与 Qt 的 readLine
    // 语义一致）。
    char buf[8192];
    PkStream::pk_int64 n = 0;
    while (!indev->atEnd()) {
        n = indev->readLine(buf, sizeof(buf));
        if (n > 0) {
            d->dev->write(indentBuf.data(), (PkStream::pk_int64)indentBuf.size());
            d->dev->write(buf, n);
        }
    }

    if (!wasOpen) {
        // Restore initial state
        indev->close();
    }
}

void KoXmlWriter::endElement()
{
    if (d->tags.isEmpty())
        warnStore << "EndElement() was called more times than startElement(). "
                     "The generated XML will be invalid! "
                     "Please report this bug (by saving the document to another format...)";

    Tag tag = d->tags.pop();

    if (!tag.hasChildren) {
        writeCString("/>");
    } else {
        if (tag.indentInside && !tag.lastChildIsText) {
            writeIndent();
        }
        writeCString("</");
        assert(tag.tagName != 0);
        writeCString(tag.tagName);
        writeChar('>');
    }
}

void KoXmlWriter::addTextNode(const PkByteArray& cstr)
{
    // Same as the const char* version below, but here we know the size
    prepareForTextNode();
    char* escaped = escapeForXML(reinterpret_cast<const char*>(cstr.data()), cstr.size());
    writeCString(escaped);
    if (escaped != d->escapeBuffer)
        delete[] escaped;
}

void KoXmlWriter::addTextNode(const char* cstr)
{
    prepareForTextNode();
    char* escaped = escapeForXML(cstr, -1);
    writeCString(escaped);
    if (escaped != d->escapeBuffer)
        delete[] escaped;
}

void KoXmlWriter::addAttribute(const char* attrName, const PkByteArray& value)
{
    // Same as the const char* one, but here we know the size
    writeChar(' ');
    writeCString(attrName);
    writeCString("=\"");
    char* escaped = escapeForXML(reinterpret_cast<const char*>(value.data()), value.size());
    writeCString(escaped);
    if (escaped != d->escapeBuffer)
        delete[] escaped;
    writeChar('"');
}

void KoXmlWriter::addAttribute(const char* attrName, const char* value)
{
    writeChar(' ');
    writeCString(attrName);
    writeCString("=\"");
    char* escaped = escapeForXML(value, -1);
    writeCString(escaped);
    if (escaped != d->escapeBuffer)
        delete[] escaped;
    writeChar('"');
}

void KoXmlWriter::addAttribute(const char* attrName, double value)
{
    char buf[64];
    appendDouble(buf, sizeof(buf), value);
    addAttribute(attrName, buf);
}

void KoXmlWriter::addAttribute(const char* attrName, float value)
{
    char buf[64];
    appendFloat(buf, sizeof(buf), value);
    addAttribute(attrName, buf);
}

void KoXmlWriter::writeIndent()
{
    // +1 because of the leading '\n'
    d->dev->write(d->indentBuffer, std::min(d->tags.size() + d->baseIndentLevel + 1,
                                            s_indentBufferLength));
}


// In case of a reallocation (ret value != d->buffer), the caller owns the return value,
// it must delete it (with [])
char* KoXmlWriter::escapeForXML(const char* source, int length = -1) const
{
    // we're going to be pessimistic on char length; so lets make the outputLength less
    // the amount one char can take: 6
    char* destBoundary = d->escapeBuffer + s_escapeBufferLen - 6;
    char* destination = d->escapeBuffer;
    char* output = d->escapeBuffer;
    const char* src = source; // src moves, source remains
    for (;;) {
        if (destination >= destBoundary) {
            // When we come to realize that our escaped string is going to
            // be bigger than the escape buffer (this shouldn't happen very often...),
            // we drop the idea of using it, and we allocate a bigger buffer.
            // Note that this if() can only be hit once per call to the method.
            if (length == -1)
                length = strlen(source);   // expensive...
            unsigned int newLength = length * 6 + 1; // worst case. 6 is due to &quot; and &apos;
            char* buffer = new char[ newLength ];
            destBoundary = buffer + newLength;
            unsigned int amountOfCharsAlreadyCopied = destination - d->escapeBuffer;
            memcpy(buffer, d->escapeBuffer, amountOfCharsAlreadyCopied);
            output = buffer;
            destination = buffer + amountOfCharsAlreadyCopied;
        }
        switch (*src) {
        case 60: // <
            memcpy(destination, "&lt;", 4);
            destination += 4;
            break;
        case 62: // >
            memcpy(destination, "&gt;", 4);
            destination += 4;
            break;
        case 34: // "
            memcpy(destination, "&quot;", 6);
            destination += 6;
            break;
#if 0 // needed?
        case 39: // '
            memcpy(destination, "&apos;", 6);
            destination += 6;
            break;
#endif
        case 38: // &
            memcpy(destination, "&amp;", 5);
            destination += 5;
            break;
        case 0:
            *destination = '\0';
            return output;
        // Control codes accepted in XML 1.0 documents.
        case 9:
        case 10:
        case 13:
            *destination++ = *src++;
            continue;
        default:
            // Don't add control codes not accepted in XML 1.0 documents.
            if (*src > 0 && *src < 32) {
                ++src;
            } else {
                *destination++ = *src++;
            }
            continue;
        }
        ++src;
    }
    // NOTREACHED (see case 0)
    return output;
}

void KoXmlWriter::addManifestEntry(const PkString& fullPath, const PkString& mediaType)
{
    startElement("manifest:file-entry");
    addAttribute("manifest:media-type", mediaType);
    addAttribute("manifest:full-path", fullPath);
    endElement();
}

// TODO check return value!!!
void KoXmlWriter::writeCString(const char* cstr) {
    d->dev->write(cstr, strlen(cstr));
}

// TODO check return value!!!
void KoXmlWriter::writeChar(char c) {
    d->dev->putChar(c);
}
