/*  This file is part of the KDE project
 *
 *  SPDX-FileCopyrightText: 2016 L. E. Segovia <amy@amyspark.me>
 *  SPDX-FileCopyrightText: 2005..2022 Halla Rempt <halla@valdyas.org>
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <sys/types.h>

#include <PkXmlCompat.h>

#include <resources/KoColorSet.h>
#include "KoColorSet_p.h"

#include <PkStream.h>
#include <PkMemoryStream.h>
#include <PkTextStream.h>
#include <PkXmlStreamReader.h>
#include <PkXmlStreamAttributes.h>
#include <PkXmlNode.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkVector.h>
#include <PkHash.h>
#include <PkStringList.h>
#include <PkGlobal.h>
#include <PkUtf16.h>
#include <PkFileStream.h>

#include <DebugPigment.h>
#include <kundo2command.h>

#include <KoStore.h>
#include <KoColor.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorProfile.h>
#include <KoColorModelStandardIds.h>
#include "KisSwatch.h"
#include "kis_dom_utils.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

/**
 * readAllLinesSafe() reads all the lines in the byte array
 * using the automated UTF8 and CR/LF transformations. That
 * might be necessary for opening GPL palettes created on Linux
 * in Windows environment.
 */
PkStringList pkReadAllLinesSafe(const PkByteArray &data)
{
    PkStringList lines;

    PkMemoryStream buffer;
    buffer.open(PkStream::WriteOnly);
    buffer.write(data.constData(), data.size());
    buffer.close();
    buffer.open(PkStream::ReadOnly);

    PkTextStream stream(&buffer);

    PkString line;
    while (stream.readLineInto(line)) {   // PkTextStream::readLineInto 取 PkString&，不是指针
        lines << line;
    }

    return lines;
}

PkByteArray pkReadAll(PkStream *dev)
{
    std::vector<uint8_t> buf;
    char chunk[4096];
    while (true) {
        PkStream::pk_int64 n = dev->read(chunk, sizeof(chunk));
        if (n <= 0) {
            break;
        }
        buf.insert(buf.end(), chunk, chunk + n);
    }
    return PkByteArray(buf);
}

// PkMemoryStream 没有「以数据播种再读」的构造形态（见 PkMemoryStream.h），
// 用 open(WriteOnly) → write → close → open(ReadOnly) 复刻内存缓冲(&data) 语义。
void pkSeedReadStream(PkMemoryStream *stream, const PkByteArray &data)
{
    stream->open(PkStream::WriteOnly);
    stream->write(data.constData(), data.size());
    stream->close();
    stream->open(PkStream::ReadOnly);
}

PkByteArray pkReadBytes(PkStream *io, int n)
{
    std::vector<uint8_t> buf(static_cast<size_t>(n));
    PkStream::pk_int64 got = io->read(reinterpret_cast<char*>(buf.data()), n);
    buf.resize(static_cast<size_t>(got > 0 ? got : 0));
    return PkByteArray(buf);
}

// QtEndian 的 qFromBigEndian 等价物（本机为小端，直接字节交换）。
quint16 pkFromBigEndian16(quint16 v) { return __builtin_bswap16(v); }
quint32 pkFromBigEndian32(quint32 v) { return __builtin_bswap32(v); }
float pkFromBigEndianFloat(float v)
{
    union { float f; quint32 u; } c;
    c.f = v;
    c.u = __builtin_bswap32(c.u);
    return c.f;
}

bool pkStartsWith(const PkByteArray &ba, const char *prefix)
{
    int len = static_cast<int>(std::strlen(prefix));
    return ba.size() >= len && std::memcmp(ba.constData(), prefix, len) == 0;
}

int pkIndexOf(const PkByteArray &ba, const char *needle, int from)
{
    int nlen = static_cast<int>(std::strlen(needle));
    if (nlen == 0) {
        return from <= ba.size() ? from : ba.size();
    }
    for (int i = from; i + nlen <= ba.size(); ++i) {
        if (std::memcmp(ba.constData() + i, needle, nlen) == 0) {
            return i;
        }
    }
    return -1;
}

bool pkContains(const PkByteArray &ba, const char *needle)
{
    return pkIndexOf(ba, needle, 0) != -1;
}

bool pkByteEquals(const PkByteArray &ba, const char *s)
{
    int len = static_cast<int>(std::strlen(s));
    return ba.size() == len && std::memcmp(ba.constData(), s, len) == 0;
}

bool pkEndsWith(const PkString &s, const PkString &suffix)
{
    if (suffix.size() > s.size()) {
        return false;
    }
    return s.mid(s.size() - suffix.size()) == suffix;
}

PkStringList pkSplit(const PkString &s, char16_t sep)
{
    PkStringList result;
    for (const PkString &part : s.split(sep)) {
        result.push_back(part);
    }
    return result;
}

PkStringList pkSplitSkipEmpty(const PkString &s, char16_t sep)
{
    PkStringList result;
    for (const PkString &part : s.split(sep)) {
        if (!part.isEmpty()) {
            result.push_back(part);
        }
    }
    return result;
}

PkString pkReplaceChar(const PkString &s, char16_t from, char16_t to)
{
    PkString out;
    for (int i = 0; i < s.size(); ++i) {
        char16_t c = s.at(i);
        if (c == from) {
            c = to;
        }
        out += pkCharToString(c);
    }
    return out;
}

// Qt remove 的等价物：删除所有出现。
PkString pkRemoveSubstr(const PkString &s, const PkString &sub)
{
    return pkStringReplaceAll(s, sub, PkString(), PkCaseSensitive);
}

int pkBound(int lo, int val, int hi)
{
    return std::max(lo, std::min(val, hi));
}

// Qt toFloat 的等价物（PkString 只有 toDouble；浮点精度差异不改变判定结果）。
float pkToFloat(const PkString &s, bool *ok)
{
    double d = s.toDouble(ok);
    return static_cast<float>(d);
}

// Qt toUInt(&ok, 16) 的等价物。
quint32 pkHexToUInt(const PkString &s, bool *ok)
{
    quint32 v = 0;
    if (ok) {
        *ok = true;
    }
    for (int i = 0; i < s.size(); ++i) {
        char16_t c = s.at(i);
        int d = -1;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        }
        if (d < 0) {
            if (ok) {
                *ok = false;
            }
            return 0;
        }
        v = v * 16 + static_cast<quint32>(d);
    }
    return v;
}

// Qt fromLatin1 的等价物（字节直接按 Latin1 映射到 char16_t）。
PkString pkFromLatin1(const char *data, int len)
{
    PkString out;
    for (int i = 0; i < len; ++i) {
        out += pkCharToString(static_cast<char16_t>(static_cast<unsigned char>(data[i])));
    }
    return out;
}

int pkLastIndexOf(const PkString &s, char16_t c)
{
    for (int i = s.size() - 1; i >= 0; --i) {
        if (s.at(i) == c) {
            return i;
        }
    }
    return -1;
}

PkString pkFileName(const PkString &path)
{
    int idx = pkLastIndexOf(path, u'/');
    if (idx < 0) {
        return path;
    }
    return path.mid(idx + 1);
}

PkString pkFileSuffix(const PkString &path)
{
    PkString base = pkFileName(path);
    int dot = pkLastIndexOf(base, u'.');
    if (dot <= 0) {
        return PkString();
    }
    return base.mid(dot + 1);
}

PkString pkFileCompleteBaseName(const PkString &path)
{
    PkString base = pkFileName(path);
    int dot = pkLastIndexOf(base, u'.');
    if (dot <= 0) {
        return base;
    }
    return base.left(dot);
}

// 去除所有 /* ... */ 块注释（非贪婪：遇第一个 */ 即止）。无闭合注释时
// 保留剩余文本（与 Qt 正则 remove 语义一致）。
PkString pkRemoveCComments(const PkString &s)
{
    PkString out;
    int i = 0;
    const int n = s.size();
    while (i < n) {
        if (i + 1 < n && s.at(i) == u'/' && s.at(i + 1) == u'*') {
            int end = -1;
            for (int j = i + 2; j + 1 < n; ++j) {
                if (s.at(j) == u'*' && s.at(j + 1) == u'/') {
                    end = j;
                    break;
                }
            }
            if (end < 0) {
                out += s.mid(i);
                break;
            }
            i = end + 2;
        } else {
            out += pkCharToString(s.at(i));
            ++i;
        }
    }
    return out;
}

struct CssColorMatch {
    PkString colorInfo;
    PkString colorName;
    PkString colorValue;
};

// 复刻正则 "(.*?){(?:[^:;]+:[^;]+;)*?color:(.*?)(?:;.*?)*?}"
// 对 text（已 join + 去 tab/space + 去 C 注释）的 globalMatch 语义：
//   1. (.*?){ —— 从当前全局位置推进到第一个 '{'，captured(1) 为 '{' 前内容；
//   2. (?:[^:;]+:[^;]+;)*?color: —— '{' 后按「属性:值;」组逐段推进，直到命中
//      "color:"；任何一段组不成「[^:;]+:[^;]+;」则该全局位置不匹配，整体前进 1；
//   3. color:(.*?)(?:;.*?)*?} —— captured(2) 为遇到第一个 ';' 或 '}' 前的值；
//      完整匹配（captured()）止于值后（或 ';' 尾巴后）第一个 '}'。
std::vector<CssColorMatch> pkCssGlobalMatch(const PkString &text)
{
    std::vector<CssColorMatch> matches;
    int i = 0;
    const int n = text.size();
    while (i < n) {
        int brace = -1;
        for (int j = i; j < n; ++j) {
            if (text.at(j) == u'{') {
                brace = j;
                break;
            }
        }
        if (brace < 0) {
            break;
        }

        int j = brace + 1;
        int colorStart = -1;
        while (j < n) {
            if (text.at(j) == u'}') {
                break;
            }
            if (j + 6 <= n && text.mid(j, 6) == "color:") {
                colorStart = j + 6;
                break;
            }
            // 尝试消费一个 [^:;]+:[^;]+; 属性组
            int p = j;
            while (p < n && text.at(p) != u':' && text.at(p) != u';') {
                ++p;
            }
            if (p >= n || text.at(p) != u':') {
                break;
            }
            int q = p + 1;
            while (q < n && text.at(q) != u';') {
                ++q;
            }
            if (q >= n) {
                break;
            }
            j = q + 1;
        }

        if (colorStart < 0) {
            i = brace + 1;
            continue;
        }

        int colorEnd = colorStart;
        while (colorEnd < n && text.at(colorEnd) != u'}' && text.at(colorEnd) != u';') {
            ++colorEnd;
        }
        int closeBrace = colorEnd;
        while (closeBrace < n && text.at(closeBrace) != u'}') {
            ++closeBrace;
        }
        if (closeBrace >= n) {
            i = colorEnd;
            continue;
        }

        CssColorMatch m;
        m.colorInfo = text.mid(i, closeBrace + 1 - i);
        m.colorName = text.mid(i, brace - i);
        m.colorValue = text.mid(colorStart, colorEnd - colorStart);
        matches.push_back(m);
        i = closeBrace + 1;
    }
    return matches;
}

PkByteArray pkMimeType(const char *s)
{
    return PkByteArray(s, static_cast<int>(std::strlen(s)));
}

}

