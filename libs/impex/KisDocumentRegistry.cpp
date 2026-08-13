/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDocumentRegistry.h"

#include <QGlobalStatic>

#include <utility>

Q_GLOBAL_STATIC(KisDocumentRegistry, s_documentRegistry)

class KisDocumentRegistry::Private
{
public:
    DocumentFactory factory;
    DocumentDeleter deleter;
    DocumentPath path;
    QList<KisDocument *> documents;
};

KisDocumentRegistry::KisDocumentRegistry(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

KisDocumentRegistry::KisDocumentRegistry(DocumentFactory factory,
                                         DocumentDeleter deleter,
                                         DocumentPath path,
                                         QObject *parent)
    : KisDocumentRegistry(parent)
{
    setDocumentServices(std::move(factory), std::move(deleter), std::move(path));
}

KisDocumentRegistry::~KisDocumentRegistry()
{
    delete d;
}

KisDocumentRegistry *KisDocumentRegistry::instance()
{
    return s_documentRegistry;
}

void KisDocumentRegistry::setDocumentServices(DocumentFactory factory,
                                              DocumentDeleter deleter,
                                              DocumentPath path)
{
    d->factory = std::move(factory);
    d->deleter = std::move(deleter);
    d->path = std::move(path);
}

void KisDocumentRegistry::clearDocumentServices()
{
    d->factory = {};
    d->deleter = {};
    d->path = {};
}

KisDocument *KisDocumentRegistry::createDocument() const
{
    return d->factory ? d->factory(true) : nullptr;
}

KisDocument *KisDocumentRegistry::createTemporaryDocument() const
{
    return d->factory ? d->factory(false) : nullptr;
}

void KisDocumentRegistry::addDocument(KisDocument *document, bool notify)
{
    if (!document || d->documents.contains(document)) {
        return;
    }

    d->documents.append(document);

    QObject *object = reinterpret_cast<QObject *>(document);
    connect(object, SIGNAL(sigSavingFinished(QString)),
            this, SIGNAL(sigDocumentSaved(QString)));
    connect(object, &QObject::destroyed, this, [this, document]() {
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
    Q_EMIT sigDocumentRemoved(d->path ? d->path(document) : QString());

    if (deleteDocument && d->deleter) {
        d->deleter(document);
    }
}
