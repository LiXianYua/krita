/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisTag.h"

#include <PkGlobal.h>
#include <PkContainerAlgo.h>

#include <string>
#include <vector>

#include "ResourceDebug.h"
#include "kis_assert.h"

namespace {

// PkString 没有 toUpper/indexOf。tag 文件是 ASCII 结构（.desktop 键、URL、
// 语言码都是 ASCII），这里用 PkToUtf8() → std::string 中转处理，不再往
// pk/string 加方法（那是别的任务的地盘）。

bool pkEqualsAsciiCaseInsensitive(const PkString &a, const PkString &b)
{
    const std::string ua = a.PkToUtf8();
    const std::string ub = b.PkToUtf8();
    if (ua.size() != ub.size()) {
        return false;
    }
    for (std::size_t i = 0; i < ua.size(); ++i) {
        char ca = ua[i];
        char cb = ub[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

// 找 ASCII 字符 c 首次出现的下标。UTF-8 多字节序列的续字节恒 >= 0x80，
// 不会与 ASCII 字符冲突，逐字节 find 安全。
int pkIndexOfAscii(const PkString &s, char c)
{
    const std::string u = s.PkToUtf8();
    const std::size_t pos = u.find(c);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
}

// 原版 split(',', SkipEmptyParts) → PkString::split 保留空段
// （对齐默认 KeepEmptyParts），这里手工过滤掉空段。
PkStringList pkSplitSkipEmpty(const PkString &s, char16_t sep)
{
    const std::vector<PkString> parts = s.split(sep);
    PkStringList result;
    for (const PkString &p : parts) {
        if (!p.isEmpty()) {
            result.append(p);
        }
    }
    return result;
}

// 把一行写入流（追加 '\n'）。原版用文本流 operator<<，写失败不报错；
// 这里同样不检查 write() 返回值，保持「save 恒返回 true」的原契约。
void pkWriteLine(PkStream &io, const PkString &line)
{
    const std::string u = line.PkToUtf8();
    io.write(u.c_str(), static_cast<PkStream::pk_int64>(u.size()));
    io.write("\n", 1);
}

} // namespace

const PkString KisTag::s_group {"Desktop Entry"};
const PkString KisTag::s_type {"Type"};
const PkString KisTag::s_tag {"Tag"};
const PkString KisTag::s_name {"Name"};
const PkString KisTag::s_resourceType {"ResourceType"};
const PkString KisTag::s_url {"URL"};
const PkString KisTag::s_comment {"Comment"};
const PkString KisTag::s_defaultResources {"Default Resources"};
const PkString KisTag::s_desktop {"[Desktop Entry]"};

class KisTag::Private {
public:
    bool valid {false};
    PkString url; // This is the actual tag
    PkString name;
    PkString comment;
    PkMap<PkString, PkString> names; // The translated tag names
    PkMap<PkString, PkString> comments; // The translated tag comments
    PkStringList defaultResources; // The list of resources as defined in the tag file
    PkString resourceType; // The resource type this tag can be applied to
    PkString filename; // the original filename for the tag
    int id {-1};
    bool active{true};
};

KisTag::KisTag()
    : d(new Private)
{
}

KisTag::~KisTag()
{
}

KisTag::KisTag(const KisTag &rhs)
    : d(new Private)
{
    *this = rhs;
}

KisTag &KisTag::operator=(const KisTag &rhs)
{
    if (this != &rhs) {
        d->valid = rhs.d->valid;
        d->url = rhs.d->url;
        d->name = rhs.d->name;
        d->comment = rhs.d->comment;
        d->names = rhs.d->names;
        d->comments = rhs.d->comments;
        d->defaultResources = rhs.d->defaultResources;
        d->resourceType = rhs.d->resourceType;
        d->filename = rhs.d->filename;
        d->id = rhs.d->id;
        d->active = rhs.d->active;
    }
    return *this;
}

KisTagSP KisTag::clone() const
{
    return KisTagSP(new KisTag(*this));
}

PkString KisTag::currentLocale()
{
    // 本地化横切项已移交，语言探测逻辑删除，
    // 硬编码 en_US（Task 8 的 Model 层消费方会照此适配）。
    return PkString("en_US");
}

bool KisTag::valid() const
{
    return d->valid;
}

int KisTag::id() const
{
    return d->id;
}

bool KisTag::active() const
{
    return d->active;
}

PkString KisTag::filename()
{
    return d->filename;
}

void KisTag::setFilename(const PkString &filename)
{
    d->filename = filename;
}

PkString KisTag::name(bool translated) const
{
    if (translated && d->names.contains(currentLocale())) {
        return d->names[currentLocale()];
    }
    Q_ASSERT(!d->name.isEmpty());
    return d->name;
}

void KisTag::setName(const PkString &name)
{
    d->name = name;
}

PkMap<PkString, PkString> KisTag::names() const
{
    return d->names;
}

void KisTag::setNames(const PkMap<PkString, PkString> &names)
{
    d->names = names;
}

PkString KisTag::comment(bool translated) const
{
    if (translated && d->comments.contains(currentLocale())) {
        return d->comments[currentLocale()];
    }
    return d->comment;
}

void KisTag::setComment(const PkString comment)
{
    d->comment = comment;
}

PkString KisTag::url() const
{
    return d->url;
}

void KisTag::setUrl(const PkString &url)
{
    d->url = url;
}


PkMap<PkString, PkString> KisTag::comments() const
{
    return d->comments;
}

void KisTag::setComments(const PkMap<PkString, PkString> &comments)
{
    d->comments = comments;
}

PkString KisTag::resourceType() const
{
    return d->resourceType;
}

void KisTag::setResourceType(const PkString &resourceType)
{
    d->resourceType = resourceType;
}

PkStringList KisTag::defaultResources() const
{
    return d->defaultResources;
}

void KisTag::setDefaultResources(const PkStringList &defaultResources)
{
    d->defaultResources = defaultResources;
}

bool KisTag::load(PkStream &io)
{
    if (!io.isOpen()) {
        io.open(PkStream::ReadOnly);
    }
    KIS_ASSERT(io.isOpen());

    setValid(false);

    // 原文本流 readLineInto 循环 → PkStream::readLine(char*, maxSize)
    // 逐行读（UTF-8 字节，含行终止符）。tag 文件行都很短，固定 4K 缓冲足够。
    PkStringList lines;
    char buf[4096];
    while (true) {
        const PkStream::pk_int64 n = io.readLine(buf, sizeof(buf));
        if (n < 0) {
            break;
        }
        PkString line = PkString::PkFromUtf8(buf, static_cast<int>(n));
        while (!line.isEmpty()
               && (line.at(line.size() - 1) == u'\n' || line.at(line.size() - 1) == u'\r')) {
            line = line.left(line.size() - 1);
        }
        lines.append(line);
    }

    if (lines.length() < 6 ) {
        warnResource  << d->filename << ": Incomplete tag file" << lines.length();
        return false;
    }
    if (!pkEqualsAsciiCaseInsensitive(lines[0], s_desktop)) {
        warnResource  << d->filename << ":Invalid tag file" << lines[0];
        return false;
    }

    lines.removeFirst();

    PK_FOREACH(const PkString line, lines) {
        if (line.isEmpty()) {
            continue;
        }

        if (!line.contains("=")) {
            warnResource << "Found invalid line:" << line;
            continue;
        }
        const int isPos = pkIndexOfAscii(line, '=');
        const PkString key = line.left(isPos).trimmed();
        const PkString value = line.right(line.size() - (isPos + 1)).trimmed();

        if (key == s_url) {
            d->url = value;
        }
        else if (key == s_resourceType) {
            d->resourceType = value;
        }
        else if (key == s_defaultResources) {
            d->defaultResources = pkSplitSkipEmpty(value, u',');
        }
        else if (key == s_name) {
            d->name = value;
        }
        else if (key == s_comment) {
            d->comment = value;
        }
        else if (key.startsWith(s_name + "[")) {
            const int start = pkIndexOfAscii(key, '[') + 1;
            const int len = key.size() - (s_name.size() + 2);
            const PkString language = key.mid(start, len);
            d->names[language] = value;
        }
        else if (key.startsWith(s_comment + "[")) {
            const int start = pkIndexOfAscii(key, '[') + 1;
            const int len = key.size() - (s_comment.size() + 2);
            const PkString language = key.mid(start, len);
            d->comments[language] = value;
        }
    }

    setValid(true);

    return true;
}

bool KisTag::save(PkStream &io)
{
    if (!io.isOpen()) {
        io.open(static_cast<PkStream::OpenMode>(PkStream::WriteOnly | PkStream::Text));
    }

    pkWriteLine(io, s_desktop);
    pkWriteLine(io, s_type + "=" + s_tag);
    pkWriteLine(io, s_url + "=" + d->url);
    pkWriteLine(io, s_resourceType + "=" + d->resourceType);
    pkWriteLine(io, s_name + "=" + d->name);
    pkWriteLine(io, s_comment + "=" + d->comment);
    pkWriteLine(io, s_defaultResources + "=" + d->defaultResources.join(u','));

    // 对齐 Qt 原版：foreach 遍历关联容器时按 value 迭代（Qt5 qmap.h
    // const_iterator::operator* 返回 i->value），不是按 key。原版即如此——
    // language 实为翻译名（value），再用它当 key 查 d->names[language]。
    // 这里照抄，保持行为等价（含这个潜在的取值缺陷，见任务报告 concern）。
    const PkList<PkString> nameValues = d->names.values();
    for (const PkString &language : nameValues) {
        pkWriteLine(io, s_name + "[" + language + "]=" + d->names[language]);
    }

    const PkList<PkString> commentValues = d->comments.values();
    for (const PkString &language : commentValues) {
        pkWriteLine(io, s_comment + "[" + language + "]=" + d->comments[language]);
    }

    return true;
}

void KisTag::setId(int id)
{
    d->id = id;
}

void KisTag::setActive(bool active)
{
    d->active = active;
}

void KisTag::setValid(bool valid)
{
    d->valid = valid;
}