const PkString KoColorSet::GLOBAL_GROUP_NAME = PkString();
const PkString KoColorSet::KPL_VERSION_ATTR = "version";
const PkString KoColorSet::KPL_GROUP_ROW_COUNT_ATTR = "rows";
const PkString KoColorSet::KPL_PALETTE_COLUMN_COUNT_ATTR = "columns";
const PkString KoColorSet::KPL_PALETTE_NAME_ATTR = "name";
const PkString KoColorSet::KPL_PALETTE_COMMENT_ATTR = "comment";
const PkString KoColorSet::KPL_PALETTE_FILENAME_ATTR = "filename";
const PkString KoColorSet::KPL_PALETTE_READONLY_ATTR = "readonly";
const PkString KoColorSet::KPL_COLOR_MODEL_ID_ATTR = "colorModelId";
const PkString KoColorSet::KPL_COLOR_DEPTH_ID_ATTR = "colorDepthId";
const PkString KoColorSet::KPL_GROUP_NAME_ATTR = "name";
const PkString KoColorSet::KPL_SWATCH_ROW_ATTR = "row";
const PkString KoColorSet::KPL_SWATCH_COL_ATTR = "column";
const PkString KoColorSet::KPL_SWATCH_NAME_ATTR = "name";
const PkString KoColorSet::KPL_SWATCH_ID_ATTR = "id";
const PkString KoColorSet::KPL_SWATCH_SPOT_ATTR = "spot";
const PkString KoColorSet::KPL_SWATCH_BITDEPTH_ATTR = "bitdepth";
const PkString KoColorSet::KPL_PALETTE_PROFILE_TAG = "Profile";
const PkString KoColorSet::KPL_SWATCH_POS_TAG = "Position";
const PkString KoColorSet::KPL_SWATCH_TAG = "ColorSetEntry";
const PkString KoColorSet::KPL_GROUP_TAG = "Group";
const PkString KoColorSet::KPL_PALETTE_TAG = "ColorSet";

const int MAXIMUM_ALLOWED_COLUMNS = 4096;


struct AddSwatchCommand : public KUndo2Command
{

   AddSwatchCommand(KoColorSet *colorSet, const KisSwatch &swatch, const PkString &groupName, int column, int row)
       : m_colorSet(colorSet)
       , m_swatch(swatch)
       , m_groupName(groupName)
       , m_x(column)
       , m_y(row)
   {
   }

   ~AddSwatchCommand() override {}

    /// redo the command
    void redo() override
    {
        KisSwatchGroupSP modifiedGroup = m_colorSet->getGroup(m_groupName);
        if (m_x < 0 || m_y < 0) {
            PkPair<int, int> pos = modifiedGroup->addSwatch(m_swatch);
            m_x = pos.first;
            m_y = pos.second;
        }
        else {
            modifiedGroup->setSwatch(m_swatch, m_x, m_y);
        }
        m_colorSet->notifySwatchChanged(m_groupName, m_x, m_y);
    }

    /// revert the actions done in redo
    void undo() override
    {
        KisSwatchGroupSP modifiedGroup = m_colorSet->getGroup(m_groupName);
        modifiedGroup->removeSwatch(m_x, m_y);
        m_colorSet->notifySwatchChanged(m_groupName, m_x, m_y);
    }

private:
    KoColorSet *m_colorSet;
    KisSwatch m_swatch;
    PkString m_groupName;
    int m_x;
    int m_y;
};


struct RemoveSwatchCommand : public KUndo2Command
{
   RemoveSwatchCommand(KoColorSet *colorSet, int column, int row, KisSwatchGroupSP group)
       : m_colorSet(colorSet)
       , m_swatch(group->getSwatch(column, row))
       , m_group(group)
       , m_x(column)
       , m_y(row)

   {
   }

   /// redo the command
    void redo() override
    {
        m_group->removeSwatch(m_x, m_y);
        m_colorSet->notifySwatchChanged(m_group->name(), m_x, m_y);
    }

    /// revert the actions done in redo
    void undo() override
    {
        m_group->setSwatch(m_swatch, m_x, m_y);
        m_colorSet->notifySwatchChanged(m_group->name(), m_x, m_y);
    }

private:
    KoColorSet *m_colorSet;
    KisSwatch m_swatch;
    KisSwatchGroupSP m_group;
    int m_x;
    int m_y;
};

struct ChangeGroupNameCommand : public KUndo2Command
{

   ChangeGroupNameCommand(KoColorSet *colorSet, PkString oldGroupName, const PkString &newGroupName)
       : m_colorSet(colorSet)
       , m_oldGroupName(oldGroupName)
       , m_newGroupName(newGroupName)
   {
   }

   ~ChangeGroupNameCommand() override {}

    /// redo the command
    void redo() override
    {
        KisSwatchGroupSP group = m_colorSet->getGroup(m_oldGroupName);
        group->setName(m_newGroupName);
        m_colorSet->entryChanged(0, m_colorSet->startRowForGroup(m_newGroupName));
    }

    /// revert the actions done in redo
    void undo() override
    {
        KisSwatchGroupSP group = m_colorSet->getGroup(m_newGroupName);
        group->setName(m_oldGroupName);
        m_colorSet->entryChanged(0, m_colorSet->startRowForGroup(m_oldGroupName));
    }

private:
    KoColorSet *m_colorSet;
    PkString m_oldGroupName;
    PkString m_newGroupName;
};

struct MoveGroupCommand : public KUndo2Command
{

   MoveGroupCommand(KoColorSet *colorSet, PkString groupName, const PkString &groupNameInsertBefore)
       : m_colorSet(colorSet)
       , m_groupName(groupName)
       , m_groupNameInsertBefore(groupNameInsertBefore)
       , m_oldIndex(0)
       , m_newIndex(0)
   {
       int idx = 0;
       for (const KisSwatchGroupSP &group : m_colorSet->d->swatchGroups) {

           if (group->name() == m_groupName) {
               m_oldIndex = idx;
           }

           if (group->name() == m_groupNameInsertBefore) {
               m_newIndex = idx;
           }

           idx++;
       }
   }

    /// redo the command
    void redo() override
    {
        if (m_groupNameInsertBefore != KoColorSet::GLOBAL_GROUP_NAME &&
                m_groupName != KoColorSet::GLOBAL_GROUP_NAME)
        {
            m_colorSet->layoutAboutToChange();
            KisSwatchGroupSP group = m_colorSet->d->swatchGroups.takeAt(m_oldIndex);
            m_colorSet->d->swatchGroups.insert(m_newIndex, group);
            m_colorSet->layoutChanged();
        }
    }


    /// revert the actions done in redo
    void undo() override
    {
        m_colorSet->layoutAboutToChange();
        KisSwatchGroupSP group = m_colorSet->d->swatchGroups.takeAt(m_newIndex);
        m_colorSet->d->swatchGroups.insert(m_oldIndex, group);
        m_colorSet->layoutChanged();
    }

private:
    KoColorSet *m_colorSet;
    PkString m_groupName;
    PkString m_groupNameInsertBefore;
    int m_oldIndex;
    int m_newIndex;
};

struct AddGroupCommand : public KUndo2Command
{

   AddGroupCommand(KoColorSet *colorSet, PkString groupName, int columnCount, int rowCount)
       : m_colorSet(colorSet)
       , m_groupName(groupName)
       , m_columnCount(columnCount)
       , m_rowCount(rowCount)
   {
   }

    /// redo the command
    void redo() override
    {
        KisSwatchGroupSP group(new KisSwatchGroup);
        group->setName(m_groupName);
        group->setColumnCount(m_columnCount);
        group->setRowCount(m_rowCount);
        m_colorSet->layoutAboutToChange();
        m_colorSet->d->swatchGroups.append(group);
        m_colorSet->layoutChanged();
    }


    /// revert the actions done in redo
    void undo() override
    {
        int idx = 0;
        bool found = false;
        for(const KisSwatchGroupSP &group : m_colorSet->d->swatchGroups) {
            if (group->name() == m_groupName) {
                found = true;
                break;
            }
            idx++;
        }
        if (found) {
            m_colorSet->layoutAboutToChange();
            m_colorSet->d->swatchGroups.takeAt(idx);
            m_colorSet->layoutChanged();
        }
    }

private:
    KoColorSet *m_colorSet;
    PkString m_groupName;
    int m_columnCount;
    int m_rowCount;
};

struct RemoveGroupCommand : public KUndo2Command
{
   RemoveGroupCommand(KoColorSet *colorSet, PkString groupName, bool keepColors = true)
       : m_colorSet(colorSet)
       , m_groupName(groupName)
       , m_keepColors(keepColors)
       , m_oldGroup(m_colorSet->getGroup(groupName))
       , m_startingRow(m_colorSet->getGlobalGroup()->rowCount())
       , m_groupIndex(0)
   {
       for (m_groupIndex = 0; m_groupIndex < colorSet->d->swatchGroups.size(); ++ m_groupIndex) {
           if (colorSet->d->swatchGroups[m_groupIndex]->name() == m_oldGroup->name()) {
               break;
           }
       }
   }

    /// redo the command
    void redo() override
    {
        if (m_keepColors) {
            // put all colors directly below global
            KisSwatchGroupSP globalGroup = m_colorSet->getGlobalGroup();
            for (const KisSwatchGroup::SwatchInfo &info : m_oldGroup->infoList()) {
                globalGroup->setSwatch(info.swatch,
                                      info.column,
                                      info.row + m_startingRow);
            }
        }

        m_colorSet->layoutAboutToChange();
        m_colorSet->d->swatchGroups.removeOne(m_oldGroup);
        m_colorSet->layoutChanged();
    }

    /// revert the actions done in redo
    void undo() override
    {
        m_colorSet->layoutAboutToChange();
        m_colorSet->d->swatchGroups.insert(m_groupIndex, m_oldGroup);

        // remove all colors that were inserted into global
        if (m_keepColors) {
            KisSwatchGroupSP globalGroup = m_colorSet->getGlobalGroup();
            for (const KisSwatchGroup::SwatchInfo &info : globalGroup->infoList()) {
                m_oldGroup->setSwatch(info.swatch, info.column, info.row - m_startingRow);
                globalGroup->removeSwatch(info.column,
                                         info.row + m_startingRow);
            }
        }
        m_colorSet->layoutChanged();
    }

private:
    KoColorSet *m_colorSet;
    PkString m_groupName;
    bool m_keepColors;
    KisSwatchGroupSP m_oldGroup;
    int m_groupIndex;
    int m_startingRow;
};

struct ClearCommand : public KUndo2Command
{
   ClearCommand(KoColorSet *colorSet)
       : m_colorSet(colorSet)
       , m_OldColorSet(new KoColorSet(*colorSet))
   {
   }


   ~ClearCommand() override
   {
       delete m_OldColorSet;
   }

    /// redo the command
    void redo() override
    {
        m_colorSet->d->swatchGroups.clear();
        KisSwatchGroupSP global(new KisSwatchGroup);
        global->setName(KoColorSet::GLOBAL_GROUP_NAME);
        m_colorSet->layoutAboutToChange();
        m_colorSet->d->swatchGroups.append(global);
        m_colorSet->layoutChanged();
    }


    /// revert the actions done in redo
    void undo() override
    {
        m_colorSet->layoutAboutToChange();
        m_colorSet->d->swatchGroups = m_OldColorSet->d->swatchGroups;
        KUndo2Command::undo();
        m_colorSet->layoutChanged();
    }

private:
    KoColorSet *m_colorSet;
    KoColorSet *m_OldColorSet;
};

struct SetColumnCountCommand : public KUndo2Command
{
   SetColumnCountCommand(KoColorSet *colorSet, int columnCount)
       : m_colorSet(colorSet)
       , m_columnsCount(columnCount)
       , m_oldColumnsCount(colorSet->columnCount())
   {
   }

    /// redo the command
    void redo() override
    {
        m_colorSet->layoutAboutToChange();
        for (KisSwatchGroupSP &group : m_colorSet->d->swatchGroups) {
            group->setColumnCount(m_columnsCount);
        }
        m_colorSet->d->columns = m_columnsCount;
        m_colorSet->layoutChanged();
    }


    /// revert the actions done in redo
    void undo() override
    {
        m_colorSet->layoutAboutToChange();
        for (KisSwatchGroupSP &group : m_colorSet->d->swatchGroups) {
            group->setColumnCount(m_oldColumnsCount);
        }
        m_colorSet->d->columns = m_oldColumnsCount;
        m_colorSet->layoutChanged();
    }

private:
    KoColorSet *m_colorSet;
    int m_columnsCount;
    int m_oldColumnsCount;
};


struct SetCommentCommand : public KUndo2Command
{
   SetCommentCommand(KoColorSet *colorSet, const PkString &comment)
       : m_colorSet(colorSet)
       , m_comment(comment)
       , m_oldComment(colorSet->comment())
   {
   }

    /// redo the command
    void redo() override
    {
        m_colorSet->d->comment = m_comment;
    }


