/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDocumentRegistry.h"
#include "KisDocument.h"

#include <QGlobalStatic>

Q_GLOBAL_STATIC(KisDocumentRegistry, s_documentRegistry)

class KisDocumentRegistry::Private
{
public:
    QList<KisDocument *> documents;
};

KisDocumentRegistry::KisDocumentRegistry(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

KisDocumentRegistry::~KisDocumentRegistry()
{
    delete d;
}

KisDocumentRegistry *KisDocumentRegistry::instance()
{
    return s_documentRegistry;
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
    connect(document, &QObject::destroyed, this, [this, document]() {
        d->documents.removeAll(document);
    });

    if (notify) {
        Q_EMIT sigDocumentAdded(document);
    }
}

QList<KisDocument *> KisDocumentRegistry::documents() const
{
    return d->documents;
}

int KisDocumentRegistry::documentCount() const
{
    return d->documents.size();
}

void KisDocumentRegistry::removeDocument(KisDocument *document, bool deleteDocument)
{
    if (!document) {
        return;
    }

    d->documents.removeAll(document);
    Q_EMIT sigDocumentRemoved(document->path());

    if (deleteDocument) {
        document->deleteLater();
    }
}
