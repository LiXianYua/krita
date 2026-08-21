/*
 *  SPDX-FileCopyrightText: 2019 Tusooa Zhu <tusooa@vista.aero>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] KisChangeCloneLayersCommand.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include kis_layer.h → kis_psd_layer_style.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂（与 Task 4 kis_paintop_registry.cc 同因）
// 关闭条件：libs/image 的 KoResource 族 virtuals 全量转 PkVector/PkList +
// layer 系头剥 Qt 后解除（Task 8）。当前状态：Qt 仅经未剥依赖头传递进入，
// 不参与薄壳构建。


#include "KisChangeCloneLayersCommand.h"

#include <kis_clone_layer.h>

struct KisChangeCloneLayersCommand::Private
{
    PkList<KisCloneLayerSP> cloneLayers;
    PkList<KisLayerSP> originalSource;
    KisLayerSP newSource;
};

KisChangeCloneLayersCommand::KisChangeCloneLayersCommand(PkList<KisCloneLayerSP> cloneLayers, KisLayerSP newSource, KUndo2Command *parent)
    : KUndo2Command(kundo2_text("Change Clone Layers"), parent)
    , d(new Private())
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!cloneLayers.isEmpty());
    d->cloneLayers = cloneLayers;
    for (KisCloneLayerSP layer : d->cloneLayers) {
        d->originalSource << layer->copyFrom();
    }
    d->newSource = newSource;
}

void KisChangeCloneLayersCommand::redo()
{
    for (KisCloneLayerSP layer : d->cloneLayers) {
        layer->setCopyFrom(d->newSource);
        layer->setDirty();
    }
}

void KisChangeCloneLayersCommand::undo()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->cloneLayers.size() == d->originalSource.size());
    for (int i = 0; i < d->cloneLayers.size(); ++i) {
        KisCloneLayerSP layer = d->cloneLayers.at(i);
        layer->setCopyFrom(d->originalSource.at(i));
        layer->setDirty();
    }
}

bool KisChangeCloneLayersCommand::mergeWith(const KUndo2Command *command)
{
    const KisChangeCloneLayersCommand *other = dynamic_cast<const KisChangeCloneLayersCommand *>(command);

    if (other && d->cloneLayers == other->d->cloneLayers) {
        d->newSource = other->d->newSource;
        return true;
    }

    return false;
}