    /// revert the actions done in redo
    void undo() override
    {
        m_colorSet->d->comment = m_oldComment;
    }

private:
    KoColorSet *m_colorSet;
    PkString m_comment;
    PkString m_oldComment;
};

struct SetPaletteTypeCommand : public KUndo2Command
{
   SetPaletteTypeCommand(KoColorSet *colorSet, const KoColorSet::PaletteType &paletteType)
       : m_colorSet(colorSet)
       , m_paletteType(paletteType)
       , m_oldPaletteType(colorSet->paletteType())
   {
   }

    /// redo the command
    void redo() override
    {
        m_colorSet->d->paletteType = m_paletteType;
        PkStringList fileName;
        for (const PkString &part : m_colorSet->filename().split(u'.')) {
            fileName.append(part);
        }
        fileName.last() = pkRemoveSubstr(suffix(m_paletteType), PkString("."));
        m_colorSet->setFilename(fileName.join("."));
    }


    /// revert the actions done in redo
    void undo() override
    {
        m_colorSet->d->paletteType = m_oldPaletteType;
        PkStringList fileName;
        for (const PkString &part : m_colorSet->filename().split(u'.')) {
            fileName.append(part);
        }
        fileName.last() = pkRemoveSubstr(suffix(m_oldPaletteType), PkString("."));
        m_colorSet->setFilename(fileName.join("."));
    }

private:

    PkString suffix(KoColorSet::PaletteType paletteType) const
    {
        PkString suffix;
        switch(paletteType) {
        case KoColorSet::GPL:
            suffix = ".gpl";
            break;
        case KoColorSet::ACT:
            suffix = ".act";
            break;
        case KoColorSet::RIFF_PAL:
        case KoColorSet::PSP_PAL:
            suffix = ".pal";
            break;
        case KoColorSet::ACO:
            suffix = ".aco";
            break;
        case KoColorSet::XML:
            suffix = ".xml";
            break;
        case KoColorSet::KPL:
            suffix = ".kpl";
            break;
        case KoColorSet::SBZ:
            suffix = ".sbz";
            break;
        case KoColorSet::CSS:
            suffix = ".css";
            break;
        default:
            suffix = m_colorSet->defaultFileExtension();
        }
        return suffix;
    }

    KoColorSet *m_colorSet;
    KoColorSet::PaletteType m_paletteType;
    KoColorSet::PaletteType m_oldPaletteType;
};

KoColorSet::KoColorSet(const PkString &filename)
    : PkObject()
    , KoResource(filename)
    , d(new Private(this))
{
    // Task 8：原 connect(&d->undoStack, SIGNAL(canUndoChanged(bool)), this,
    // SLOT(canUndoChanged(bool))) 两条元对象连接已移除，对应信号语义由
    // Task 8 重建（undoStack 的 canUndoChanged/canRedoChanged → 本类 slot）。
}

/// Create an copied palette
KoColorSet::KoColorSet(const KoColorSet &rhs)
    : PkObject()
    , KoResource(rhs)
    , d(new Private(this))
{
    d->paletteType = rhs.d->paletteType;
    d->data = rhs.d->data;
    d->comment = rhs.d->comment;
    d->swatchGroups = rhs.d->swatchGroups;
}

KoColorSet::~KoColorSet()
{
}

KoResourceSP KoColorSet::clone() const
{
    return KoResourceSP(new KoColorSet(*this));
}

bool KoColorSet::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(resourcesInterface);

    if (!dev->isOpen()) dev->open(PkStream::ReadOnly);

    d->data = pkReadAll(dev);

    Q_ASSERT(d->data.size() != 0);

    return d->init();
}

bool KoColorSet::saveToDevice(PkStream *dev) const
{
    bool res = false;
    switch(d->paletteType) {
    case GPL:
        res = d->saveGpl(dev);
        break;
    default:
        res = d->saveKpl(dev);
    }

    if (res) const_cast<KoColorSet*>(this)->setDirty(false);
    d->undoStack.clear();

    return res;
}

bool KoColorSet::fromByteArray(PkByteArray &data, KisResourcesInterfaceSP resourcesInterface)
{
    PkMemoryStream buf;
    pkSeedReadStream(&buf, data);
    return loadFromDevice(&buf, resourcesInterface);
}

KoColorSet::PaletteType KoColorSet::paletteType() const
{
    return d->paletteType;
}

void KoColorSet::setPaletteType(PaletteType paletteType)
{
    if (d->isLocked || paletteType == d->paletteType) return;

    SetPaletteTypeCommand *cmd = new SetPaletteTypeCommand(this, paletteType);

    d->undoStack.push(cmd);
}


void KoColorSet::addSwatch(const KisSwatch &swatch, const PkString &groupName, int column, int row)
{
    if (d->isLocked) return;

    AddSwatchCommand *cmd = new AddSwatchCommand(this, swatch, groupName, column, row);

    d->undoStack.push(cmd);

}

void KoColorSet::removeSwatch(int column, int row, KisSwatchGroupSP group)
{
    if (d->isLocked) return;
    RemoveSwatchCommand *cmd = new RemoveSwatchCommand(this, column, row, group);

    d->undoStack.push(cmd);
}

void KoColorSet::clear()
{
    if (d->isLocked) return;

    ClearCommand *cmd = new ClearCommand(this);

    d->undoStack.push(cmd);
}

KisSwatch KoColorSet::getColorGlobal(quint32 column, quint32 row) const
{
    KisSwatchGroupSP group = getGroup(row);
    Q_ASSERT(group);

    int titleRow = startRowForGroup(group->name());
    int rowInGroup = -1;

    if (group->name().isEmpty()) {
        rowInGroup = (int)row - titleRow;
    }
    else {
        rowInGroup = (int)row - (titleRow + 1);
    }

    Q_ASSERT((isGroupTitleRow(titleRow) && titleRow > 0) || titleRow == 0);
    Q_ASSERT(rowInGroup < group->rowCount());

    return group->getSwatch(column, rowInGroup);

}

KisSwatch KoColorSet::getSwatchFromGroup(quint32 column, quint32 row, PkString groupName) const
{
    KisSwatch swatch;
    for (const KisSwatchGroupSP &group: d->swatchGroups) {
        if (group->name() == groupName) {
            if (group->checkSwatchExists(column, row)) {
                swatch = group->getSwatch(column, row);
            }
            break;
        }
    }
    return swatch;
}

PkStringList KoColorSet::swatchGroupNames() const
{
    PkStringList groupNames;
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        groupNames << group->name();
    }
    return groupNames;
}

bool KoColorSet::isGroupTitleRow(int row) const
{
    int idx = 0;
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        idx += group->rowCount();
        if (group->name() != KoColorSet::GLOBAL_GROUP_NAME) {
            idx++;
        }
        if (idx == row) {
            return true;
        }
    }
    return false;
}

int KoColorSet::startRowForGroup(const PkString &groupName) const
{
    if (groupName.isEmpty()) return 0;

    int row = 0;
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        if (group->name() == groupName) {
            return row;
        }
        row += group->rowCount();
        if (group->name() != KoColorSet::GLOBAL_GROUP_NAME) {
            row++;
        }
    }
    return row;
}

int KoColorSet::rowNumberInGroup(int rowNumber) const
{
    if (isGroupTitleRow(rowNumber)) {
        return -1;
    }

    int rowInGroup = -1;
    for (int i = rowNumber; i > -1; i--) {
        if (isGroupTitleRow(i)) {
            return rowInGroup;
        }
        else {
            rowInGroup++;
        }
    }

    return rowInGroup;
}

void KoColorSet::setModified(bool _modified)
{
    setDirty(_modified);
    if (_modified) {
        modified();
    }
}

void KoColorSet::notifySwatchChanged(const PkString &groupName, int column, int row)
{
    int startRow = 0;
    if (!groupName.isEmpty()) {
        startRow = startRowForGroup(groupName) + 1;
    }
    entryChanged(column, startRow + row);
}


void KoColorSet::canUndoChanged(bool canUndo)
{
    if (canUndo) {
        setModified(true);
    }
    else {
        setModified(false);
    }
}

void KoColorSet::canRedoChanged(bool /*canRedo*/)
{
    if (d->undoStack.canUndo()) {
        setModified(true);
    }
    else {
        setModified(false);
    }
}

void KoColorSet::changeGroupName(const PkString &oldGroupName, const PkString &newGroupName)
{
    PkStringList names = swatchGroupNames();
    bool hasOld = std::find(names.begin(), names.end(), oldGroupName) != names.end();
    if (!hasOld || (oldGroupName == newGroupName) || d->isLocked) return;

    ChangeGroupNameCommand *cmd = new ChangeGroupNameCommand(this, oldGroupName, newGroupName);
    d->undoStack.push(cmd);
}

void KoColorSet::setColumnCount(int columns)
{
    if (d->isLocked || (columns == d->columns)) return;

    SetColumnCountCommand *cmd = new SetColumnCountCommand (this, columns);

    d->undoStack.push(cmd);
}

int KoColorSet::columnCount() const
{
    Q_ASSERT(d->swatchGroups.size() > 0);

    return d->swatchGroups.first()->columnCount();
}

PkString KoColorSet::comment()
{
    return d->comment;
}

void KoColorSet::setComment(PkString comment)
{
    if (d->isLocked || comment == d->comment) return;

    SetCommentCommand *cmd = new SetCommentCommand(this, comment);

    d->undoStack.push(cmd);
}

void KoColorSet::addGroup(const PkString &groupName, int columnCount, int rowCount)
{
    PkStringList names = swatchGroupNames();
    bool hasName = std::find(names.begin(), names.end(), groupName) != names.end();
    if (hasName || d->isLocked) return;

    AddGroupCommand *cmd = new AddGroupCommand(this, groupName, columnCount, rowCount);

    d->undoStack.push(cmd);
}

void KoColorSet::moveGroup(const PkString &groupName, const PkString &groupNameInsertBefore)
{
    PkStringList groupNames = swatchGroupNames();
    bool hasGroup = std::find(groupNames.begin(), groupNames.end(), groupName) != groupNames.end();
    bool hasInsertBefore = std::find(groupNames.begin(), groupNames.end(), groupNameInsertBefore) != groupNames.end();
    if (!hasGroup
            || !hasInsertBefore
            || d->isLocked) return;

    MoveGroupCommand *cmd = new MoveGroupCommand(this, groupName, groupNameInsertBefore);

    d->undoStack.push(cmd);

}

void KoColorSet::removeGroup(const PkString &groupName, bool keepColors)
{
    PkStringList names = swatchGroupNames();
    bool hasName = std::find(names.begin(), names.end(), groupName) != names.end();
    if (!hasName || (groupName == GLOBAL_GROUP_NAME) || d->isLocked) return;

    RemoveGroupCommand *cmd = new RemoveGroupCommand(this, groupName, keepColors);

    d->undoStack.push(cmd);
}

PkString KoColorSet::defaultFileExtension() const
{
    return (d->paletteType == GPL) ? PkString(".gpl") : PkString(".kpl");
}

KUndo2Stack *KoColorSet::undoStack() const
{
    return &d->undoStack;
}

void KoColorSet::setLocked(bool lock)
{
    d->isLocked = lock;
}

bool KoColorSet::isLocked() const
{
    return d->isLocked;
}

int KoColorSet::rowCount() const
{
    int res = 0;
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        res += group->rowCount();
    }
    return res;
}

int KoColorSet::rowCountWithTitles() const
{
    return rowCount() + d->swatchGroups.size() - 1;
}

quint32 KoColorSet::colorCount() const
{
    int colorCount = 0;
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        colorCount += group->colorCount();
    }
    return colorCount;
}

quint32 KoColorSet::slotCount() const
{
    int slotCount = 0;
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        slotCount += group->slotCount();
    }
    return slotCount;
}

KisSwatchGroupSP KoColorSet::getGroup(const PkString &name) const
{
    for (KisSwatchGroupSP &group : d->swatchGroups) {
        if (group->name() == name) {
            return group;
        }
    }
    return 0;
}

