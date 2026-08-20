/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisResourceIterator.h"

KisResourceItem::KisResourceItem(KisResourceModel *resourceModel,
                                 const KisResourceRecord &record)
    : m_resourceModel(resourceModel)
    , m_record(record)
{
}

int KisResourceItem::id()
{
    return m_record.id;
}

PkString KisResourceItem::resourceType()
{
    return m_record.resourceType;
}

PkString KisResourceItem::name()
{
    return m_record.name;
}

PkString KisResourceItem::filename()
{
    return m_record.filename;
}

PkString KisResourceItem::tooltip()
{
    return m_record.tooltip;
}

PkString KisResourceItem::md5sum()
{
    return m_record.md5;
}

PkImage KisResourceItem::thumbnail()
{
    return m_record.thumbnail;
}

KoResourceSP KisResourceItem::resource()
{
    return m_resourceModel && m_record.id >= 0
        ? m_resourceModel->resourceForId(m_record.id)
        : KoResourceSP();
}

struct KisResourceIterator::Private
{
    KisResourceModel *resourceModel = nullptr;
    PkVector<KisResourceRecord> records;
    int currentRow = 0;
};

KisResourceIterator::KisResourceIterator(KisResourceModel *resourceModel)
    : d(new Private)
{
    d->resourceModel = resourceModel;
    if (resourceModel) {
        d->records = resourceModel->records();
    }
}

KisResourceIterator::~KisResourceIterator()
{
    delete d;
}

bool KisResourceIterator::hasNext() const
{
    return d->currentRow < d->records.size();
}

bool KisResourceIterator::hasPrevious() const
{
    return d->currentRow > 0 && !d->records.isEmpty();
}

const KisResourceItemSP KisResourceIterator::next()
{
    if (!hasNext()) {
        return KisResourceItemSP(new KisResourceItem(nullptr,
                                                     KisResourceRecord()));
    }
    const KisResourceRecord record = d->records.at(d->currentRow++);
    return KisResourceItemSP(new KisResourceItem(d->resourceModel, record));
}

const KisResourceItemSP KisResourceIterator::peekNext() const
{
    if (!hasNext()) {
        return KisResourceItemSP(new KisResourceItem(nullptr,
                                                     KisResourceRecord()));
    }
    return KisResourceItemSP(
        new KisResourceItem(d->resourceModel, d->records.at(d->currentRow)));
}

const KisResourceItemSP KisResourceIterator::peekPrevious() const
{
    if (!hasPrevious()) {
        return KisResourceItemSP(new KisResourceItem(nullptr,
                                                     KisResourceRecord()));
    }
    return KisResourceItemSP(
        new KisResourceItem(d->resourceModel,
                            d->records.at(d->currentRow - 1)));
}

const KisResourceItemSP KisResourceIterator::previous()
{
    if (!hasPrevious()) {
        return KisResourceItemSP(new KisResourceItem(nullptr,
                                                     KisResourceRecord()));
    }
    --d->currentRow;
    return KisResourceItemSP(
        new KisResourceItem(d->resourceModel, d->records.at(d->currentRow)));
}

void KisResourceIterator::toFront()
{
    d->currentRow = 0;
}

void KisResourceIterator::toBack()
{
    d->currentRow = d->records.size();
}
