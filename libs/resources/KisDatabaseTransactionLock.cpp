/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDatabaseTransactionLock.h"

#include <PkMessageLogger.h>

#include <kis_assert.h>

#include <sqlite3.h>


namespace detail
{

std::recursive_mutex &resourceDatabaseConnectionMutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}

void *lockResourceDatabaseNativeMutex(PkSqlDatabase database)
{
    sqlite3 *handle = database.PkHandle();
    sqlite3_mutex *mutex = handle ? sqlite3_db_mutex(handle) : nullptr;
    if (mutex) {
        sqlite3_mutex_enter(mutex);
    }
    return mutex;
}

void unlockResourceDatabaseNativeMutex(void *nativeMutex)
{
    if (nativeMutex) {
        sqlite3_mutex_leave(static_cast<sqlite3_mutex *>(nativeMutex));
    }
}

KisDatabaseTransactionLockAdapter::KisDatabaseTransactionLockAdapter(PkSqlDatabase database)
    : m_database(database)
{
}

void KisDatabaseTransactionLockAdapter::lock()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_transactionStarted);
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_connectionMutexLocked);

    resourceDatabaseConnectionMutex().lock();
    m_connectionMutexLocked = true;
    m_nativeMutex = lockResourceDatabaseNativeMutex(m_database);
    if (!m_database.transaction()) {
        qWarning() << "WARNING: Failed to start a transaction:" << m_database.lastError().text();
        releaseConnectionLocks();
    } else {
        m_transactionStarted = true;
    }
}

void KisDatabaseTransactionLockAdapter::unlock()
{
    if (!m_transactionStarted) {
        releaseConnectionLocks();
        return;
    }

    if (!m_database.rollback()) {
        qWarning() << "WARNING: Failed to rollback a transaction:" << m_database.lastError().text();
    }

    m_transactionStarted = false;
    releaseConnectionLocks();
}

bool KisDatabaseTransactionLockAdapter::commit()
{
    if (!m_transactionStarted) {
        releaseConnectionLocks();
        return false;
    }

    const bool committed = m_database.commit();
    if (!committed) {
        qWarning() << "WARNING: Failed to commit a transaction:" << m_database.lastError().text();
        if (!m_database.rollback()) {
            qWarning() << "WARNING: Failed to rollback after commit failure:"
                       << m_database.lastError().text();
        }
    }

    m_transactionStarted = false;
    releaseConnectionLocks();
    return committed;
}

void KisDatabaseTransactionLockAdapter::releaseConnectionLocks()
{
    if (!m_connectionMutexLocked) {
        return;
    }
    unlockResourceDatabaseNativeMutex(m_nativeMutex);
    m_nativeMutex = nullptr;
    m_connectionMutexLocked = false;
    resourceDatabaseConnectionMutex().unlock();
}

} // namespace detail
