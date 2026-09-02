/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "kis_png_document_context.h"


KisPngDocumentContext::KisPngDocumentContext(KisDocument *document)
    : m_document(document)
{
}

KisUndoStore *KisPngDocumentContext::createUndoStore()
{
    return m_document ? m_document->createUndoStore() : nullptr;
}

KoDocumentInfo *KisPngDocumentContext::documentInfo() const
{
    return m_document ? m_document->documentInfo() : nullptr;
}
