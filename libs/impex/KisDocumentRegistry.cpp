/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDocumentRegistry.h"
#include "KisDocument.h"

#include <PkPointer.h>

class KisDocumentRegistry::Private
{
public:
    // PkObject 无 destroyed 信号；用 PkPointer（Qt 弱指针 Q 指针的替代）观察文档
    // 生命周期：文档在未走 removeDocument 路径被析构时条目自动置 null，
    // documents()/documentCount() 读取时过滤——语义与原 Qt 的 destroyed 信号连接
    // （析构即从注册表摘除）一致，防悬垂指针。
    PkList<PkPointer<KisDocument>> documents;
};

KisDocumentRegistry::KisDocumentRegistry(PkObject *parent)
    : PkObject(parent)
    , d(new Private)
{
}

KisDocumentRegistry::~KisDocumentRegistry()
{
    delete d;
}

KisDocumentRegistry *KisDocumentRegistry::instance()
{
    static KisDocumentRegistry s_documentRegistry;
    return &s_documentRegistry;
}

KisDocument *KisDocumentRegistry::createDocument() const
{
    return new KisDocument(true);
}

KisDocument *KisDocumentRegistry::createTemporaryDocument() const
{
    return new KisDocument(false);
}

void KisDocumentRegistry::addDocument(KisDocument *document, bool notify)
{
    if (!document || d->documents.contains(document)) {
        return;
    }

    d->documents.append(document);

    connect(document, &KisDocument::sigSavingFinished,
            this, &KisDocumentRegistry::sigDocumentSaved);

    if (notify) {
        sigDocumentAdded(document);
    }
}

PkList<KisDocument *> KisDocumentRegistry::documents() const
{
    PkList<KisDocument *> result;
    for (const auto &ptr : d->documents) {
        if (!ptr.isNull()) {
            result.append(ptr.data());
        }
    }
    return result;
}

int KisDocumentRegistry::documentCount() const
{
    int count = 0;
    for (const auto &ptr : d->documents) {
        if (!ptr.isNull()) {
            ++count;
        }
    }
    return count;
}

void KisDocumentRegistry::removeDocument(KisDocument *document, bool deleteDocument)
{
    if (!document) {
        return;
    }

    d->documents.removeAll(document);
    sigDocumentRemoved(document->path());

    if (deleteDocument) {
        document->deleteLater();
    }
}
