/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDatabaseTransactionLock.h"

#include <PkMessageLogger.h>

#include <kis_assert.h>

#include <sqlite3.h>

#include <atomic>


namespace detail
{

namespace {

std::atomic<bool> &connectionPoisoned()
{
    static std::atomic<bool> poisoned {false};
    return poisoned;
}

} // namespace

void poisonResourceDatabaseConnection(PkSqlDatabase database,
                                      ResourceDatabaseConnectionGuard &connectionGuard,
                                      const char *operation) noexcept
{
    connectionPoisoned().store(true, std::memory_order_release);

    // sqlite3_close() invalidates the connection mutex when it succeeds, so
    // release the native mutex first while retaining the resources mutex.
    // PkSqlDatabase::close() also clears its process-wide facade handle when
    // SQLite reports BUSY; the poison bit then prevents accidental reopen/use.
    connectionGuard.releaseNativeMutex();
    try {
        database.close();
    } catch (...) {
    }
    try {
        qWarning() << "Resource database connection poisoned after" << operation;
    } catch (...) {
    }
}

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

ResourceDatabaseConnectionGuard::ResourceDatabaseConnectionGuard(PkSqlDatabase database)
    : m_connectionLock(resourceDatabaseConnectionMutex())
    , m_nativeMutex(lockResourceDatabaseNativeMutex(database))
{
}

ResourceDatabaseConnectionGuard::~ResourceDatabaseConnectionGuard() noexcept
{
    releaseNativeMutex();
}

void ResourceDatabaseConnectionGuard::releaseNativeMutex() noexcept
{
    unlockResourceDatabaseNativeMutex(m_nativeMutex);
    m_nativeMutex = nullptr;
}

bool resourceDatabaseConnectionIsPoisoned()
{
    return connectionPoisoned().load(std::memory_order_acquire);
}

PkString resourceDatabaseConnectionPoisonError()
{
    return PkString("Resource database connection is poisoned and closed after "
                    "transaction cleanup failed");
}

bool ensureResourceDatabaseAutocommitState(
    PkSqlDatabase database,
    bool expectedAutocommit,
    ResourceDatabaseConnectionGuard &connectionGuard,
    const char *operation) noexcept
{
    sqlite3 *handle = database.PkHandle();
    const bool stateMatches = handle &&
        (sqlite3_get_autocommit(handle) != 0) == expectedAutocommit;
    if (stateMatches) {
        return true;
    }
    poisonResourceDatabaseConnection(database, connectionGuard, operation);
    return false;
}

KisDatabaseTransactionLockAdapter::KisDatabaseTransactionLockAdapter(PkSqlDatabase database)
    : m_database(database)
{
}

void KisDatabaseTransactionLockAdapter::lock()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_transactionStarted);
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_connectionGuard);

    if (resourceDatabaseConnectionIsPoisoned()) {
        if (m_database.isOpen()) {
            m_database.close();
        }
        qWarning() << resourceDatabaseConnectionPoisonError();
        return;
    }

    // Allocate the owning guard before it acquires either mutex.  Keeping the
    // guard local until BEGIN succeeds makes every allocation/SQLite exception
    // unwind both lock layers in native-then-outer order.
    auto connectionGuard =
        std::make_unique<ResourceDatabaseConnectionGuard>(m_database);
    if (!m_database.transaction()) {
        qWarning() << "WARNING: Failed to start a transaction:" << m_database.lastError().text();
    } else {
        m_connectionGuard = std::move(connectionGuard);
        m_transactionStarted = true;
    }
}

void KisDatabaseTransactionLockAdapter::unlock() noexcept
{
    // Move ownership to a local first: even rollback/error-reporting throws,
    // both mutexes are released while unwinding this no-throw boundary.
    auto connectionGuard = std::move(m_connectionGuard);
    const bool transactionStarted = m_transactionStarted;
    m_transactionStarted = false;

    if (transactionStarted) {
        bool rollbackAttempted = false;
        try {
            rollbackAttempted = m_database.rollback();
            if (!rollbackAttempted) {
                qWarning() << "WARNING: Failed to rollback a transaction:"
                           << m_database.lastError().text();
            }
        } catch (...) {
            // Lockable::unlock() is a no-throw destructor boundary.  The local
            // connectionGuard still releases both serialization layers.
        }
        if (connectionGuard &&
            !ensureResourceDatabaseAutocommitState(m_database,
                                                   true,
                                                   *connectionGuard,
                                                   rollbackAttempted
                                                       ? "rollback"
                                                       : "failed rollback")) {
            // The verifier already poisoned and closed the facade before
            // this noexcept cleanup path releases the outer mutex.
        }
    }
}

bool KisDatabaseTransactionLockAdapter::commit()
{
    if (!m_transactionStarted) {
        releaseConnectionLocks();
        return false;
    }

    auto connectionGuard = std::move(m_connectionGuard);
    m_transactionStarted = false;

    bool committed = false;
    try {
        committed = m_database.commit();
        if (!committed) {
            qWarning() << "WARNING: Failed to commit a transaction:"
                       << m_database.lastError().text();
            if (!m_database.rollback()) {
                qWarning() << "WARNING: Failed to rollback after commit failure:"
                           << m_database.lastError().text();
            }
        }
    } catch (...) {
        try {
            (void)m_database.rollback();
        } catch (...) {
        }
        committed = false;
    }
    if (connectionGuard &&
        !ensureResourceDatabaseAutocommitState(m_database,
                                               true,
                                               *connectionGuard,
                                               committed ? "commit"
                                                         : "commit cleanup")) {
        committed = false;
    }
    return committed;
}

void KisDatabaseTransactionLockAdapter::releaseConnectionLocks()
{
    m_connectionGuard.reset();
}

} // namespace detail
