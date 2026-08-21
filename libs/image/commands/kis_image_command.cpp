/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_image_command.cpp 阻塞登记（S-06 Task 5）
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
#include "kis_layer.h"

KisImageCommand::KisImageCommand(const KUndo2MagicString& name, KisImageWSP image, KUndo2Command *parent)
    : KUndo2Command(name, parent)
    , m_image(image)
{
}

KisImageCommand::~KisImageCommand()
{
}

static inline bool isLayer(KisNodeSP node) {
    return dynamic_cast<KisLayer*>(node.data());
}

KisImageCommand::UpdateTarget::UpdateTarget(KisImageWSP image,
                                            KisNodeSP removedNode,
                                            const PkRect &updateRect)
    : m_image(image), m_updateRect(updateRect)
{
    /**
     * We are saving an index, but not shared pointer, because the
     * target node may suddenly reincarnate into another type of a
     * layer during the removal process
     */
    m_removedNodeParent = removedNode->parent();
    m_removedNodeIndex = m_removedNodeParent ? m_removedNodeParent->index(removedNode) : -1;
}

void KisImageCommand::UpdateTarget::update() {
    if (!m_removedNodeParent) return;
    KIS_ASSERT_RECOVER_RETURN(m_removedNodeIndex >= 0);

    KisNodeSP node;
    int index = m_removedNodeIndex;

    while ((node = m_removedNodeParent->at(index)) && !isLayer(node)) {
        index++;
    }

    if (!node) {
        index = qMax(0, m_removedNodeIndex - 1);

        while ((node = m_removedNodeParent->at(index)) && !isLayer(node)) {
            index--;
        }
    }

    if (node) {
        node->setDirty(m_updateRect);
    } else {
        KisImageSP image = m_image.toStrongRef();
        if (image) {
            image->refreshGraphAsync(m_removedNodeParent);
            m_removedNodeParent->setDirty(m_updateRect);
        }
    }
}