KisSwatchGroupSP KoColorSet::getGroup(int row) const
{
    if (row >= rowCountWithTitles()) return nullptr;

    int currentRow = 0;

    for (KisSwatchGroupSP &group : d->swatchGroups) {

        int groupRowCount = group->rowCount();
        if (group->name() != KoColorSet::GLOBAL_GROUP_NAME) {
            groupRowCount++;
        }

        bool hit = (currentRow <= row && row < currentRow + groupRowCount);

        if  (hit) {
            return group;
        }

        currentRow += group->rowCount();

        if (group->name() != KoColorSet::GLOBAL_GROUP_NAME) {
             currentRow += 1;
        }

        if (currentRow >= rowCountWithTitles()) return nullptr;
    }

    return nullptr;

}

KisSwatchGroupSP KoColorSet::getGlobalGroup() const
{
    Q_ASSERT(d->swatchGroups.size() > 0);
    Q_ASSERT(d->swatchGroups.first()->name() == GLOBAL_GROUP_NAME);
    return d->swatchGroups.first();
}

KisSwatchGroup::SwatchInfo KoColorSet::getClosestSwatchInfo(KoColor compare, bool useGivenColorSpace) const
{
    KisSwatchGroup::SwatchInfo closestSwatch;

    quint8 highestPercentage = 0;
    quint8 testPercentage = 0;

    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        for (const KisSwatchGroup::SwatchInfo &currInfo : group->infoList()) {
            KoColor color = currInfo.swatch.color();
            if (useGivenColorSpace == true && compare.colorSpace() != color.colorSpace()) {
                color.convertTo(compare.colorSpace());

            } else if (compare.colorSpace() != color.colorSpace()) {
                compare.convertTo(color.colorSpace());
            }
            testPercentage = (255 - compare.colorSpace()->difference(compare.data(), color.data()));
            if (testPercentage > highestPercentage)
            {
                highestPercentage = testPercentage;
                closestSwatch = currInfo;
            }
        }
    }
    return closestSwatch;
}

void KoColorSet::updateThumbnail()
{
    int rows = 0;

    // Determine the last filled row in each group
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        int lastRowInGroup = 0;
        for (const KisSwatchGroup::SwatchInfo &info : group->infoList()) {
            lastRowInGroup = std::max(lastRowInGroup, info.row);
        }
        rows += (lastRowInGroup + 1);
    }

    PkImage img(d->global()->columnCount() * 4, rows * 4, PkImage::Format_ARGB32);
    img.fill(0xff808080u); // Qt::darkGray

    int lastRow = 0;
    for (const KisSwatchGroupSP &group : d->swatchGroups) {
        int lastRowGroup = 0;
        for (const KisSwatchGroup::SwatchInfo &info : group->infoList()) {
            PkColor c = info.swatch.color().toQColor();
            for (int x = 0; x < 4; ++x) {
                for (int y = 0; y < 4; ++y) {
                    img.setPixelColor(info.column * 4 + x, (lastRow + info.row) * 4 + y, c.rgba());
                }
            }
            lastRowGroup = std::max(lastRowGroup, info.row);
        }
        lastRow += (lastRowGroup + 1);
    }

    setImage(img);
}

/********************************KoColorSet::Private**************************/

KoColorSet::Private::Private(KoColorSet *a_colorSet)
    : colorSet(a_colorSet)
{
    undoStack.setUndoLimit(100);
    KisSwatchGroupSP group(new KisSwatchGroup);
    group->setName(KoColorSet::GLOBAL_GROUP_NAME);
    swatchGroups.clear();
    swatchGroups.append(group);
}

KoColorSet::PaletteType KoColorSet::Private::detectFormat(const PkString &fileName, const PkByteArray &ba)
{
    PkString suffix = pkFileSuffix(fileName).toLower();

    // .pal
    if (pkStartsWith(ba, "RIFF") && pkIndexOf(ba, "PAL data", 8) != 0) {
        return KoColorSet::RIFF_PAL;
    }
    // .gpl
    else if (pkStartsWith(ba, "GIMP Palette")) {
        return KoColorSet::GPL;
    }
    // .pal
    else if (pkStartsWith(ba, "JASC-PAL")) {
        return KoColorSet::PSP_PAL;
    }
    else if (pkContains(ba, "krita/x-colorset") || pkContains(ba, "application/x-krita-palette")) {
        return KoColorSet::KPL;
    }
    else if (suffix == "aco") {
        return KoColorSet::ACO;
    }
    else if (suffix == "act") {
        return KoColorSet::ACT;
    }
    else if (suffix == "xml") {
        return KoColorSet::XML;
    }
    else if (suffix == "sbz") {
        return KoColorSet::SBZ;
    }
    else if (suffix == "ase" || pkStartsWith(ba, "ASEF")) {
        return KoColorSet::ASE;
    }
    else if (suffix == "acb" || pkStartsWith(ba, "8BCB")) {
        return KoColorSet::ACB;
    }
    else if (suffix == "css") {
        return KoColorSet::CSS;
    }
    return KoColorSet::UNKNOWN;
}

void KoColorSet::Private::scribusParseColor(KoColorSet *set, PkXmlStreamReader *xml)
{
    KisSwatch colorEntry;
    // It's a color, retrieve it
    PkXmlStreamAttributes colorProperties = xml->attributes();
    PkString colorName = colorProperties.value("NAME");
    colorEntry.setName(colorName.isEmpty() ? PkString("Untitled") : colorName);

    // RGB or CMYK?
    if (colorProperties.hasAttribute("RGB")) {
        dbgPigment << "Color " << colorProperties.value("NAME") << ", RGB " << colorProperties.value("RGB");

        KoColor currentColor(KoColorSpaceRegistry::instance()->rgb8());
        PkString colorValue = colorProperties.value("RGB");

        if (colorValue.size() != 7 && colorValue.at(0) != '#') { // Color is a hexadecimal number
            xml->raiseError(PkString("Invalid rgb8 color (malformed): ") + colorValue);
            return;
        } else {
            bool rgbOk;
            quint32 rgb = pkHexToUInt(colorValue.mid(1), &rgbOk);
            if  (!rgbOk) {
                xml->raiseError(PkString("Invalid rgb8 color (unable to convert): ") + colorValue);
                return;
            }

            quint8 r = rgb >> 16 & 0xff;
            quint8 g = rgb >> 8 & 0xff;
            quint8 b = rgb & 0xff;

            dbgPigment << "Color parsed: "<< r << g << b;

            currentColor.data()[0] = r;
            currentColor.data()[1] = g;
            currentColor.data()[2] = b;
            currentColor.setOpacity(OPACITY_OPAQUE_U8);
            colorEntry.setColor(currentColor);

            set->addSwatch(colorEntry);

            while(xml->readNextStartElement()) {
                //ignore - these are all unknown or the /> element tag
                xml->skipCurrentElement();
            }
            return;
        }
    }
    else if (colorProperties.hasAttribute("CMYK")) {
        dbgPigment << "Color " << colorProperties.value("NAME") << ", CMYK " << colorProperties.value("CMYK");

        KoColor currentColor(KoColorSpaceRegistry::instance()->colorSpace(CMYKAColorModelID.id(), Integer8BitsColorDepthID.id(), PkString()));
        PkString colorValue = colorProperties.value("CMYK");

        if (colorValue.size() != 9 && colorValue.at(0) != '#') { // Color is a hexadecimal number
            xml->raiseError(PkString("Invalid cmyk color (malformed): ") + colorValue);
            return;
        }
        else {
            bool cmykOk;
            quint32 cmyk = pkHexToUInt(colorValue.mid(1), &cmykOk); // cmyk uses the full 32 bits
            if  (!cmykOk) {
                xml->raiseError(PkString("Invalid cmyk color (unable to convert): ") + colorValue);
                return;
            }

            quint8 c = cmyk >> 24 & 0xff;
            quint8 m = cmyk >> 16 & 0xff;
            quint8 y = cmyk >> 8 & 0xff;
            quint8 k = cmyk & 0xff;

            dbgPigment << "Color parsed: "<< c << m << y << k;

            currentColor.data()[0] = c;
            currentColor.data()[1] = m;
            currentColor.data()[2] = y;
            currentColor.data()[3] = k;
            currentColor.setOpacity(OPACITY_OPAQUE_U8);
            colorEntry.setColor(currentColor);

            set->addSwatch(colorEntry);

            while(xml->readNextStartElement()) {
                //ignore - these are all unknown or the /> element tag
                xml->skipCurrentElement();
            }
            return;
        }
    }
    else {
        xml->raiseError(PkString("Unknown color space for color ") + colorEntry.name());
    }
}

bool KoColorSet::Private::loadScribusXmlPalette(KoColorSet *set, PkXmlStreamReader *xml)
{

    //1. Get name
    PkXmlStreamAttributes paletteProperties = xml->attributes();
    PkString paletteName = paletteProperties.value("Name");
    dbgPigment << "Processed name of palette:" << paletteName;
    set->setName(paletteName);

    //2. Inside the SCRIBUSCOLORS, there are lots of colors. Retrieve them

    while(xml->readNextStartElement()) {
        PkString currentElement = xml->name();
        if (currentElement.toLower() == PkString("color")) {
            scribusParseColor(set, xml);
        }
        else {
            xml->skipCurrentElement();
        }
    }

    if(xml->hasError()) {
        return false;
    }

    return true;
}

quint8 KoColorSet::Private::readByte(PkStream *io)
{
    quint8 val;
    PkStream::pk_int64 read = io->read(reinterpret_cast<char*>(&val), 1);
    if (read != 1) return 0;
    return val;
}

quint16 KoColorSet::Private::readShort(PkStream *io) {
    quint16 val;
    PkStream::pk_int64 read = io->read(reinterpret_cast<char*>(&val), 2);
    if (read != 2) return 0;
    return pkFromBigEndian16(val);
}

qint32 KoColorSet::Private::readInt(PkStream *io)
{
    qint32 val;
    PkStream::pk_int64 read = io->read(reinterpret_cast<char*>(&val), 4);
    if (read != 4) return 0;
    return static_cast<qint32>(pkFromBigEndian32(static_cast<quint32>(val)));
}

float KoColorSet::Private::readFloat(PkStream *io)
{
    float val;
    PkStream::pk_int64 read = io->read(reinterpret_cast<char*>(&val), 4);
    if (read != 4) return 0.0f;
    return pkFromBigEndianFloat(val);
}

PkString KoColorSet::Private::readUnicodeString(PkStream *io, bool sizeIsInt)
{
    PkString unicode;
    qint32 size = 0;
    if (sizeIsInt) {
        size = readInt(io);
    } else {
        size = readShort(io)-1;
    }
    if (size>0) {
        PkByteArray ba = pkReadBytes(io, size*2);
        if (ba.size() == int(size)*2) {
            unicode = pkUtf16BEToUtf8(ba);
        } else {
            warnPigment << "Unicode name block is the wrong size" << colorSet->filename();
        }
    }
    if (!sizeIsInt) {
        readShort(io); // when the size is quint16, the string is 00 terminated;
    }
    return unicode.trimmed();
}

bool KoColorSet::Private::init()
{
    // just in case this is a reload (eg by KoEditColorSetDialog),
    swatchGroups.clear();
    KisSwatchGroupSP globalGroup(new KisSwatchGroup);
    globalGroup->setName(KoColorSet::GLOBAL_GROUP_NAME);
    swatchGroups.append(globalGroup);
    undoStack.clear();

    if (colorSet->filename().isEmpty()) {
        warnPigment << "Cannot load palette" << colorSet->name() << "there is no filename set";
        return false;
    }
    if (data.isEmpty()) {
        PkFileStream file(colorSet->filename());
        if (!file.open(PkStream::ReadOnly)) {
            warnPigment << "Cannot load palette" << colorSet->name() << "cannot open file";
            return false;
        }
        if (file.size() == 0) {
            warnPigment << "Cannot load palette" << colorSet->name() << "there is no data available";
            return false;
        }
        data = pkReadAll(&file);
        file.close();
    }

    bool res = false;
    paletteType = detectFormat(colorSet->filename(), data);
    switch(paletteType) {
    case GPL:
        res = loadGpl();
        break;
    case ACT:
        res = loadAct();
        break;
    case RIFF_PAL:
        res = loadRiff();
        break;
    case PSP_PAL:
        res = loadPsp();
        break;
    case ACO:
        res = loadAco();
        break;
    case XML:
        res = loadXml();
        break;
    case KPL:
        res = loadKpl();
        break;
    case SBZ:
        res = loadSbz();
        break;
    case ASE:
        res = loadAse();
        break;
    case ACB:
        res = loadAcb();
        break;
    case CSS:
        res = loadCss();
        break;
    default:
        res = false;
    }
    if (paletteType != KPL) {
        int rowCount = global()->colorCount() / global()->columnCount();
        if (global()->colorCount() % global()->columnCount() > 0) {
            rowCount ++;
        }
        global()->setRowCount(rowCount);
    }
    colorSet->setValid(res);
    colorSet->updateThumbnail();

    data.resize(0);
    undoStack.clear();

    return res;
}

