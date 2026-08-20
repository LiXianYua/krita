/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISDATABASETRANSACTIONLOCK_H
#define KISDATABASETRANSACTIONLOCK_H

#include <PkSqlDatabase.h>

#include <kritaresources_export.h>
#include <KisAdaptedLock.h>

namespace detail
{
struct KRITARESOURCES_EXPORT KisDatabaseTransactionLockAdapter {
    explicit KisDatabaseTransactionLockAdapter(PkSqlDatabase database);

    void lock();
    void unlock();
    void commit();

private:
    PkSqlDatabase m_database;
    bool m_transactionStarted {false};
};
} // namespace detail

/** RAII transaction: destruction rolls back until commit() defuses the lock. */
class KRITARESOURCES_EXPORT KisDatabaseTransactionLock
    : public KisAdaptedLock<detail::KisDatabaseTransactionLockAdapter>
{
public:
    using BaseClass = KisAdaptedLock<detail::KisDatabaseTransactionLockAdapter>;
    using BaseClass::BaseClass;
    using BaseClass::commit;

    void rollback() { unlock(); }
};

#endif /* KISDATABASETRANSACTIONLOCK_H */
