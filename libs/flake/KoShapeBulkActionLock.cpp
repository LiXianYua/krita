/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoShapeBulkActionLock.h>

#include <kis_debug.h>
#include <FlakeDebug.h>
#include <KoShape.h>

#include <KoShapeBulkActionInterface.h>

KoShapeBulkActionLockAdapter::KoShapeBulkActionLockAdapter(const PkList<KoShape*> &shapes)
{
    for (KoShape *shape : shapes) {
        // explicitly called shaped will be "updated" in both
        // ways, using normal updates and dependent bulk updates
        m_normalUpdateShapes.append(shape);
        tryAddBulkInterfaceShape(shape);
        addBulkInterfaceDependees(shape->dependees());
    }
}

void KoShapeBulkActionLockAdapter::tryAddBulkInterfaceShape(KoShape *shape)
{
    KoShapeBulkActionInterface *iface = dynamic_cast<KoShapeBulkActionInterface *>(shape);
    if (iface) {
        // postpone update till the end, if the shape is present
        // multiple times
        if (m_bulkInterfaceShapes.contains(iface)) {
            m_bulkInterfaceShapes.removeOne(iface);
        }
        m_bulkInterfaceShapes.append(iface);
    }
};

void KoShapeBulkActionLockAdapter::addBulkInterfaceDependees(const PkList<KoShape*> dependees)
{
    for (KoShape *shape : dependees) {
        tryAddBulkInterfaceShape(shape);
        addBulkInterfaceDependees(shape->dependees());
    }
}

void KoShapeBulkActionLockAdapter::lock()
{
    m_finalUpdates.clear();

    for (KoShape *shape : m_normalUpdateShapes) {
        m_finalUpdates[shape] |= shape->boundingRect();
    }

    for (KoShapeBulkActionInterface *iface : m_bulkInterfaceShapes) {
        iface->startBulkAction();
    }

}

void KoShapeBulkActionLockAdapter::unlock()
{
    for (KoShapeBulkActionInterface *iface : m_bulkInterfaceShapes) {
        const PkRectF update = iface->endBulkAction();

        KoShape *shape = dynamic_cast<KoShape*>(iface);
        KIS_SAFE_ASSERT_RECOVER(shape) { continue; }
        m_finalUpdates[shape] |= update;
    }

    for (KoShape *shape : m_normalUpdateShapes) {
        m_finalUpdates[shape] |= shape->boundingRect();
    }
}

KoShapeBulkActionLockAdapter::UpdatesList
KoShapeBulkActionLockAdapter::takeFinalUpdatesList()
{
    UpdatesList result;
    result.reserve(m_finalUpdates.size());

    for (auto it = m_finalUpdates.begin(); it != m_finalUpdates.end(); ++it) {
        result.emplace_back(it->first, it->second);
    }

    return result;
}

KoShapeBulkActionLock::~KoShapeBulkActionLock()
{
    if (owns_lock()) {
        warnFlake << "WARNING: KoShapeBulkActionLock is destroyed while being locked. Update rect will be lost!";
    }
}

void KoShapeBulkActionLock::bulkShapesUpdate(const UpdatesList &updates)
{
    // TODO: implement optimization for shape managers merging
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        it->first->updateAbsolute(it->second);
    }
}
