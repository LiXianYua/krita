/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_image_layer_move_command.cpp 阻塞登记（S-06 Task 5）
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

#include "KoColor.h"
#include "KoColorProfile.h"

#include "kis_image.h"
#include "kis_layer.h"
#include "kis_group_layer.h"
#include "kis_undo_adapter.h"

KisImageLayerMoveCommand::KisImageLayerMoveCommand(KisImageWSP image, KisNodeSP layer, KisNodeSP newParent, KisNodeSP newAbove, bool doUpdates)
        : KisImageCommand(kundo2_text("Move Layer"), image)
{
    m_layer = layer;
    m_newParent = newParent;
    m_newAbove = newAbove;
    m_prevParent = layer->parent();
    m_prevAbove = layer->prevSibling();
    m_index = -1;
    m_useIndex = false;
    m_doUpdates = doUpdates;
}

KisImageLayerMoveCommand::KisImageLayerMoveCommand(KisImageWSP image, KisNodeSP node, KisNodeSP newParent, quint32 index)
        : KisImageCommand(kundo2_text("Move Layer"), image)
{
    m_layer = node;
    m_newParent = newParent;
    m_newAbove = 0;
    m_prevParent = node->parent();
    m_prevAbove = node->prevSibling();
    m_index = index;
    m_useIndex = true;
    m_doUpdates = true;
}

void KisImageLayerMoveCommand::redo()
{
    KisImageSP image = m_image.toStrongRef();
    if (!image) {
        return;
    }
    if (m_useIndex) {
        image->moveNode(m_layer, m_newParent, m_index);
    } else {
        image->moveNode(m_layer, m_newParent, m_newAbove);
    }

    if (m_doUpdates) {
        image->refreshGraphAsync(m_prevParent, KisProjectionUpdateFlag::NoFilthy);
        if (m_newParent != m_prevParent) {
            m_layer->setDirty(image->bounds());
        }
    }
}

void KisImageLayerMoveCommand::undo()
{
    KisImageSP image = m_image.toStrongRef();
    if (!image) {
        return;
    }
    image->moveNode(m_layer, m_prevParent, m_prevAbove);

    if (m_doUpdates) {
        image->refreshGraphAsync(m_newParent, KisProjectionUpdateFlag::NoFilthy);
        if (m_newParent != m_prevParent) {
            m_layer->setDirty(image->bounds());
        }
    }
}
