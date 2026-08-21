/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] KisChangeChannelFlagsCommand.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include kis_layer.h → kis_psd_layer_style.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂（与 Task 4 kis_paintop_registry.cc 同因）
// 关闭条件：libs/image 的 KoResource 族 virtuals 全量转 PkVector/PkList +
// layer 系头剥 Qt 后解除（Task 8）。当前状态：Qt 仅经未剥依赖头传递进入，
// 不参与薄壳构建。


#include "KisChangeChannelFlagsCommand.h"
#include "kis_layer.h"

KisChangeChannelFlagsCommand::KisChangeChannelFlagsCommand(const PkBitArray &newFlags, const PkBitArray &oldFlags,
                                                           KisLayerSP layer, KUndo2Command *parentCommand)
    : KUndo2Command(kundo2_text_raw("change-channel-flags-command"), parentCommand),
      m_layer(layer),
      m_oldFlags(oldFlags),
      m_newFlags(newFlags)
{
}

KisChangeChannelFlagsCommand::KisChangeChannelFlagsCommand(const PkBitArray &newFlags, KisLayerSP layer, KUndo2Command *parentCommand)
    : KisChangeChannelFlagsCommand(newFlags, layer->channelFlags(), layer, parentCommand)
{
}

void KisChangeChannelFlagsCommand::redo()
{
    m_layer->setChannelFlags(m_newFlags);
}

void KisChangeChannelFlagsCommand::undo()
{
    m_layer->setChannelFlags(m_oldFlags);
}
