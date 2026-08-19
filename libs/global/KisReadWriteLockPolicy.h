/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISREADWRITELOCKPOLICY_H
#define KISREADWRITELOCKPOLICY_H

#include <boost/utility.hpp>

#include <PkReadWriteLock.h>
#include <PkReadWriteLock.h>
#include "KisUpgradeToWriteLocker.h"

struct NormalLockPolicy {
    typedef PkReadLocker ReadLocker;
    typedef PkWriteLocker WriteLocker;
};

struct UpgradeLockPolicy {
    struct FakeLocker : private boost::noncopyable {
        FakeLocker(PkReadWriteLock *) {}
    };

    typedef FakeLocker ReadLocker;
    typedef KisUpgradeToWriteLocker WriteLocker;
};

struct NoLockPolicy {
    struct FakeLocker : private boost::noncopyable {
        FakeLocker(PkReadWriteLock *) {}
    };

    typedef FakeLocker ReadLocker;
    typedef FakeLocker WriteLocker;
};
#endif // KISREADWRITELOCKPOLICY_H