bool KoColorSet::Private::saveGpl(PkStream *dev) const
{
    Q_ASSERT(dev->isOpen());
    Q_ASSERT(dev->isWritable());

    PkTextStream stream(dev);
    stream << "GIMP Palette\nName: " << colorSet->name() << "\nColumns: " << colorSet->columnCount() << "\n#\n";

    KisSwatchGroupSP global = colorSet->getGlobalGroup();
    for (int y = 0; y < global->rowCount(); y++) {
        for (int x = 0; x < colorSet->columnCount(); x++) {
            if (!global->checkSwatchExists(x, y)) {
                continue;
            }
            const KisSwatch& entry = global->getSwatch(x, y);
            PkColor c = entry.color().toQColor();
            stream << c.red() << " " << c.green() << " " << c.blue() << "\t";
            if (entry.name().isEmpty())
                stream << "Untitled\n";
            else
                stream << entry.name() << "\n";
        }
    }

    return true;
}

bool KoColorSet::Private::loadGpl()
{
    if (data.isEmpty() || data.size() < 50) {
        warnPigment << "Illegal Gimp palette file: " << colorSet->filename();
        return false;
    }

    quint32 index = 0;

    PkStringList lines = pkReadAllLinesSafe(data);

    if (lines.size() < 3) {
        warnPigment << "Not enough lines in palette file: " << colorSet->filename();
        return false;
    }

    PkString columnsText;
    qint32 r, g, b;
    KisSwatch swatch;

    // Read name
    if (!lines[0].startsWith("GIMP") || !lines[1].toLower().contains("name")) {
        warnPigment << "Illegal Gimp palette file: " << colorSet->filename();
        return false;
    }

    // translated name will be in a tooltip, here don't translate
    colorSet->setName(lines[1].split(u':')[1].trimmed());

    index = 2;

    // Read columns
    int columns = 0;
    if (lines[index].toLower().contains("columns")) {
        columnsText = lines[index].split(u':')[1].trimmed();
        columns = columnsText.toInt();
        if (columns > MAXIMUM_ALLOWED_COLUMNS) {
            warnPigment << "Refusing to set unreasonable number of columns (" << columns << ") in GIMP Palette file " << colorSet->filename() << " - using maximum number of allowed columns instead";
            global()->setColumnCount(MAXIMUM_ALLOWED_COLUMNS);
        }
        else {
            global()->setColumnCount(columns);
        }
        index = 3;
    }


    for (qint32 i = index; i < static_cast<qint32>(lines.size()); i++) {
        if (lines[i].startsWith("#")) {
            comment += lines[i].mid(1).trimmed() + PkString(" ");
        } else if (!lines[i].isEmpty()) {
            PkStringList a = pkSplitSkipEmpty(pkReplaceChar(lines[i], u'\t', u' '), u' ');

            if (a.count() < 3) {
                continue;
            }

            r = pkBound(0, a[0].toInt(), 255);
            g = pkBound(0, a[1].toInt(), 255);
            b = pkBound(0, a[2].toInt(), 255);

            swatch.setColor(KoColor(PkColor(r, g, b), KoColorSpaceRegistry::instance()->rgb8()));

            for (int i = 0; i != 3; i++) {
                a.pop_front();
            }
            PkString name = a.join(" ");
            swatch.setName(name.isEmpty() || name == PkString("Untitled") ? PkString("Untitled") : name);

            global()->addSwatch(swatch);
        }
    }
    return true;
}

bool KoColorSet::Private::loadAct()
{
    colorSet->setName(pkFileCompleteBaseName(colorSet->filename()));
    KisSwatch swatch;
    int numOfTriplets = int(data.size() / 3);
    for (int i = 0; i < numOfTriplets * 3; i += 3) {
        quint8 r = static_cast<quint8>(data.constData()[i]);
        quint8 g = static_cast<quint8>(data.constData()[i+1]);
        quint8 b = static_cast<quint8>(data.constData()[i+2]);
        swatch.setColor(KoColor(PkColor(r, g, b), KoColorSpaceRegistry::instance()->rgb8()));
        global()->addSwatch(swatch);
    }
    return true;
}

bool KoColorSet::Private::loadRiff()
{
    // https://worms2d.info/Palette_file
    colorSet->setName(pkFileCompleteBaseName(colorSet->filename()));
    KisSwatch swatch;

    RiffHeader header;
    if (data.size() < static_cast<int>(sizeof(RiffHeader))) {
        warnPigment << "Illegal RIFF palette file: " << colorSet->filename();
        return false;
    }
    std::memcpy(&header, data.constData(), sizeof(RiffHeader));
    header.colorcount = pkFromBigEndian16(header.colorcount);

    for (int i = sizeof(RiffHeader);
         (i < (int)(sizeof(RiffHeader) + (header.colorcount * 4)) && i < data.size());
         i += 4) {
        quint8 r = static_cast<quint8>(data.constData()[i]);
        quint8 g = static_cast<quint8>(data.constData()[i+1]);
        quint8 b = static_cast<quint8>(data.constData()[i+2]);
        swatch.setColor(KoColor(PkColor(r, g, b), KoColorSpaceRegistry::instance()->rgb8()));
        colorSet->getGlobalGroup()->addSwatch(swatch);
    }
    return true;
}


bool KoColorSet::Private::loadPsp()
{
    colorSet->setName(pkFileCompleteBaseName(colorSet->filename()));
    KisSwatch swatch;
    qint32 r, g, b;

    PkStringList l = pkReadAllLinesSafe(data);
    if (l.size() < 4) return false;
    if (l[0] != PkString("JASC-PAL")) return false;
    if (l[1] != PkString("0100")) return false;

    int entries = l[2].toInt();

    KisSwatchGroupSP global = colorSet->getGlobalGroup();

    for (int i = 0; i < entries; ++i)  {

        PkStringList a = pkSplitSkipEmpty(pkReplaceChar(l[i + 3], u'\t', u' '), u' ');

        if (a.count() != 3) {
            continue;
        }

        r = pkBound(0, a[0].toInt(), 255);
        g = pkBound(0, a[1].toInt(), 255);
        b = pkBound(0, a[2].toInt(), 255);

        swatch.setColor(KoColor(PkColor(r, g, b),
                                KoColorSpaceRegistry::instance()->rgb8()));

        PkString name = a.join(" ");
        swatch.setName(name.isEmpty() ? PkString("Untitled") : name);

        global->addSwatch(swatch);
    }
    return true;
}

bool KoColorSet::Private::loadCss()
{
    colorSet->setName(pkFileCompleteBaseName(colorSet->filename()));

    PkString text = pkReadAllLinesSafe(data).join(PkString());
    text = pkRemoveSubstr(text, PkString("\t"));
    text = pkRemoveSubstr(text, PkString(" "));

    text = pkRemoveCComments(text); // Remove comments

    KisSwatch swatch;

    std::vector<CssColorMatch> colors = pkCssGlobalMatch(text);

    if (colors.empty()) {
        warnPigment << "No color found in CSS palette : " << colorSet->filename();
        return false;
    }

    for (const CssColorMatch &match : colors) {
        PkString colorInfo = match.colorInfo;
        PkString colorName = match.colorName;
        PkString colorValue = match.colorValue;

        if (!colorInfo.startsWith(".") || colorValue.isEmpty()) {
            warnPigment << "Illegal CSS palette syntax : " << colorInfo;
            return false;
        }

        PkColor qColor;

        colorName = pkRemoveSubstr(colorName, PkString("."));
        swatch.setName(colorName);

        if (colorValue.startsWith("rgb")) {
            PkStringList color;

            if (colorValue.startsWith("rgba")) {
                colorValue = pkRemoveSubstr(pkRemoveSubstr(colorValue, PkString("rgba(")), PkString(")"));
                color = pkSplit(colorValue, u',');

                if (color.size() != 4) {
                    warnPigment << "Invalid RGBA color definition : " << colorInfo;
                    return false;
                }

                int alpha = static_cast<int>(pkToFloat(color[3], nullptr) * 255);

                if (alpha < 0 || alpha > 255) {
                    warnPigment << "Invalid alpha parameter : " << colorInfo;
                    return false;
                }
            }
            else {
                colorValue = pkRemoveSubstr(pkRemoveSubstr(colorValue, PkString("rgb(")), PkString(")"));

                color = pkSplit(colorValue, u',');

                if (color.size() != 3) {
                    warnPigment << "Invalid RGB color definition : " << colorInfo;
                    return false;
                }
            }

            int rgb[3];

            for (int i = 0; i < 3; i++) {
                if (pkEndsWith(color[i], PkString("%"))) {
                    color[i] = pkRemoveSubstr(color[i], PkString("%"));
                    rgb[i] = static_cast<int>(pkToFloat(color[i], nullptr) / 100 * 255);
                }
                else {
                    rgb[i] = color[i].toInt();
                };
            }

            qColor = PkColor(rgb[0], rgb[1], rgb[2]);
        }
        else if (colorValue.startsWith("hsl")) {
            PkStringList color;

            if (colorValue.startsWith("hsla")) {
                colorValue = pkRemoveSubstr(pkRemoveSubstr(pkRemoveSubstr(colorValue, PkString("hsla(")), PkString(")")), PkString("%"));
                color = pkSplit(colorValue, u',');
                if (color.size() != 4) {
                    warnPigment << "Invalid HSLA color definition : " << colorInfo;
                    return false;
                }

                float alpha = pkToFloat(color[3], nullptr);

                if (alpha < 0.0 || alpha > 1.0) {
                    warnPigment << "Invalid alpha parameter : " << colorInfo;
                    return false;
                }

            }
            else {
                colorValue = pkRemoveSubstr(pkRemoveSubstr(pkRemoveSubstr(colorValue, PkString("hsl(")), PkString(")")), PkString("%"));
                color = pkSplit(colorValue, u',');
                if (color.size() != 3) {
                    warnPigment << "Invalid HSL color definition : " << colorInfo;
                    return false;
                }
            }

            float hue = pkToFloat(color[0], nullptr) / 359;
            float saturation = pkToFloat(color[1], nullptr) / 100;
            float lightness = pkToFloat(color[2], nullptr) / 100;

            if (hue < 0.0 || hue > 1.0) {
                warnPigment << "Invalid hue parameter : " << colorInfo;
                return false;
            }

            if (saturation < 0.0 || saturation > 1.0) {
                warnPigment << "Invalid saturation parameter : " << colorInfo;
                return false;
            }

            if (lightness < 0.0 || lightness > 1.0) {
                warnPigment << "Invalid lightness parameter : " << colorInfo;
                return false;
            }

            qColor = PkColor::fromHslF(hue, saturation, lightness);

        }
        else if (colorValue.startsWith("#")) {
            if (colorValue.size() == 9) {
                // Convert the CSS format #RRGGBBAA to #RRGGBB
                // Due to Qt 颜色 8 位格式为 #AARRGGBB 且这里不载入 alpha 通道
                colorValue = colorValue.left(7);
            }

            qColor = PkColor(colorValue);
        }
        else {
            warnPigment << "Unknown color declaration : " << colorInfo;
            return false;
        }

        if (!qColor.isValid()) {
            warnPigment << "Invalid color definition : " << colorInfo;
            return false;
        }

        swatch.setColor(KoColor(qColor, KoColorSpaceRegistry::instance()->rgb8()));

        global()->addSwatch(swatch);
    }

    return true;
}

