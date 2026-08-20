/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISRESOURCEITERATOR_H
#define KISRESOURCEITERATOR_H

#include <PkImage.h>
#include <PkSharedPointer.h>
#include <PkString.h>

#include <KoResource.h>

#include "KisResourceModel.h"
#include "kritaresources_export.h"

class KRITARESOURCES_EXPORT KisResourceItem
{
private:
    friend class KisResourceIterator;
    KisResourceItem(KisResourceModel *resourceModel,
                    const KisResourceRecord &record);

public:
    int id();
    PkString resourceType();
    PkString name();
    PkString filename();
    PkString tooltip();
    PkString md5sum();
    PkImage thumbnail();
    KoResourceSP resource();

private:
    KisResourceModel *m_resourceModel = nullptr;
    KisResourceRecord m_record;
};

using KisResourceItemSP = PkSharedPointer<KisResourceItem>;

/** Stable iterator over a record snapshot captured at construction. */
class KRITARESOURCES_EXPORT KisResourceIterator
{
public:
    explicit KisResourceIterator(KisResourceModel *resourceModel);
    ~KisResourceIterator();

    bool hasNext() const;
    bool hasPrevious() const;
    const KisResourceItemSP next();
    const KisResourceItemSP peekNext() const;
    const KisResourceItemSP peekPrevious() const;
    const KisResourceItemSP previous();
    void toFront();
    void toBack();

private:
    struct Private;
    Private *const d;
};

#endif // KISRESOURCEITERATOR_H
