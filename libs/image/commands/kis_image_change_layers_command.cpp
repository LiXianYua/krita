/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_image_change_layers_command.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include kis_layer.h → kis_psd_layer_style.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂（与 Task 4 kis_paintop_registry.cc 同因）
// 关闭条件：libs/image 的 KoResource 族 virtuals 全量转 PkVector/PkList +
// layer 系头剥 Qt 后解除（Task 8）。当前状态：Qt 仅经未剥依赖头传递进入，
// 不参与薄壳构建。


#include "kis_image_commands.h"
#include "kis_image.h"
#include "kis_group_layer.h"

KisImageChangeLayersCommand::KisImageChangeLayersCommand(KisImageWSP image, KisNodeSP oldRootLayer, KisNodeSP newRootLayer)
    : KisImageCommand(kundo2_text_raw("change-layer-command"), image)
{
    m_oldRootLayer = oldRootLayer;
    m_newRootLayer = newRootLayer;
}

void KisImageChangeLayersCommand::redo()
{
    KisImageSP image = m_image.toStrongRef();
    if (image) {
        image->setRootLayer(static_cast<KisGroupLayer*>(m_newRootLayer.data()));
        image->refreshGraphAsync();
        image->notifyLayersChanged();
    }
}

void KisImageChangeLayersCommand::undo()
{
    KisImageSP image = m_image.toStrongRef();
    if (image) {
        image->setRootLayer(static_cast<KisGroupLayer*>(m_oldRootLayer.data()));
        image->refreshGraphAsync();
        image->notifyLayersChanged();
    }
}
