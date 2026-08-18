#include "PkConfigGroup.h"
#include "PkConfigStore.h"

#include <string>
#include <vector>

namespace {

// PkStringList 的元素分隔符：ASCII Unit Separator，元素本身几乎不可能包含它，
// 所以不用逗号（逗号会跟元素内容冲突）。
const char16_t kListSeparator = u'\x1f';

const PkString &trueLiteral()
{
    static const PkString s("true");
    return s;
}

const PkString &falseLiteral()
{
    static const PkString s("false");
    return s;
}

PkString formatColor(const PkConfigColor &c)
{
    std::string s = std::to_string(static_cast<int>(c.r)) + "," +
                     std::to_string(static_cast<int>(c.g)) + "," +
                     std::to_string(static_cast<int>(c.b)) + "," +
                     std::to_string(static_cast<int>(c.a));
    return PkString(s.c_str());
}

PkString formatPoint(const PkPoint &p)
{
    std::string s = std::to_string(p.x()) + "," + std::to_string(p.y());
    return PkString(s.c_str());
}

} // namespace

PkConfigGroup::PkConfigGroup() : m_groupName()
{
}

PkConfigGroup::PkConfigGroup(const PkString &groupName) : m_groupName(groupName)
{
}

bool PkConfigGroup::readEntry(const PkString &key, bool defaultValue) const
{
    PkConfigStore &store = PkConfigStore::instance();
    if (!store.has(m_groupName, key)) {
        return defaultValue;
    }
    PkString raw = store.get(m_groupName, key, PkString());
    if (raw == trueLiteral()) {
        return true;
    }
    if (raw == falseLiteral()) {
        return false;
    }
    return defaultValue;
}

int PkConfigGroup::readEntry(const PkString &key, int defaultValue) const
{
    PkConfigStore &store = PkConfigStore::instance();
    if (!store.has(m_groupName, key)) {
        return defaultValue;
    }
    PkString raw = store.get(m_groupName, key, PkString());
    bool ok = false;
    int value = raw.toInt(&ok);
    return ok ? value : defaultValue;
}

double PkConfigGroup::readEntry(const PkString &key, double defaultValue) const
{
    PkConfigStore &store = PkConfigStore::instance();
    if (!store.has(m_groupName, key)) {
        return defaultValue;
    }
    PkString raw = store.get(m_groupName, key, PkString());
    bool ok = false;
    double value = raw.toDouble(&ok);
    return ok ? value : defaultValue;
}

PkString PkConfigGroup::readEntry(const PkString &key, const PkString &defaultValue) const
{
    PkConfigStore &store = PkConfigStore::instance();
    if (!store.has(m_groupName, key)) {
        return defaultValue;
    }
    return store.get(m_groupName, key, defaultValue);
}

PkStringList PkConfigGroup::readEntry(const PkString &key, const PkStringList &defaultValue) const
{
    PkConfigStore &store = PkConfigStore::instance();
    if (!store.has(m_groupName, key)) {
        return defaultValue;
    }
    PkString raw = store.get(m_groupName, key, PkString());
    if (raw.isEmpty()) {
        return PkStringList();
    }
    std::vector<PkString> parts = raw.split(kListSeparator);
    PkStringList result;
    for (const PkString &part : parts) {
        result.append(part);
    }
    return result;
}

PkConfigColor PkConfigGroup::readEntry(const PkString &key, const PkConfigColor &defaultValue) const
{
    PkConfigStore &store = PkConfigStore::instance();
    if (!store.has(m_groupName, key)) {
        return defaultValue;
    }
    PkString raw = store.get(m_groupName, key, PkString());
    std::vector<PkString> parts = raw.split(u',');
    if (parts.size() != 4) {
        return defaultValue;
    }
    int values[4];
    for (int i = 0; i < 4; ++i) {
        bool ok = false;
        values[i] = parts[static_cast<std::size_t>(i)].toInt(&ok);
        if (!ok) {
            return defaultValue;
        }
    }
    return PkConfigColor(values[0], values[1], values[2], values[3]);
}

PkPoint PkConfigGroup::readEntry(const PkString &key, const PkPoint &defaultValue) const
{
    PkConfigStore &store = PkConfigStore::instance();
    if (!store.has(m_groupName, key)) {
        return defaultValue;
    }
    PkString raw = store.get(m_groupName, key, PkString());
    std::vector<PkString> parts = raw.split(u',');
    if (parts.size() != 2) {
        return defaultValue;
    }
    bool okX = false, okY = false;
    int x = parts[0].toInt(&okX);
    int y = parts[1].toInt(&okY);
    if (!okX || !okY) {
        return defaultValue;
    }
    return PkPoint(x, y);
}

void PkConfigGroup::writeEntry(const PkString &key, bool value)
{
    PkConfigStore::instance().set(m_groupName, key, value ? trueLiteral() : falseLiteral());
}

void PkConfigGroup::writeEntry(const PkString &key, int value)
{
    PkConfigStore::instance().set(m_groupName, key, PkString(std::to_string(value).c_str()));
}

void PkConfigGroup::writeEntry(const PkString &key, double value)
{
    PkConfigStore::instance().set(m_groupName, key, PkString(std::to_string(value).c_str()));
}

void PkConfigGroup::writeEntry(const PkString &key, const PkString &value)
{
    PkConfigStore::instance().set(m_groupName, key, value);
}

void PkConfigGroup::writeEntry(const PkString &key, const PkStringList &value)
{
    PkConfigStore::instance().set(m_groupName, key, value.join(kListSeparator));
}

void PkConfigGroup::writeEntry(const PkString &key, const PkConfigColor &value)
{
    PkConfigStore::instance().set(m_groupName, key, formatColor(value));
}

void PkConfigGroup::writeEntry(const PkString &key, const PkPoint &value)
{
    PkConfigStore::instance().set(m_groupName, key, formatPoint(value));
}

bool PkConfigGroup::hasKey(const PkString &key) const
{
    return PkConfigStore::instance().has(m_groupName, key);
}

void PkConfigGroup::deleteEntry(const PkString &key)
{
    PkConfigStore::instance().remove(m_groupName, key);
}

void PkConfigGroup::sync()
{
    // 不做真实磁盘持久化（见 task brief「Global Constraints」）：数据一直
    // 活在 PkConfigStore 的进程内单例里，sync() 只需要不抛异常/不崩。
}

PkString PkConfigGroup::name() const
{
    return m_groupName;
}
