#pragma once
#include "PkConfigGroup.h"
#include "PkString.h"

// PkSharedConfig —— KSharedConfig 的零 Qt 替代。真实调用点全部是
// KSharedConfig::openConfig()->group(...) 链式一次性用完，没有一处保留
// KSharedConfigPtr 做长期持有，所以这里直接返回裸指针，不做引用计数包装。
class PkSharedConfig
{
public:
    // 真实 KDE 的 KSharedConfig::Ptr 是引用计数智能指针；这里用裸指针别名——
    // openConfig() 本来就返回一个进程生命周期单例的裸指针，实测的调用点里
    // 没有一处对它做手动生命周期管理（见类顶注释），裸指针别名与这个既有设计
    // 是同一件事，不是新架构决策。
    using Ptr = PkSharedConfig*;

    static PkSharedConfig *openConfig();

    PkConfigGroup group(const PkString &groupName) const;

private:
    PkSharedConfig() = default;
};

// KSharedConfigPtr（KDE 的自由 typedef 拼写，未挂在 KSharedConfig:: 下）对应的
// 别名——#define KSharedConfig PkSharedConfig 只重写单个 token，覆盖不到
// KSharedConfigPtr 这个不同的 token，所以单独给它一个可以被 compat 垫片
// #define 的顶层名字。理由同上（PkSharedConfig::Ptr 的注释）。
using PkSharedConfigPtr = PkSharedConfig*;
