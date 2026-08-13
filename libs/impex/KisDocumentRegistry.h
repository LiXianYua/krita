/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DOCUMENT_REGISTRY_H
#define KIS_DOCUMENT_REGISTRY_H

#include <functional>

#include <QObject>
#include <QList>
#include <QString>

#include "kritaimpex_export.h"

class KisDocument;

/**
 * Application-independent ownership and lifecycle registry for documents.
 *
 * Construction, deletion and path access are supplied by the document owner.
 * This keeps the registry usable below the desktop UI while KisDocument's
 * implementation is moved into the document/import-export domain.
 */
class KRITAIMPEX_EXPORT KisDocumentRegistry : public QObject
{
    Q_OBJECT

public:
    using DocumentFactory = std::function<KisDocument *(bool addStorage)>;
    using DocumentDeleter = std::function<void(KisDocument *)>;
    using DocumentPath = std::function<QString(KisDocument *)>;

    explicit KisDocumentRegistry(QObject *parent = nullptr);
    KisDocumentRegistry(DocumentFactory factory,
                        DocumentDeleter deleter,
                        DocumentPath path,
                        QObject *parent = nullptr);
    ~KisDocumentRegistry() override;

    static KisDocumentRegistry *instance();

    void setDocumentServices(DocumentFactory factory,
                             DocumentDeleter deleter,
                             DocumentPath path);
    void clearDocumentServices();

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