const KoColorProfile *KoColorSet::Private::loadColorProfile(std::unique_ptr<KoStore> &store,
                                                            const PkString &path,
                                                            const PkString &modelId,
                                                            const PkString &colorDepthId)
{
    if (!store->open(path)) {
        return nullptr;
    }

    PkByteArray bytes = store->read(store->size());
    store->close();

    const KoColorProfile *profile = KoColorSpaceRegistry::instance()
        ->createColorProfile(modelId, colorDepthId, bytes);
    if (!profile || !profile->valid()) {
        return nullptr;
    }

    KoColorSpaceRegistry::instance()->addProfile(profile);
    return profile;
}

bool KoColorSet::Private::loadKplProfiles(std::unique_ptr<KoStore> &store)
{
    if (!store->open("profiles.xml")) {
        return false;
    }

    PkByteArray bytes = store->read(store->size());
    store->close();

    PkXmlDocument doc;
    if(!doc.setContent(bytes)) {
        return false;
    }

    PkXmlElement root = doc.documentElement();
    for (PkXmlElement c = root.firstChildElement(KPL_PALETTE_PROFILE_TAG);
         !c.isNull();
         c = c.nextSiblingElement(KPL_PALETTE_PROFILE_TAG)) {
        PkString name         = c.attribute(KPL_PALETTE_NAME_ATTR);
        PkString filename     = c.attribute(KPL_PALETTE_FILENAME_ATTR);
        PkString colorModelId = c.attribute(KPL_COLOR_MODEL_ID_ATTR);
        PkString colorDepthId = c.attribute(KPL_COLOR_DEPTH_ID_ATTR);

        if (KoColorSpaceRegistry::instance()->profileByName(name)) {
            continue;
        }

        loadColorProfile(store, filename, colorModelId, colorDepthId);
        // TODO: What should happen if this fails?
    }

    return true;
}

bool KoColorSet::Private::loadKplColorset(std::unique_ptr<KoStore> &store)
{
    if (!store->open("colorset.xml")) {
        return false;
    }

    PkByteArray bytes = store->read(store->size());
    store->close();

    PkXmlDocument doc;
    if (!doc.setContent(bytes)) {
        return false;
    }

    PkXmlElement root = doc.documentElement();
    colorSet->setName(root.attribute(KPL_PALETTE_NAME_ATTR));
    PkString version = root.attribute(KPL_VERSION_ATTR);
    comment         = root.attribute(KPL_PALETTE_COMMENT_ATTR);

    int desiredColumnCount = root.attribute(KPL_PALETTE_COLUMN_COUNT_ATTR).toInt();
    if (desiredColumnCount > MAXIMUM_ALLOWED_COLUMNS) {
        warnPigment << "Refusing to set unreasonable number of columns (" << desiredColumnCount
                    << ") in KPL palette file " << colorSet->filename()
                    << " - setting maximum allowed column count instead.";
        colorSet->setColumnCount(MAXIMUM_ALLOWED_COLUMNS);
    } else {
        colorSet->setColumnCount(desiredColumnCount);
    }

    loadKplGroup(doc, root, colorSet->getGlobalGroup(), version);

    for (PkXmlElement g = root.firstChildElement(KPL_GROUP_TAG);
         !g.isNull();
         g = g.nextSiblingElement(KPL_GROUP_TAG)) {
        PkString groupName = g.attribute(KPL_GROUP_NAME_ATTR);
        colorSet->addGroup(groupName);
        loadKplGroup(doc, g, colorSet->getGroup(groupName), version);
    }

    return true;
}

bool KoColorSet::Private::loadKpl()
{
    PkMemoryStream buf;
    pkSeedReadStream(&buf, data);

    std::unique_ptr<KoStore> store(
        KoStore::createStore(&buf, KoStore::Read,
                             pkMimeType("application/x-krita-palette"),
                             KoStore::Zip));
    if (!store || store->bad()) {
        return false;
    }

    if (store->hasFile("profiles.xml") && !loadKplProfiles(store)) {
        return false;
    }

    if (!loadKplColorset(store)) {
        return false;
    }

    buf.close();
    return true;
}

bool KoColorSet::Private::loadAco()
{
    colorSet->setName(pkFileCompleteBaseName(colorSet->filename()));

    PkMemoryStream buf;
    pkSeedReadStream(&buf, data);

    quint16 version = readShort(&buf);
    quint16 numColors = readShort(&buf);
    KisSwatch swatch;

    if (version == 1 && buf.size() > 4+numColors*10) {
        buf.seek(4+numColors*10);
        version = readShort(&buf);
        numColors = readShort(&buf);
    }

    const quint16 quint16_MAX = 65535;

    KisSwatchGroupSP group = colorSet->getGlobalGroup();

    for (int i = 0; i < numColors && !buf.atEnd(); ++i) {

        quint16 colorSpace = readShort(&buf);
        quint16 ch1 = readShort(&buf);
        quint16 ch2 = readShort(&buf);
        quint16 ch3 = readShort(&buf);
        quint16 ch4 = readShort(&buf);

        bool skip = false;
        if (colorSpace == 0) { // RGB
            const KoColorProfile *srgb = KoColorSpaceRegistry::instance()->rgb8()->profile();
            KoColor c(KoColorSpaceRegistry::instance()->rgb16(srgb));
            reinterpret_cast<quint16*>(c.data())[0] = ch3;
            reinterpret_cast<quint16*>(c.data())[1] = ch2;
            reinterpret_cast<quint16*>(c.data())[2] = ch1;
            c.setOpacity(OPACITY_OPAQUE_U8);
            swatch.setColor(c);
        }
        else if (colorSpace == 1) { // HSB
            PkColor qc;
            qc.setHsvF(ch1 / 65536.0, ch2 / 65536.0, ch3 / 65536.0);
            KoColor c(qc, KoColorSpaceRegistry::instance()->rgb16());
            c.setOpacity(OPACITY_OPAQUE_U8);
            swatch.setColor(c);
        }
        else if (colorSpace == 2) { // CMYK
            KoColor c(KoColorSpaceRegistry::instance()->colorSpace(CMYKAColorModelID.id(), Integer16BitsColorDepthID.id(), PkString()));
            reinterpret_cast<quint16*>(c.data())[0] = quint16_MAX - ch1;
            reinterpret_cast<quint16*>(c.data())[1] = quint16_MAX - ch2;
            reinterpret_cast<quint16*>(c.data())[2] = quint16_MAX - ch3;
            reinterpret_cast<quint16*>(c.data())[3] = quint16_MAX - ch4;
            c.setOpacity(OPACITY_OPAQUE_U8);
            swatch.setColor(c);
        }
        else if (colorSpace == 7) { // LAB
            KoColor c = KoColor(KoColorSpaceRegistry::instance()->lab16());
            reinterpret_cast<quint16*>(c.data())[0] = ch3;
            reinterpret_cast<quint16*>(c.data())[1] = ch2;
            reinterpret_cast<quint16*>(c.data())[2] = ch1;
            c.setOpacity(OPACITY_OPAQUE_U8);
            swatch.setColor(c);
        }
        else if (colorSpace == 8) { // GRAY
            KoColor c(KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), PkString()));
            reinterpret_cast<quint16*>(c.data())[0] = ch1 * (quint16_MAX / 10000);
            c.setOpacity(OPACITY_OPAQUE_U8);
            swatch.setColor(c);
        }
        else {
            warnPigment << "Unsupported colorspace in palette" << colorSet->filename() << "(" << colorSpace << ")";
            skip = true;
        }

        if (version == 2) {
            PkString name = readUnicodeString(&buf, true);
            swatch.setName(name);
        }
        if (!skip) {
            group->addSwatch(swatch);
        }
    }
    return true;
}

