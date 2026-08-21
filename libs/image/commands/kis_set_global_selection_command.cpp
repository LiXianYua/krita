/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_set_global_selection_command.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include kis_layer.h → kis_psd_layer_style.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂（与 Task 4 kis_paintop_registry.cc 同因）
// 关闭条件：libs/image 的 KoResource 族 virtuals 全量转 PkVector/PkList +
// layer 系头剥 Qt 后解除（Task 8）。当前状态：Qt 仅经未剥依赖头传递进入，
// 不参与薄壳构建。


#include "kis_selection_commands.h"

#include "kis_image.h"
#include "kis_default_bounds.h"
#include "KisImageResolutionProxy.h"
#include "kis_selection.h"
#include "kis_undo_adapter.h"
#include "kis_selection_mask.h"
#include "kis_pixel_selection.h"
#include "KisImageGlobalSelectionManagementInterface.h"
#include "kis_group_layer.h"

#include "kis_image_layer_remove_command.h"
#include "kis_image_layer_add_command.h"
#include "kis_selection_mask.h"
#include "kis_activate_selection_mask_command.h"
#include "KisChangeValueCommand.h"
#include "KisChangeDeselectedMaskCommand.h"
#include "KisNotifySelectionChangedCommand.h"

KisSetGlobalSelectionCommand::KisSetGlobalSelectionCommand(KisImageWSP image, KisSelectionSP selection)
    : m_image(image)
{
    KisImageSP imageSP = m_image.toStrongRef();
    if (!image) {
        return;
    }
    m_oldSelection = imageSP->globalSelection();
    m_newSelection = selection;
}

void KisSetGlobalSelectionCommand::populateChildCommands()
{
    KisImageSP image = m_image.toStrongRef();
    KIS_SAFE_ASSERT_RECOVER_RETURN(image);

    addCommand(new KisNotifySelectionChangedCommand(image, KisNotifySelectionChangedCommand::INITIALIZING));

    KisSelectionMaskSP selectionMask = image->rootLayer()->selectionMask();
    if (selectionMask) {
        addCommand(new KisImageLayerRemoveCommand(image, selectionMask, false, false));
    }

    if (m_newSelection) {
        selectionMask = new KisSelectionMask(image, PkString("Selection Mask"));
        selectionMask->initSelection(image->rootLayer());

        // If we do not set the selection now, the setActive call coming next
        // can be very, very expensive, depending on the size of the image.
        selectionMask->setSelection(m_newSelection);

        addCommand(new KisImageLayerAddCommand(image, selectionMask,
                                               image->root(), image->root()->lastChild(),
                                               false, false));
        addCommand(new KisActivateSelectionMaskCommand(selectionMask, true));

    }

    addCommand(new KisChangeDeselectedMaskCommand(image));
    addCommand(new KisNotifySelectionChangedCommand(image, KisNotifySelectionChangedCommand::FINALIZING));
}

KisSetEmptyGlobalSelectionCommand::KisSetEmptyGlobalSelectionCommand(KisImageWSP image)
    : KisSetGlobalSelectionCommand(image,
                                   new KisSelection(new KisSelectionEmptyBounds(image),
                                                    toQShared(new KisImageResolutionProxy(image))))
{
}
