#pragma once
#include "PkConfigGroup.h"
#include "PkString.h"

// PkSharedConfig —— KSharedConfig 的零 Qt 替代。真实调用点全部是
// KSharedConfig::openConfig()->group(...) 链式一次性用完，没有一处保留
// KSharedConfigPtr 做长期持有，所以这里直接返回裸指针，不做引用计数包装。
class PkSharedConfig
{
public:
    static PkSharedConfig *openConfig();

    PkConfigGroup group(const PkString &groupName) const;

private:
    PkSharedConfig() = default;
};