bool KoColorSet::Private::loadSbzSwatchbook(std::unique_ptr<KoStore> &store)
{
    if (!store->open("swatchbook.xml")) {
        return false;
    }

    PkByteArray bytes = store->read(store->size());
    store->close();

    dbgPigment << "XML palette: " << colorSet->filename() << ", SwatchBooker format";

    PkXmlDocument doc;
    int errorLine, errorColumn;
    PkString errorMessage;
    if (!doc.setContent(bytes, &errorMessage, &errorLine, &errorColumn)) {
        warnPigment << "Illegal XML palette:" << colorSet->filename();
        warnPigment << "Error (line" << errorLine
                    << ", column" << errorColumn
                    << "):" << errorMessage;
        return false;
    }

    PkXmlElement root = doc.documentElement(); // SwatchBook

    // Start reading properties...
    PkXmlElement metadata = root.firstChildElement("metadata");
    if (metadata.isNull()) {
        warnPigment << "Palette metadata not found";
        return false;
    }

    PkXmlElement title = metadata.firstChildElement("dc:title");
    PkString colorName = title.text();
    colorName = colorName.isEmpty() ? PkString("Untitled") : colorName;
    colorSet->setName(colorName);
    dbgPigment << "Processed name of palette:" << colorSet->name();
    // End reading properties

    // Also read the swatch book...
    PkXmlElement book = root.firstChildElement("book");
    if (book.isNull()) {
        warnPigment << "Palette book (swatch composition) not found (line" << root.lineNumber()
                    << ", column" << root.columnNumber()
                    << ")";
        return false;
    }

    // Which has lots of "swatch"es (todo: support groups)
    PkXmlElement swatch = book.firstChildElement();
    if (swatch.isNull()) {
        warnPigment << "Swatches/groups definition not found (line" << book.lineNumber()
                    << ", column" << book.columnNumber()
                    << ")";
        return false;
    }

    // Now read colors...
    PkXmlElement materials = root.firstChildElement("materials");
    if (materials.isNull()) {
        warnPigment << "Materials (color definitions) not found";
        return false;
    }

    // This one has lots of "color" elements
    if (materials.firstChildElement("color").isNull()) {
        warnPigment << "Color definitions not found (line" << materials.lineNumber()
                    << ", column" << materials.columnNumber()
                    << ")";
        return false;
    }

    // We'll store colors here, and as we process swatches
    // we'll add them to the palette
    PkHash<PkString, KisSwatch> materialsBook;
    PkHash<PkString, const KoColorSpace*> fileColorSpaces;

    // Color processing
    store->enterDirectory("profiles"); // Color profiles (icc files) live here
    for (PkXmlElement colorElement = materials.firstChildElement("color");
         !colorElement.isNull();
         colorElement = colorElement.nextSiblingElement("color")) {
        KisSwatch currentEntry;
        // Set if color is spot
        currentEntry.setSpotColor(colorElement.attribute("usage") == "spot");

        // <metadata> inside contains id and name
        // one or more <values> define the color
        PkXmlElement currentColorMetadata = colorElement.firstChildElement("metadata");
        // Get color name
        PkXmlElement colorTitle = currentColorMetadata.firstChildElement("dc:title");
        PkXmlElement colorId = currentColorMetadata.firstChildElement("dc:identifier");
        // Is there an id? (we need that at the very least for identifying a color)
        if (colorId.text().isEmpty()) {
            warnPigment << "Unidentified color (line" << colorId.lineNumber()
                        << ", column" << colorId.columnNumber()
                        << ")";
            return false;
        }

        if (materialsBook.contains(colorId.text())) {
            warnPigment << "Duplicated color definition (line" << colorId.lineNumber()
                        << ", column" << colorId.columnNumber()
                        << ")";
            return false;
        }

        // Get a valid color name
        currentEntry.setId(colorId.text());
        currentEntry.setName(colorTitle.text().isEmpty() ? colorId.text() : colorTitle.text());

        // Get a valid color definition
        if (colorElement.firstChildElement("values").isNull()) {
            warnPigment << "Color definitions not found (line" << colorElement.lineNumber()
                        << ", column" << colorElement.columnNumber()
                        << ")";
            return false;
        }

        bool status;
        bool firstDefinition = false;
        KoColorSpaceRegistry *colorSpaceRegistry = KoColorSpaceRegistry::instance();
        const PkString colorDepthId = Float32BitsColorDepthID.id();
        // Priority: Lab, otherwise the first definition found
        for (PkXmlElement colorValueE = colorElement.firstChildElement("values");
             !colorValueE.isNull();
             colorValueE = colorValueE.nextSiblingElement("values")) {
            PkString model = colorValueE.attribute("model");

            PkString modelId;
            const KoColorProfile *profile = nullptr;
            if (model == "Lab") {
                modelId = LABAColorModelID.id();
            } else if (model == "sRGB") {
                modelId = RGBAColorModelID.id();
                profile = colorSpaceRegistry->rgb8()->profile();
            } else if (model == "XYZ") {
                modelId = XYZAColorModelID.id();
            } else if (model == "CMYK") {
                modelId = CMYKAColorModelID.id();
            } else if (model == "GRAY") {
                modelId = GrayAColorModelID.id();
            } else if (model == "RGB") {
                modelId = RGBAColorModelID.id();
            } else {
                warnPigment << "Color space not implemented:" << model
                            << "(line" << colorValueE.lineNumber()
                            << ", column "<< colorValueE.columnNumber()
                            << ")";
                continue;
            }

            const KoColorSpace *colorSpace = colorSpaceRegistry->colorSpace(modelId, colorDepthId, profile);

            // The 'space' attribute is the name of the icc file
            // sitting in the 'profiles' directory in the zip.
            PkString space = colorValueE.attribute("space");
            if (!space.isEmpty()) {
                if (fileColorSpaces.contains(space)) {
                    colorSpace = fileColorSpaces.value(space);
                } else {
                    // Try loading the profile and add it to the registry
                    profile = loadColorProfile(store, space, modelId, colorDepthId);
                    if (profile) {
                        colorSpace = colorSpaceRegistry->colorSpace(modelId, colorDepthId, profile);
                        fileColorSpaces.insert(space, colorSpace);
                    }
                }
            }

            KoColor c(colorSpace);

            // sRGB,RGB,HSV,HSL,CMY,CMYK,nCLR: 0 -> 1
            // YIQ: Y 0 -> 1 : IQ -0.5 -> 0.5
            // Lab: L 0 -> 100 : ab -128 -> 127
            // XYZ: 0 -> ~100
            PkVector<float> channels;
            for (const PkString &str : colorValueE.text().split(u' ')) {
                float channelValue = pkToFloat(str, &status);
                if (!status) {
                    warnPigment << "Invalid float definition (line" << colorValueE.lineNumber()
                                << ", column" << colorValueE.columnNumber()
                                << ")";

                    channelValue = 0;
                }

                channels.append(channelValue);
            }
            channels.append(OPACITY_OPAQUE_F); // Alpha channel
            colorSpace->fromNormalisedChannelsValue(c.data(), channels);

            currentEntry.setColor(c);
            firstDefinition = true;

            if (model == "Lab") {
                break; // Immediately add this one
            }
        }

        if (firstDefinition) {
            materialsBook.insert(currentEntry.id(), currentEntry);
        } else {
            warnPigment << "No supported color  spaces for the current color (line" << colorElement.lineNumber()
                        << ", column "<< colorElement.columnNumber()
                        << ")";
            return false;
        }
    }

    store->leaveDirectory(); // Return to root
    // End colors
    // Now decide which ones will go into the palette

    KisSwatchGroupSP global = colorSet->getGlobalGroup();
    for(; !swatch.isNull(); swatch = swatch.nextSiblingElement()) {
        PkString type = swatch.tagName();
        if (type.isEmpty()) {
            warnPigment << "Invalid swatch/group definition (no id) (line" << swatch.lineNumber()
                        << ", column" << swatch.columnNumber()
                        << ")";
            return false;
        } else if (type == "swatch") {
            PkString id = swatch.attribute("material");
            if (id.isEmpty()) {
                warnPigment << "Invalid swatch definition (no material id) (line" << swatch.lineNumber()
                            << ", column" << swatch.columnNumber()
                            << ")";
                return false;
            }

            if (materialsBook.contains(id)) {
                global->addSwatch(materialsBook.value(id));
            } else {
                warnPigment << "Invalid swatch definition (material not found) (line" << swatch.lineNumber()
                            << ", column" << swatch.columnNumber()
                            << ")";
                return false;
            }
        } else if (type == "group") {
            PkXmlElement groupMetadata = swatch.firstChildElement("metadata");
            if (groupMetadata.isNull()) {
                warnPigment << "Invalid group definition (missing metadata) (line" << groupMetadata.lineNumber()
                            << ", column" << groupMetadata.columnNumber()
                            << ")";
                return false;
            }
            // 保留原实现行为：这里读的是调色板级 metadata 的 dc:title，
            // 不是本组的 groupMetadata（原 Krita 就是如此）。
            PkXmlElement groupTitle = metadata.firstChildElement("dc:title");
            if (groupTitle.isNull()) {
                warnPigment << "Invalid group definition (missing title) (line" << groupTitle.lineNumber()
                            << ", column" << groupTitle.columnNumber()
                            << ")";
                return false;
            }
            PkString currentGroupName = groupTitle.text();
            colorSet->addGroup(currentGroupName);

            for (PkXmlElement groupSwatch = swatch.firstChildElement("swatch");
                 !groupSwatch.isNull();
                 groupSwatch = groupSwatch.nextSiblingElement("swatch")) {
                PkString id = groupSwatch.attribute("material");
                if (id.isEmpty()) {
                    warnPigment << "Invalid swatch definition (no material id) (line" << groupSwatch.lineNumber()
                                << ", column" << groupSwatch.columnNumber()
                                << ")";
                    return false;
                }

                if (materialsBook.contains(id)) {
                    colorSet->getGroup(currentGroupName)->addSwatch(materialsBook.value(id));
                } else {
                    warnPigment << "Invalid swatch definition (material not found) (line" << groupSwatch.lineNumber()
                                << ", column" << groupSwatch.columnNumber()
                                << ")";
                    return false;
                }
            }
        }
    }
    // End palette

    return true;
}

bool KoColorSet::Private::loadSbz() {
    PkMemoryStream buf;
    pkSeedReadStream(&buf, data);

    // &buf is a subclass of PkStream
    std::unique_ptr<KoStore> store(
        KoStore::createStore(&buf, KoStore::Read,
                             pkMimeType("application/x-swatchbook"),
                             KoStore::Zip));
    if (!store || store->bad()) {
        return false;
    }

    if (store->hasFile("swatchbook.xml") && !loadSbzSwatchbook(store)) {
        return false;
    }

    buf.close();
    return true;
}

bool KoColorSet::Private::loadAse()
{
    colorSet->setName(pkFileCompleteBaseName(colorSet->filename()));

    PkMemoryStream buf;
    pkSeedReadStream(&buf, data);

    PkByteArray signature = pkReadBytes(&buf, 4); // should be "ASEF";
    quint16 version = readShort(&buf);
    quint16 version2 = readShort(&buf);

    if (!pkByteEquals(signature, "ASEF") && version!= 1 && version2 != 0) {
        PkString sigStr = PkString::PkFromUtf8(signature.constData(), signature.size());
        warnPigment << "incorrect header:" << sigStr << version << version2;
        return false;
    }
    qint32 numBlocks = readInt(&buf);

    PkByteArray groupStart("\xC0\x01", 2);
    PkByteArray groupEnd("\xC0\x02", 2);
    PkByteArray swatchSig("\x00\x01", 2);
    Q_UNUSED(swatchSig);

    bool inGroup = false;
    PkString groupName;
    for (qint32 i = 0; i < numBlocks; i++) {
        PkByteArray blockType = pkReadBytes(&buf, 2);
        qint32 blockSize = readInt(&buf);
        PkStream::pk_int64 pos = buf.pos();

        if (blockType == groupStart) {
            groupName = readUnicodeString(&buf);
            colorSet->addGroup(groupName);
            inGroup = true;
        }
        else if (blockType == groupEnd) {
            int colorCount = colorSet->getGroup(groupName)->colorCount();
            int columns = colorSet->columnCount();
            int rows = colorCount/columns;
            if (colorCount % columns > 0) {
                rows += 1;
            }
            colorSet->getGroup(groupName)->setRowCount(rows);
            inGroup = false;
        }
        else /* if (blockType == swatchSig)*/ {
            KisSwatch swatch;
            swatch.setName(readUnicodeString(&buf).trimmed());
            PkXmlDocument doc;
            PkByteArray colorModel = pkReadBytes(&buf, 4);
            if (pkByteEquals(colorModel, "RGB ")) {
                PkXmlElement elt = doc.createElement("sRGB");

                elt.setAttribute("r", KisDomUtils::toString(readFloat(&buf)));
                elt.setAttribute("g", KisDomUtils::toString(readFloat(&buf)));
                elt.setAttribute("b", KisDomUtils::toString(readFloat(&buf)));

                KoColor color = KoColor::fromXML(elt, "U8");
                swatch.setColor(color);
            } else if (pkByteEquals(colorModel, "CMYK")) {
                PkXmlElement elt = doc.createElement("CMYK");

                elt.setAttribute("c", KisDomUtils::toString(readFloat(&buf)));
                elt.setAttribute("m", KisDomUtils::toString(readFloat(&buf)));
                elt.setAttribute("y", KisDomUtils::toString(readFloat(&buf)));
                elt.setAttribute("k", KisDomUtils::toString(readFloat(&buf)));
                //try to select the default PS icc profile if possible.
                elt.setAttribute("space", "U.S. Web Coated (SWOP) v2");

                KoColor color = KoColor::fromXML(elt, "U8");
                swatch.setColor(color);
            } else if (pkByteEquals(colorModel, "LAB ")) {
                PkXmlElement elt = doc.createElement("Lab");

                elt.setAttribute("L", KisDomUtils::toString(readFloat(&buf)*100.0));
                elt.setAttribute("a", KisDomUtils::toString(readFloat(&buf)));
                elt.setAttribute("b", KisDomUtils::toString(readFloat(&buf)));

                KoColor color = KoColor::fromXML(elt, "U16");
                swatch.setColor(color);
            } else if (pkByteEquals(colorModel, "GRAY")) {
                PkXmlElement elt = doc.createElement("Gray");

                elt.setAttribute("g", KisDomUtils::toString(readFloat(&buf)));

                KoColor color = KoColor::fromXML(elt, "U8");
                swatch.setColor(color);
            }
            quint16 type = readShort(&buf);
            if (type == 1) { //0 is global, 2 is regular;
                swatch.setSpotColor(true);
            }
            if (inGroup) {
                colorSet->addSwatch(swatch, groupName);
            } else {
                colorSet->addSwatch(swatch);
            }
        }
        buf.seek(pos + qint64(blockSize));
    }
    return true;
}

