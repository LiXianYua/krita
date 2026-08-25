/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisCloneDocumentStroke.h"

#include <PkString.h>
#include <PkThread.h>

#include "KisDocument.h"
#include "kis_layer_utils.h"


struct KRITAIMAGE_NO_EXPORT KisCloneDocumentStroke::Private
{
    Private(KisDocument *_document)
        : document(_document)
    {
    }

    KisDocument *document = 0;
};

KisCloneDocumentStroke::KisCloneDocumentStroke(KisDocument *document)
    : KisSimpleStrokeStrategy(PkString("clone-document-stroke"), kundo2_text("Clone Document")),
      m_d(new Private(document))
{
    setClearsRedoOnStart(false);
    setRequestsOtherStrokesToEnd(false);
    setNeedsExplicitCancel(true);
    enableJob(JOB_INIT, true, KisStrokeJobData::BARRIER, KisStrokeJobData::EXCLUSIVE);
    enableJob(JOB_FINISH, true, KisStrokeJobData::BARRIER, KisStrokeJobData::EXCLUSIVE);
    enableJob(JOB_CANCEL, true, KisStrokeJobData::SEQUENTIAL);
}

KisCloneDocumentStroke::~KisCloneDocumentStroke()
{
}

void KisCloneDocumentStroke::initStrokeCallback()
{
    KisLayerUtils::forceAllDelayedNodesUpdate(m_d->document->image()->root());
}

void KisCloneDocumentStroke::finishStrokeCallback()
{
    KisDocument *doc = m_d->document->clone();
    doc->moveToThread(PkThread::mainThreadId());
    sigDocumentCloned(doc);
}

void KisCloneDocumentStroke::cancelStrokeCallback()
{
    sigCloningCancelled();
}
