#pragma once
#include "../PkConfigGroup.h"
// 兼容垫片：真实调用点的 `#include <kconfiggroup.h>`（小写拼写，
// 例：libs/command/KisCumulativeUndoData.cpp:8）在一个字都不改的前提下解析到
// PkConfigGroup。内容与 compat/KConfigGroup 完全相同，理由同上。
#define KConfigGroup PkConfigGroup
