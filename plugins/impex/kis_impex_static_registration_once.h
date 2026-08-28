/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_IMPEX_STATIC_REGISTRATION_ONCE_H
#define KIS_IMPEX_STATIC_REGISTRATION_ONCE_H

#include <utility>

template<typename Registration>
bool invokeKisImpexRegistrationOnce(bool &registered, Registration &&registration)
{
    if (registered) {
        return false;
    }

    registered = true;
    std::forward<Registration>(registration)();
    return true;
}

#endif
