#pragma once
#include "../PkSharedConfig.h"
// 兼容垫片：真实调用点的 `#include <ksharedconfig.h>`（小写拼写）在一个字都不改
// 的前提下解析到 PkSharedConfig。内容与 compat/KSharedConfig 完全相同，理由同上。
#define KSharedConfig PkSharedConfig
// 理由同 compat/KSharedConfig：KSharedConfigPtr 是独立 token，需要单独一条。
#define KSharedConfigPtr PkSharedConfigPtr
