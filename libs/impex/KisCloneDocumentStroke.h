/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISCLONEDOCUMENTSTROKE_H
#define KISCLONEDOCUMENTSTROKE_H

#include "kritaimage_export.h"
#include <PkObject.h>
#include <PkScopedPointer.h>
#include "kis_simple_stroke_strategy.h"

class KisDocument;

class KisCloneDocumentStroke : public PkObject, public KisSimpleStrokeStrategy
{
public:
    KisCloneDocumentStroke(KisDocument *document);
    ~KisCloneDocumentStroke();

    void initStrokeCallback() override;
    void finishStrokeCallback() override;
    void cancelStrokeCallback() override;

public:
    void sigDocumentCloned(KisDocument *image);
    void sigCloningCancelled();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KISCLONEDOCUMENTSTROKE_H
