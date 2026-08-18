#pragma once
#include "PkString.h"
#include "PkStringList.h"
#include "PkConfigColor.h"
#include "PkPoint.h"

// PkConfigGroup —— KConfigGroup 的零 Qt 替代。轻量句柄：只持有 group 名字的
// 副本，每次读写都通过 PkConfigStore::instance() 落到进程内单例存储，
// 因此同名的两个 PkConfigGroup 实例共享同一份底层数据。
class PkConfigGroup
{
public:
    PkConfigGroup();
    PkConfigGroup(const PkString &groupName);

    bool readEntry(const PkString &key, bool defaultValue) const;
    int readEntry(const PkString &key, int defaultValue) const;
    double readEntry(const PkString &key, double defaultValue) const;
    PkString readEntry(const PkString &key, const PkString &defaultValue) const;
    PkStringList readEntry(const PkString &key, const PkStringList &defaultValue) const;
    PkConfigColor readEntry(const PkString &key, const PkConfigColor &defaultValue) const;
    PkPoint readEntry(const PkString &key, const PkPoint &defaultValue) const;

    void writeEntry(const PkString &key, bool value);
    void writeEntry(const PkString &key, int value);
    void writeEntry(const PkString &key, double value);
    void writeEntry(const PkString &key, const PkString &value);
    void writeEntry(const PkString &key, const PkStringList &value);
    void writeEntry(const PkString &key, const PkConfigColor &value);
    void writeEntry(const PkString &key, const PkPoint &value);

    bool hasKey(const PkString &key) const;
    void deleteEntry(const PkString &key);
    void sync();
    PkString name() const;

private:
    PkString m_groupName;
};
