/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DOCUMENT_REGISTRY_H
#define KIS_DOCUMENT_REGISTRY_H

#include <QObject>
#include <QList>
#include <QString>

#include "kritaimpex_export.h"

class KisDocument;

/**
 * Application-independent ownership and lifecycle registry for documents.
 *
 * The registry and its default document lifecycle belong to the lower
 * document/import-export domain; desktop shells only observe its signals.
 */
class KRITAIMPEX_EXPORT KisDocumentRegistry : public QObject
{
    Q_OBJECT

public:
    explicit KisDocumentRegistry(QObject *parent = nullptr);
    ~KisDocumentRegistry() override;

    static KisDocumentRegistry *instance();

    KisDocument *createDocument() const;
    KisDocument *createTemporaryDocument() const;

    void addDocument(KisDocument *document, bool notify = true);
    QList<KisDocument *> documents() const;
    int documentCount() const;
    void removeDocument(KisDocument *document, bool deleteDocument = true);

Q_SIGNALS:
    void sigDocumentAdded(KisDocument *document);
    void sigDocumentSaved(const QString &path);
    void sigDocumentRemoved(const QString &path);

private:
    class Private;
    Private *const d;
};

#endif
