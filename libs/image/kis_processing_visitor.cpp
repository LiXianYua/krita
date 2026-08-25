/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_processing_visitor.h"

#include <KoUpdater.h>
#include <KoProgressUpdater.h>
#include "kis_node_progress_proxy.h"
#include "kis_node.h"

KisProcessingVisitor::ProgressHelper::ProgressHelper(const KisNode *node)
{
    KIS_ASSERT(node);
    KisNodeProgressProxy *progressProxy = node->nodeProgressProxy();

    if(progressProxy) {
        m_progressUpdater = new KoProgressUpdater(progressProxy);
        m_progressUpdater->start(100, "Processing");
        m_progressUpdater->moveToThread(node->thread());
    }
    else {
        m_progressUpdater = 0;
    }
}

KisProcessingVisitor::ProgressHelper::~ProgressHelper()
{
    if (m_progressUpdater) {
        m_progressUpdater->deleteLater();
    }
}

KoUpdater* KisProcessingVisitor::ProgressHelper::updater() const
{
    return m_progressUpdater ? m_progressUpdater->startSubtask() : 0;
}

void KisProcessingVisitor::ProgressHelper::cancel()
{
    if (m_progressUpdater) {
        // 壳内无事件队列：Qt 的 singleShot(0, receiver, method) 改为直接调用（R-30 契约）。
        m_progressUpdater->cancel();
    }
}

KisProcessingVisitor::~KisProcessingVisitor()
{
}

KUndo2Command *KisProcessingVisitor::createInitCommand()
{
    return 0;
}
