#pragma once
#include "PkString.h"
#include "PkStringList.h"
#include "PkConfigColor.h"
#include "PkPoint.h"

// 只需要指针类型：两参构造函数只做类型检查、不解引用，前置声明避免与
// PkSharedConfig.h（它反过来 #include 本头）形成真正的循环包含。
class PkSharedConfig;

// PkConfigGroup —— KConfigGroup 的零 Qt 替代。轻量句柄：只持有 group 名字的
// 副本，每次读写都通过 PkConfigStore::instance() 落到进程内单例存储，
// 因此同名的两个 PkConfigGroup 实例共享同一份底层数据。
class PkConfigGroup
{
public:
    PkConfigGroup();
    PkConfigGroup(const PkString &groupName);
    // 真实调用点的两参构造形式：KConfigGroup cfg(KSharedConfig::openConfig(), "Group")。
    // config 只用于类型检查，不使用——底层存储是 PkConfigStore 的全局单例，与
    // 走哪个"配置句柄"进来无关（同 PkSharedConfig::group() 的实现方式）。
    PkConfigGroup(PkSharedConfig *config, const PkString &groupName);

    bool readEntry(const PkString &key, bool defaultValue) const;
    int readEntry(const PkString &key, int defaultValue) const;
    double readEntry(const PkString &key, double defaultValue) const;
    PkString readEntry(const PkString &key, const PkString &defaultValue) const;
    // const char* 字面量默认值的精确匹配重载：没有它，`readEntry("k", "lit")`
    // 会靠标准布尔转换悄悄绑到上面的 bool 重载（const char* → bool 是唯一不需要
    // 用户自定义转换就能编译通过的路径），返回类型也变成 bool，且编译器不报错。
    PkString readEntry(const PkString &key, const char *defaultValue) const
    {
        return readEntry(key, PkString(defaultValue));
    }
    PkStringList readEntry(const PkString &key, const PkStringList &defaultValue) const;
    PkConfigColor readEntry(const PkString &key, const PkConfigColor &defaultValue) const;
    PkPoint readEntry(const PkString &key, const PkPoint &defaultValue) const;

    // 显式模板实参形式：真实调用点里大量出现 `g.readEntry<bool>("k", def)` 这种
    // 写法（源自 KConfigGroup::readEntry 本来就是模板），上面 7 个非模板重载不
    // 支持在成员函数名后面接 <T>。转发到非模板重载：对于参数推导能精确匹配某个
    // 非模板重载的调用（包括不写 <T> 的普通调用），重载决议按标准规则优先选非
    // 模板的精确匹配，本模板不会被选中，因此不影响任何既有的非模板调用形式；
    // 只有显式写 <T> 或没有非模板重载能精确匹配时才会走到这里。
    template<typename T>
    T readEntry(const PkString &key, const T &defaultValue) const
    {
        return readEntry(key, defaultValue);
    }

    void writeEntry(const PkString &key, bool value);
    void writeEntry(const PkString &key, int value);
    void writeEntry(const PkString &key, double value);
    void writeEntry(const PkString &key, const PkString &value);
    void writeEntry(const PkString &key, const PkStringList &value);
    void writeEntry(const PkString &key, const PkConfigColor &value);
    void writeEntry(const PkString &key, const PkPoint &value);

    // 同上的模板转发，专治 `writeEntry("k", quint32(v))` 这类调用：quint32
    // （unsigned int）到 bool/int/double 是三个同等排名的标准转换，编译器选不出
    // 唯一最佳匹配，报 ambiguous。模板实例化后的形参类型与实参类型完全一致
    // （identity 转换），比任何一个需要转换的非模板重载都更优，重载决议因此
    // 不再有歧义，落到这里、转成 int 存储——与真实调用点"枚举值先转 quint32
    // 再写入、读回来走 int 重载"的用法一致。
    template<typename T>
    void writeEntry(const PkString &key, const T &value)
    {
        writeEntry(key, static_cast<int>(value));
    }

    bool hasKey(const PkString &key) const;
    void deleteEntry(const PkString &key);
    void sync();
    PkString name() const;

private:
    PkString m_groupName;
};