bool KoColorSet::Private::loadAcb()
{
    PkMemoryStream buf;
    pkSeedReadStream(&buf, data);

    PkByteArray signature = pkReadBytes(&buf, 4); // should be "8BCB";
    quint16 version = readShort(&buf);
    quint16 bookID = readShort(&buf);
    Q_UNUSED(bookID);

    if (!pkByteEquals(signature, "8BCB") && version!= 1) {
        return false;
    }

    PkStringList metadata;
    for (int i = 0; i< 4; i++) {

        PkString metadataString = readUnicodeString(&buf, true);
        if (metadataString.startsWith(PkString("\""))) {
            metadataString = metadataString.mid(1);
        }
        if (pkEndsWith(metadataString, PkString("\""))) {
            metadataString = metadataString.left(metadataString.size() - 1);
        }
        if (metadataString.startsWith(PkString("$$$/"))) {
            if (metadataString.contains(PkString("="))) {
                metadataString = metadataString.split(u'=').back();
            } else {
                metadataString = PkString();
            }
        }
        metadata.append(metadataString);
    }
    PkString title = metadata.at(0);
    colorSet->setName(title);
    PkString prefix = metadata.at(1);
    PkString postfix = metadata.at(2);
    PkString description = metadata.at(3);
    colorSet->setComment(description);

    quint16 numColors = readShort(&buf);
    quint16 numColumns = readShort(&buf);
    numColumns = numColumns > 0 ? numColumns : 8; // overwrite with sane default in case of 0
    colorSet->setColumnCount(numColumns);
    quint16 numKeyColorPage = readShort(&buf);
    Q_UNUSED(numKeyColorPage);
    quint16 colorType = readShort(&buf);

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    if (colorType == 2) {
        PkString profileName = "U.S. Web Coated (SWOP) v2";
        cs = KoColorSpaceRegistry::instance()->colorSpace(CMYKAColorModelID.id(), Integer8BitsColorDepthID.id(), profileName);
    } else if (colorType == 7) {
        cs = KoColorSpaceRegistry::instance()->colorSpace(LABAColorModelID.id(), Integer8BitsColorDepthID.id(), PkString());
    }

    for (quint16 i = 0; i < numColors; i++) {
        KisSwatch swatch;
        PkStringList name;
        name << prefix;
        name << readUnicodeString(&buf, true);
        name << postfix;
        swatch.setName(name.join(" ").trimmed());
        PkByteArray key = pkReadBytes(&buf, 6); // should be "8BCB";
        swatch.setId(pkFromLatin1(key.constData(), key.size()));
        swatch.setSpotColor(true);
        quint8 c1 = readByte(&buf);
        quint8 c2 = readByte(&buf);
        quint8 c3 = readByte(&buf);
        KoColor c(cs);
        if (colorType == 0) {
            c.data()[0] = c3;
            c.data()[1] = c2;
            c.data()[2] = c1;
        } else if (colorType == 2) {
            quint8 c4 = readByte(&buf);
            c.data()[0] = c1;
            c.data()[1] = c2;
            c.data()[2] = c3;
            c.data()[3] = c4;
        } else if (colorType == 7) {
            c.data()[0] = c1;
            c.data()[1] = c2;
            c.data()[2] = c3;
        }
        c.setOpacity(1.0);
        swatch.setColor(c);
        colorSet->addSwatch(swatch);
    }

    return true;
}

bool KoColorSet::Private::loadXml() {
    bool res = false;

    PkXmlStreamReader xml(PkString::PkFromUtf8(data.constData(), data.size()));

    if (xml.readNextStartElement()) {
        PkString paletteId = xml.name();
        if (paletteId.toLower() == PkString("scribuscolors")) { // Scribus
            dbgPigment << "XML palette: " << colorSet->filename() << ", Scribus format";
            res = loadScribusXmlPalette(colorSet, &xml);
        }
        else {
            // Unknown XML format
            xml.raiseError(PkString("Unknown XML palette format. Expected SCRIBUSCOLORS, found ") + paletteId);
        }
    }

    // If there is any error (it should be returned through the stream)
    if (xml.hasError() || !res) {
        warnPigment << "Illegal XML palette:" << colorSet->filename();
        warnPigment << "Error (line"<< xml.lineNumber() << ", column" << xml.columnNumber() << "):" << xml.errorString();
        return false;
    }
    else {
        dbgPigment << "XML palette parsed successfully:" << colorSet->filename();
        return true;
    }
}

bool KoColorSet::Private::saveKpl(PkStream *dev) const
{
    std::unique_ptr<KoStore> store(KoStore::createStore(dev, KoStore::Write, pkMimeType("application/x-krita-palette"), KoStore::Zip));
    if (!store || store->bad()) {
        warnPigment << "saveKpl could not create store";
        return false;
    }

    PkSet<const KoColorSpace *> colorSpaces;

    {
        PkXmlDocument doc;
        PkXmlElement root = doc.createElement(KPL_PALETTE_TAG);
        root.setAttribute(KPL_VERSION_ATTR, "2.0");
        root.setAttribute(KPL_PALETTE_NAME_ATTR, colorSet->name());
        root.setAttribute(KPL_PALETTE_COMMENT_ATTR, comment);
        root.setAttribute(KPL_PALETTE_COLUMN_COUNT_ATTR, KisDomUtils::toString(colorSet->columnCount()));
        root.setAttribute(KPL_GROUP_ROW_COUNT_ATTR, KisDomUtils::toString(colorSet->getGlobalGroup()->rowCount()));

        saveKplGroup(doc, root, colorSet->getGroup(KoColorSet::GLOBAL_GROUP_NAME), colorSpaces);

        for (const KisSwatchGroupSP &group : swatchGroups) {
            if (group->name() == KoColorSet::GLOBAL_GROUP_NAME) { continue; }
            PkXmlElement gl = doc.createElement(KPL_GROUP_TAG);
            gl.setAttribute(KPL_GROUP_NAME_ATTR, group->name());
            root.appendChild(gl);
            saveKplGroup(doc, gl, group, colorSpaces);
        }

        doc.appendChild(root);
        if (!store->open("colorset.xml")) { return false; }
        PkString xmlStr = doc.toByteArray();
        std::string utf8 = xmlStr.PkToUtf8();
        PkByteArray ba(utf8.data(), (int)utf8.size());
        if (store->write(ba) != ba.size()) { return false; }
        if (!store->close()) { return false; }
    }

    PkXmlDocument doc;
    PkXmlElement profileElement = doc.createElement("Profiles");

    for (const KoColorSpace *colorSpace : colorSpaces) {
        PkString fn = pkFileName(colorSpace->profile()->fileName());
        if (!store->open(fn)) { warnPigment << "Could not open the store for profiles directory"; return false; }
        PkByteArray profileRawData = colorSpace->profile()->rawData();
        if (!store->write(profileRawData)) { warnPigment << "Could not write the profiles data into the store"; return false; }
        if (!store->close()) { warnPigment << "Could not close the store for profiles directory"; return false; }
        PkXmlElement el = doc.createElement(KPL_PALETTE_PROFILE_TAG);
        el.setAttribute(KPL_PALETTE_FILENAME_ATTR, fn);
        el.setAttribute(KPL_PALETTE_NAME_ATTR, colorSpace->profile()->name());
        el.setAttribute(KPL_COLOR_MODEL_ID_ATTR, colorSpace->colorModelId().id());
        el.setAttribute(KPL_COLOR_DEPTH_ID_ATTR, colorSpace->colorDepthId().id());
        profileElement.appendChild(el);

    }
    doc.appendChild(profileElement);

    if (!store->open("profiles.xml")) { warnPigment << "Could not open profiles.xml"; return false; }
    PkString xmlStr2 = doc.toByteArray();
    std::string utf82 = xmlStr2.PkToUtf8();
    PkByteArray ba2(utf82.data(), (int)utf82.size());

    PkStream::pk_int64 bytesWritten = store->write(ba2);
    if (bytesWritten != ba2.size()) { warnPigment << "Bytes written is wrong" << ba2.size(); return false; }

    if (!store->close()) { warnPigment << "Could not close the store"; return false; }

    bool r = store->finalize();
    if (!r) { warnPigment << "Could not finalize the store"; }
    return r;
}

void KoColorSet::Private::saveKplGroup(PkXmlDocument &doc,
                                       PkXmlElement &groupEle,
                                       const KisSwatchGroupSP group,
                                       PkSet<const KoColorSpace *> &colorSetSet) const
{
    groupEle.setAttribute(KPL_GROUP_ROW_COUNT_ATTR, KisDomUtils::toString(group->rowCount()));

    for (const KisSwatchGroup::SwatchInfo &info : group->infoList()) {
        const KoColorProfile *profile = info.swatch.color().colorSpace()->profile();
        // Only save non-builtin profiles.=
        if (!profile->fileName().isEmpty()) {
            bool alreadyIncluded = false;
            for (const KoColorSpace* colorSpace : colorSetSet) {
                if (colorSpace->profile()->fileName() == profile->fileName()) {
                    alreadyIncluded = true;
                    break;
                }
            }
            if(!alreadyIncluded) {
                colorSetSet.insert(info.swatch.color().colorSpace());
            }
        }
        PkXmlElement swatchEle = doc.createElement(KPL_SWATCH_TAG);
        swatchEle.setAttribute(KPL_SWATCH_NAME_ATTR, info.swatch.name());
        swatchEle.setAttribute(KPL_SWATCH_ID_ATTR, info.swatch.id());
        swatchEle.setAttribute(KPL_SWATCH_SPOT_ATTR, info.swatch.spotColor() ? PkString("true") : PkString("false"));
        swatchEle.setAttribute(KPL_SWATCH_BITDEPTH_ATTR, info.swatch.color().colorSpace()->colorDepthId().id());
        info.swatch.color().toXML(doc, swatchEle);

        PkXmlElement positionEle = doc.createElement(KPL_SWATCH_POS_TAG);
        positionEle.setAttribute(KPL_SWATCH_ROW_ATTR, KisDomUtils::toString(info.row));
        positionEle.setAttribute(KPL_SWATCH_COL_ATTR, KisDomUtils::toString(info.column));
        swatchEle.appendChild(positionEle);

        groupEle.appendChild(swatchEle);
    }
}

void KoColorSet::Private::loadKplGroup(const PkXmlDocument &doc, const PkXmlElement &parentEle, KisSwatchGroupSP group, PkString version)
{
    (void)doc;
    if (!parentEle.attribute(KPL_GROUP_ROW_COUNT_ATTR).isEmpty()) {
        group->setRowCount(parentEle.attribute(KPL_GROUP_ROW_COUNT_ATTR).toInt());
    }
    group->setColumnCount(colorSet->columnCount());

    for (PkXmlElement swatchEle = parentEle.firstChildElement(KPL_SWATCH_TAG);
         !swatchEle.isNull();
         swatchEle = swatchEle.nextSiblingElement(KPL_SWATCH_TAG)) {
        PkString colorDepthId = swatchEle.attribute(KPL_SWATCH_BITDEPTH_ATTR, Integer8BitsColorDepthID.id());
        KisSwatch swatch;

        if (version == "1.0" && swatchEle.firstChildElement().tagName() == "Lab") {
            // previous version of krita had the values wrong, and scaled everything between 0 to 1,
            // but lab requires L = 0-100 and AB = -128-127.
            // TODO: write unittest for this.
            PkXmlElement el = swatchEle.firstChildElement();
            double L = KisDomUtils::toDouble(el.attribute("L"));
            el.setAttribute("L", KisDomUtils::toString(L*100.0));
            double ab = KisDomUtils::toDouble(el.attribute("a"));
            if (ab <= .5) {
                ab = (0.5 - ab) * 2 * -128.0;
            } else {
                ab = (ab - 0.5) * 2 * 127.0;
            }
            el.setAttribute("a", KisDomUtils::toString(ab));

            ab = KisDomUtils::toDouble(el.attribute("b"));
            if (ab <= .5) {
                ab = (0.5 - ab) * 2 * -128.0;
            } else {
                ab = (ab - 0.5) * 2 * 127.0;
            }
            el.setAttribute("b", KisDomUtils::toString(ab));
            swatch.setColor(KoColor::fromXML(el, colorDepthId));
        } else {
            swatch.setColor(KoColor::fromXML(swatchEle.firstChildElement(), colorDepthId));
        }
        swatch.setName(swatchEle.attribute(KPL_SWATCH_NAME_ATTR));
        swatch.setId(swatchEle.attribute(KPL_SWATCH_ID_ATTR));
        swatch.setSpotColor(swatchEle.attribute(KPL_SWATCH_SPOT_ATTR, PkString("false")) == PkString("true") ? true : false);
        PkXmlElement positionEle = swatchEle.firstChildElement(KPL_SWATCH_POS_TAG);
        if (!positionEle.isNull()) {
            int rowNumber = positionEle.attribute(KPL_SWATCH_ROW_ATTR).toInt();
            int columnNumber = positionEle.attribute(KPL_SWATCH_COL_ATTR).toInt();
            if (columnNumber < 0 ||
                    columnNumber >= colorSet->columnCount() ||
                    rowNumber < 0
                    ) {
                warnPigment << "Swatch" << swatch.name()
                            << "of palette" << colorSet->name()
                            << "has invalid position.";
                continue;
            }
            group->setSwatch(swatch, columnNumber, rowNumber);
        } else {
            group->addSwatch(swatch);
        }
    }

    if (parentEle.attribute(KPL_GROUP_ROW_COUNT_ATTR).isEmpty()
            && group->colorCount() > 0
            && group->columnCount() > 0
            && (group->colorCount() / (group->columnCount()) + 1) < 20) {
        group->setRowCount((group->colorCount() / group->columnCount()) + 1);
    }

}
