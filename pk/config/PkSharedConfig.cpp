#include "PkSharedConfig.h"

PkSharedConfig *PkSharedConfig::openConfig()
{
    static PkSharedConfig instance;   // 进程内单例，C++11 magic static
    return &instance;
}

PkConfigGroup PkSharedConfig::group(const PkString &groupName) const
{
    // 每次调用构造一个新的 PkConfigGroup 句柄，但底层数据都落在
    // PkConfigStore::instance()（PkConfigGroup 自己内部去拿），所以同名的两次
    // group() 调用共享同一份存储。
    return PkConfigGroup(groupName);
}
