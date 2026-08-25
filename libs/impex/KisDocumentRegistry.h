/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DOCUMENT_REGISTRY_H
#define KIS_DOCUMENT_REGISTRY_H

#include <PkList.h>
#include <PkObject.h>
#include <PkString.h>

#include "kritaimpex_export.h"

class KisDocument;

/**
 * Application-independent ownership and lifecycle registry for documents.
 *
 * The registry and its default document lifecycle belong to the lower
 * document/import-export domain; desktop shells only observe its signals.
 */
class KRITAIMPEX_EXPORT KisDocumentRegistry : public PkObject
{
    Q_OBJECT

public:
    explicit KisDocumentRegistry(PkObject *parent = nullptr);
    ~KisDocumentRegistry() override;

    static KisDocumentRegistry *instance();

    KisDocument *createDocument() const;
    KisDocument *createTemporaryDocument() const;

    void addDocument(KisDocument *document, bool notify = true);
    PkList<KisDocument *> documents() const;
    int documentCount() const;
    void removeDocument(KisDocument *document, bool deleteDocument = true);

Q_SIGNALS:
    void sigDocumentAdded(KisDocument *document);
    void sigDocumentSaved(const PkString &path);
    void sigDocumentRemoved(const PkString &path);

private:
    class Private;
    Private *const d;
};

#endif
