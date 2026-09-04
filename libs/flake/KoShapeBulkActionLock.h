/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSHAPEBULKACTIONLOCK_H
#define KOSHAPEBULKACTIONLOCK_H

#include "kritaflake_export.h"

#include <KisAdaptedLock.h>
#include <PkList.h>
#include <PkRect.h>

class KoShape;
struct KoShapeBulkActionInterface;

class KRITAFLAKE_EXPORT KoShapeBulkActionLockAdapter {
public:
    using Update = std::pair<KoShape*, PkRectF>;
    using UpdatesList = std::vector<Update>;
public:
    KoShapeBulkActionLockAdapter(const PkList<KoShape*> &shapes);
    void lock();
    void unlock();

    UpdatesList takeFinalUpdatesList();

private:
    void tryAddBulkInterfaceShape(KoShape *shape);
    void addBulkInterfaceDependees(const PkList<KoShape*> dependees);

private:
    PkList<KoShapeBulkActionInterface*> m_bulkInterfaceShapes;
    PkList<KoShape*> m_normalUpdateShapes;
    std::unordered_map<KoShape*, PkRectF> m_finalUpdates;
};

class KRITAFLAKE_EXPORT KoShapeBulkActionLock : protected KisAdaptedLock<KoShapeBulkActionLockAdapter>
{
public:
    using BaseClass = KisAdaptedLock<KoShapeBulkActionLockAdapter>;
    using BaseClass::BaseClass;

    template <typename T, typename = std::enable_if_t<std::is_base_of_v<KoShape, T>>>
    KoShapeBulkActionLock(T *shape)
        : BaseClass(PkList<KoShape*>{shape})
    {}

    ~KoShapeBulkActionLock();

    using BaseClass::Update;
    using BaseClass::UpdatesList;

    using BaseClass::lock;

    [[nodiscard]] UpdatesList unlock() {
        BaseClass::unlock();
        return KoShapeBulkActionLockAdapter::takeFinalUpdatesList();
    }

    using BaseClass::owns_lock;
    using BaseClass::swap;
    using BaseClass::release;
    using BaseClass::operator bool;

    static void bulkShapesUpdate(const UpdatesList &updates);
};

#endif /* KOSHAPEBULKACTIONLOCK_H */
