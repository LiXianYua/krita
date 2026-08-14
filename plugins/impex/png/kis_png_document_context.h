/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PNG_DOCUMENT_CONTEXT_H
#define KIS_PNG_DOCUMENT_CONTEXT_H

#include <KisPngCodec.h>

class KisDocument;

class KisPngDocumentContext final : public KisImportExportDocumentContext
{
public:
    explicit KisPngDocumentContext(KisDocument *document);

    KisUndoStore *createUndoStore() override;
    KoDocumentInfo *documentInfo() const override;

private:
    KisDocument *m_document;
};

#endif // KIS_PNG_DOCUMENT_CONTEXT_H
