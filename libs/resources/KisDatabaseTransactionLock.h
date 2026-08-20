/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISDATABASETRANSACTIONLOCK_H
#define KISDATABASETRANSACTIONLOCK_H

#include <PkSqlDatabase.h>

#include <mutex>

#include <kritaresources_export.h>
#include <KisAdaptedLock.h>

namespace detail
{
/**
 * PkSqlDatabase is a facade over one process-wide SQLite connection.  Every
 * multi-statement transaction in resources takes this recursive mutex for its
 * whole lifetime; the native SQLite connection mutex is held as well so
 * single-statement users of the same connection cannot interleave with it.
 */
KRITARESOURCES_EXPORT std::recursive_mutex &resourceDatabaseConnectionMutex();
KRITARESOURCES_EXPORT void *lockResourceDatabaseNativeMutex(PkSqlDatabase database);
KRITARESOURCES_EXPORT void unlockResourceDatabaseNativeMutex(void *nativeMutex);

struct KRITARESOURCES_EXPORT KisDatabaseTransactionLockAdapter {
    explicit KisDatabaseTransactionLockAdapter(PkSqlDatabase database);

    void lock();
    void unlock();
    bool commit();
    bool transactionStarted() const { return m_transactionStarted; }

private:
    void releaseConnectionLocks();

    PkSqlDatabase m_database;
    bool m_transactionStarted {false};
    bool m_connectionMutexLocked {false};
    void *m_nativeMutex {nullptr};
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
    using BaseClass::transactionStarted;

    void rollback() { unlock(); }
};

#endif /* KISDATABASETRANSACTIONLOCK_H */
