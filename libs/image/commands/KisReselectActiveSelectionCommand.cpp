/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] KisReselectActiveSelectionCommand.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include kis_layer.h → kis_psd_layer_style.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂（与 Task 4 kis_paintop_registry.cc 同因）
// 关闭条件：libs/image 的 KoResource 族 virtuals 全量转 PkVector/PkList +
// layer 系头剥 Qt 后解除（Task 8）。当前状态：Qt 仅经未剥依赖头传递进入，
// 不参与薄壳构建。


#include "KisReselectActiveSelectionCommand.h"

#include "kis_image.h"
#include "kis_node.h"
#include "kis_layer.h"
#include "kis_selection_mask.h"
#include <KoProperties.h>

KisReselectActiveSelectionCommand::KisReselectActiveSelectionCommand(KisNodeSP activeNode, KisImageWSP image, KUndo2Command *parent)
    : KisReselectGlobalSelectionCommand(image, parent),
      m_activeNode(activeNode)
{
}

void KisReselectActiveSelectionCommand::redo()
{
    bool shouldReselectFGlobalSelection = true;

    if (m_activeNode) {
        KisSelectionMaskSP mask = dynamic_cast<KisSelectionMask*>(m_activeNode.data());

        if (!mask) {

            KisLayerSP layer;
            KisNodeSP node = m_activeNode;
            while (node && !(layer = dynamic_cast<KisLayer*>(node.data()))) {
                node = node->parent();
            }

            if (layer && !layer->selectionMask()) {
                KoProperties properties;
                properties.setProperty("active", false);
                properties.setProperty("visible", true);
                PkList<KisNodeSP> masks = layer->childNodes(PkStringList{PkString("KisSelectionMask")}, properties);

                if (!masks.isEmpty()) {
                    mask = dynamic_cast<KisSelectionMask*>(masks.first().data());
                }
            } else if (layer && layer->selectionMask()) {
                shouldReselectFGlobalSelection = false;
            }
        }

        if (mask) {
            mask->setActive(true);
            shouldReselectFGlobalSelection = false;
            m_reselectedMask = mask;
        }
    }

    if (shouldReselectFGlobalSelection) {
        KisReselectGlobalSelectionCommand::redo();
    }
}

void KisReselectActiveSelectionCommand::undo()
{
    if (m_reselectedMask) {
        m_reselectedMask->setActive(false);
        m_reselectedMask.clear();
    } else {
        KisReselectGlobalSelectionCommand::undo();
    }
}
