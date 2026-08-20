/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KisResourcesInterface_P_H
#define KisResourcesInterface_P_H

#include "kritaresources_export.h"
#include "KisResourcesInterface.h"
#include <unordered_map>
#include <memory>

#include <PkReadWriteLock.h>
#include <PkStringHash.h>

#include "kis_assert.h"

namespace {

struct PkStringHasher
{
    std::size_t operator()(const PkString &s) const noexcept {
        return static_cast<std::size_t>(qHash(s));
    }
};

}

class KRITARESOURCES_EXPORT KisResourcesInterfacePrivate
{
public:
    mutable std::unordered_map<PkString,
                       std::unique_ptr<
                           KisResourcesInterface::ResourceSourceAdapter>, PkStringHasher> sourceAdapters;
    mutable PkReadWriteLock lock;

    KisResourcesInterface::ResourceSourceAdapter* findExistingSource(const PkString &type) const {
        auto it = this->sourceAdapters.find(type);
        if (it != this->sourceAdapters.end()) {
            KIS_ASSERT(bool(it->second));

            return it->second.get();
        }

        return nullptr;
    }

    virtual ~KisResourcesInterfacePrivate() {}
};

#endif // KisResourcesInterface_P_H
