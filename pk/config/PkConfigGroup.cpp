#include "PkConfigGroup.h"
#include "PkConfigStore.h"

#include <cstdio>
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

PkString formatColor(const PkColor &c)
{
    std::string s = std::to_string(c.red()) + "," +
                     std::to_string(c.green()) + "," +
                     std::to_string(c.blue()) + "," +
                     std::to_string(c.alpha());
    return PkString(s.c_str());
}

PkString formatPoint(const PkPoint &p)
{
    std::string s = std::to_string(p.x()) + "," + std::to_string(p.y());
    return PkString(s.c_str());
}

// std::to_string(double) 不是往返安全的（固定 6 位小数：小量会被截成 "0.000000"，
// 大量/高精度值会被截断）——"%.17g"（17 位十进制有效数字）能保证任何 IEEE754
// double 精确往返，需要时会落到科学计数法（如 "1.5e-07"），PkString::toDouble()
// 底层走 strtod_l，原生支持这种写法，不需要额外处理。
PkString formatDouble(double value)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", value);
    return PkString(buf);
}

} // namespace

PkConfigGroup::PkConfigGroup() : m_groupName()
{
}

PkConfigGroup::PkConfigGroup(const PkString &groupName) : m_groupName(groupName)
{
}

PkConfigGroup::PkConfigGroup(PkSharedConfig *config, const PkString &groupName) : m_groupName(groupName)
{
    // config 故意不用：底层存储是 PkConfigStore 的全局单例，与调用方从哪个
    // "配置句柄"拿到这个 group 无关（同 PkSharedConfig::group() 的实现方式）。
    (void)config;
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

PkColor PkConfigGroup::readEntry(const PkString &key, const PkColor &defaultValue) const
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
        // 每段必须落在 uint8_t 的有效范围内——没有这层检查，"300,-5,0,255" 这类
        // 越界值会让下面 PkColor 构造出无效色（分量全 0），而不是像段数不对时那样
        // 退回 defaultValue。显式判越界保证「数据格式不对 → 一律退回 defaultValue」
        // 的行为一致（PkColor 的越界语义是置无效色，不能当格式错误处理）。
        if (!ok || values[i] < 0 || values[i] > 255) {
            return defaultValue;
        }
    }
    return PkColor(values[0], values[1], values[2], values[3]);
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
    PkConfigStore::instance().set(m_groupName, key, formatDouble(value));
}

void PkConfigGroup::writeEntry(const PkString &key, const PkString &value)
{
    PkConfigStore::instance().set(m_groupName, key, value);
}

void PkConfigGroup::writeEntry(const PkString &key, const PkStringList &value)
{
    PkConfigStore::instance().set(m_groupName, key, value.join(kListSeparator));
}

void PkConfigGroup::writeEntry(const PkString &key, const PkColor &value)
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
