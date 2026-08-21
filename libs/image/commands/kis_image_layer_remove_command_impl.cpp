/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_image_layer_remove_command_impl.cpp 阻塞登记（S-06 Task 5）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 本文件传递 include kis_layer.h → kis_psd_layer_style.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂（与 Task 4 kis_paintop_registry.cc 同因）
// 关闭条件：libs/image 的 KoResource 族 virtuals 全量转 PkVector/PkList +
// layer 系头剥 Qt 后解除（Task 8）。当前状态：Qt 仅经未剥依赖头传递进入，
// 不参与薄壳构建。


#include "kis_image_layer_remove_command_impl.h"
#include "kis_image.h"

#include "kis_layer.h"
#include "kis_clone_layer.h"
#include "kis_paint_layer.h"

struct Q_DECL_HIDDEN KisImageLayerRemoveCommandImpl::Private {
    Private(KisImageLayerRemoveCommandImpl *_q) : q(_q) {}

    KisImageLayerRemoveCommandImpl *q;

    KisNodeSP node;
    KisNodeSP prevParent;
    KisNodeSP prevAbove;

    PkList<KisCloneLayerSP> clonesList;
    PkList<KisLayerSP> reincarnatedNodes;

    void restoreClones();
    void processClones(KisNodeSP node);
    void moveChildren(KisNodeSP src, KisNodeSP dst);
    void moveClones(KisLayerSP src, KisLayerSP dst);
};

KisImageLayerRemoveCommandImpl::KisImageLayerRemoveCommandImpl(KisImageWSP image, KisNodeSP node, KUndo2Command *parent)
    : KisImageCommand(kundo2_text("Remove Layer"), image, parent),
      m_d(new Private(this))
{
    m_d->node = node;
    m_d->prevParent = node->parent();
    m_d->prevAbove = node->prevSibling();
}

KisImageLayerRemoveCommandImpl::~KisImageLayerRemoveCommandImpl()
{
    delete m_d;
}

void KisImageLayerRemoveCommandImpl::redo()
{
    KisImageSP image = m_image.toStrongRef();
    if (!image) {
        return;
    }
    m_d->processClones(m_d->node);
    image->removeNode(m_d->node);
}

void KisImageLayerRemoveCommandImpl::undo()
{
    KisImageSP image = m_image.toStrongRef();
    if (!image) {
        return;
    }
    m_d->restoreClones(); // so that we can get prevAbove back, if it is our clone
    image->addNode(m_d->node, m_d->prevParent, m_d->prevAbove);
}

void KisImageLayerRemoveCommandImpl::Private::restoreClones()
{
    Q_ASSERT(reincarnatedNodes.size() == clonesList.size());
    KisImageSP image = q->m_image.toStrongRef();
    if (!image) {
        return;
    }
    for (int i = 0; i < reincarnatedNodes.size(); i++) {
        KisCloneLayerSP clone = clonesList[i];
        KisLayerSP newNode = reincarnatedNodes[i];

        image->addNode(clone, newNode->parent(), newNode);
        moveChildren(newNode, clone);
        moveClones(newNode, clone);
        image->removeNode(newNode);
    }
}

void KisImageLayerRemoveCommandImpl::Private::processClones(KisNodeSP node)
{
    KisLayerSP layer(dynamic_cast<KisLayer*>(node.data()));
    if(!layer || !layer->hasClones()) return;

    if(reincarnatedNodes.isEmpty()) {
        /**
         * Initialize the list of reincarnates nodes
         */
        for (KisCloneLayerWSP _clone : layer->registeredClones()) {
            KisCloneLayerSP clone = _clone;
            Q_ASSERT(clone);

            clonesList.append(clone);
            reincarnatedNodes.append(clone->reincarnateAsPaintLayer());
        }
    }

    KisImageSP image = q->m_image.toStrongRef();
    if (!image) {
        return;
    }

    /**
     * Move the children and transitive clones to the
     * reincarnated nodes
     */
    for (int i = 0; i < reincarnatedNodes.size(); i++) {
        KisCloneLayerSP clone = clonesList[i];
        KisLayerSP newNode = reincarnatedNodes[i];

        image->addNode(newNode, clone->parent(), clone);
        moveChildren(clone, newNode);
        moveClones(clone, newNode);
        image->removeNode(clone);
    }
}

void KisImageLayerRemoveCommandImpl::Private::moveChildren(KisNodeSP src, KisNodeSP dst)
{
    KisImageSP image = q->m_image.toStrongRef();
    if (!image) {
        return;
    }
    KisNodeSP child = src->firstChild();
    while(child) {
        image->moveNode(child, dst, dst->lastChild());
        child = child->nextSibling();
    }
}

void KisImageLayerRemoveCommandImpl::Private::moveClones(KisLayerSP src, KisLayerSP dst)
{
    for (KisCloneLayerWSP _clone : src->registeredClones()) {
        KisCloneLayerSP clone = _clone;
        Q_ASSERT(clone);

        clone->setCopyFrom(dst);
    }
}
