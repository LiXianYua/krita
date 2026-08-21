/*
 * SPDX-FileCopyrightText: 2021 Mathias Wein <lynx.mw+kde@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <PkXmlCompat.h>

#include <resources/KisUniqueColorSet.h>

#include <PkGlobal.h>
#include <PkHash.h>
#include <PkMessageLogger.h>

#include <kis_assert.h>

#include <algorithm>
#include <deque>

unsigned int qHash(const KoColor &color, unsigned int seed = 0)
{
    // hash the color data bytes, while using the hash of the colorspace pointer as seed
    // TODO: take pixelSize directly from the color.m_size (private member)
    // （原实现用 Qt 的 qHashBits + qHash(pointer)；Pk 侧无 qHashBits，哈希数值
    //   不必与 Qt 逐位相同，只要签名形状一致、值稳定。）
    const quint8 *data = color.data();
    const int size = color.colorSpace()->pixelSize();
    unsigned int h = qHash(color.colorSpace(), seed);
    for (int i = 0; i < size; ++i) {
        h = pkHashMix64(h ^ static_cast<unsigned int>(data[i]), h);
    }
    return h;
}

struct KisUniqueColorSet::ColorEntry
{
    static bool less(const KisUniqueColorSet::ColorEntry *lhs, const KisUniqueColorSet::ColorEntry *rhs)
    {
        // larger key == more recent == earlier in list
        return (lhs->key > rhs->key);
    }

    KoColor color;
    quint64 key;
};

struct KisUniqueColorSet::Private
{
    PkHash<KoColor, KisUniqueColorSet::ColorEntry*> colorHash;
    std::deque<ColorEntry*> history;
    size_t maxSize {200};
    quint64 key {0};
};

KisUniqueColorSet::KisUniqueColorSet(PkObject *parent)
    : PkObject(parent)
    , d(new Private)
{ }

KisUniqueColorSet::~KisUniqueColorSet()
{
    for (ColorEntry *entry: d->history) {
        delete entry;
    }
}

void KisUniqueColorSet::addColor(const KoColor &color)
{
    auto hashEntry = d->colorHash.find(color);
    if (hashEntry != d->colorHash.end()) {
        auto historyEl = std::lower_bound(d->history.begin(), d->history.end(), *hashEntry, &ColorEntry::less);
        if (historyEl != d->history.end()) {
            int oldPos = historyEl - d->history.begin();
            if (historyEl == d->history.begin()) {
                KIS_ASSERT((*historyEl)->key == d->key);
                return;
            }
            ColorEntry *node = *historyEl;
            d->history.erase(historyEl);
            node->key = ++d->key;
            d->history.push_front(node);
            sigColorMoved(oldPos, 0);
        }
        else {
            qDebug() << "inconsistent color history state!";
        }
    }
    else {
        ColorEntry *entry;
        if (d->history.size() >= d->maxSize) {
            entry = d->history.back();
            d->history.pop_back();
            KIS_ASSERT(d->colorHash.remove(entry->color) == 1);
            entry->color = color;
            entry->key = ++d->key;
            sigColorRemoved(d->maxSize - 1);
        }
        else {
            entry = new ColorEntry {color, ++d->key};
        }
        d->colorHash.insert(color, entry);
        d->history.push_front(entry);
        sigColorAdded(0);
    }
}

KoColor KisUniqueColorSet::color(int index) const
{
    if (index < 0 || index >= static_cast<int>(d->history.size())) {
        return KoColor();
    }
    return d->history.at(index)->color;
}

int KisUniqueColorSet::size() const
{
    return static_cast<int>(d->history.size());
}

void KisUniqueColorSet::clear()
{
    for (ColorEntry *entry: d->history) {
        delete entry;
    }
    d->history.clear();
    d->colorHash.clear();
    d->key = 0;
    sigReset();
}
