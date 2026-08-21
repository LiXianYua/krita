/*
 *  SPDX-FileCopyrightText: 2010 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_image_set_resolution_command.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include kis_layer.h → kis_psd_layer_style.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂（与 Task 4 kis_paintop_registry.cc 同因）
// 关闭条件：libs/image 的 KoResource 族 virtuals 全量转 PkVector/PkList +
// layer 系头剥 Qt 后解除（Task 8）。当前状态：Qt 仅经未剥依赖头传递进入，
// 不参与薄壳构建。


#include "kis_image_set_resolution_command.h"

#include <kis_image.h>

KisImageSetResolutionCommand::KisImageSetResolutionCommand(KisImageWSP image, qreal newXRes, qreal newYRes, KUndo2Command *parent)
    : KUndo2Command(kundo2_text("Set Image Resolution"), parent)
    , m_image(image)
    , m_newXRes(newXRes)
    , m_newYRes(newYRes)
    , m_oldXRes(0)
    , m_oldYRes(0)
{
    KisImageSP imageSP = image.toStrongRef();
     if (!imageSP) {
         return;
     }
     m_oldXRes = imageSP->xRes();
     m_oldYRes = imageSP->yRes();
}

void KisImageSetResolutionCommand::undo()
{
    KisImageSP image = m_image.toStrongRef();
    if (!image) {
        return;
    }
    image->setResolution(m_oldXRes, m_oldYRes);
}

void KisImageSetResolutionCommand::redo()
{
    KisImageSP image = m_image.toStrongRef();
    if (!image) {
        return;
    }
    image->setResolution(m_newXRes, m_newYRes);
}

#include "kis_processing_visitor.h"

#include "kis_adjustment_layer.h"
#include "generator/kis_generator_layer.h"
#include "kis_external_layer_iface.h"
#include "kis_transparency_mask.h"
#include "kis_filter_mask.h"
#include "kis_transform_mask.h"
#include "kis_selection_mask.h"

#include "kis_selection.h"

class ResetShapesProcessingVisitor : public KisProcessingVisitor
{
public:
    void visit(KisNode*, KisUndoAdapter*) override {}
    void visit(KisPaintLayer*, KisUndoAdapter*) override {}
    void visit(KisGroupLayer*, KisUndoAdapter*) override {}
    void visit(KisCloneLayer*, KisUndoAdapter*) override {}

    void visit(KisAdjustmentLayer *layer, KisUndoAdapter*) override { layer->internalSelection()->updateProjection(); }
    void visit(KisGeneratorLayer *layer, KisUndoAdapter*) override { layer->internalSelection()->updateProjection(); }
    void visit(KisExternalLayer *layer, KisUndoAdapter*) override { layer->resetCache(); }
    void visit(KisFilterMask *mask, KisUndoAdapter*) override { mask->selection()->updateProjection(); }
    void visit(KisTransformMask *mask, KisUndoAdapter*) override { KIS_ASSERT_RECOVER_NOOP(!mask->selection()); }
    void visit(KisTransparencyMask *mask, KisUndoAdapter*) override { mask->selection()->updateProjection(); }
    void visit(KisSelectionMask *mask, KisUndoAdapter*) override { mask->selection()->updateProjection(); }
    void visit(KisColorizeMask *, KisUndoAdapter*) override {}
};

KisResetShapesCommand::KisResetShapesCommand(KisNodeSP rootNode)
    : KUndo2Command(kundo2_text_raw("RESET_SHAPES_COMMAND")),
      m_rootNode(rootNode)
{
}

void KisResetShapesCommand::undo()
{
    KUndo2Command::undo();
    resetNode(m_rootNode);
}

void KisResetShapesCommand::redo()
{
    KUndo2Command::redo();
    resetNode(m_rootNode);
}

void KisResetShapesCommand::resetNode(KisNodeSP node)
{
    ResetShapesProcessingVisitor visitor;
    node->accept(visitor, 0);

    node = node->firstChild();
    while(node) {
        resetNode(node);
        node = node->nextSibling();
    }
}
