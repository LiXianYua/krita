/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../kis_impex_static_registration_once.h"

#include <cstdlib>
#include <iostream>

namespace
{

[[noreturn]] void fail(const char *message)
{
    std::cerr << message << '\n';
    std::exit(1);
}

} // namespace

int main()
{
    bool registered = false;
    int registrationCount = 0;

    if (!invokeKisImpexRegistrationOnce(registered, [&registrationCount] {
            ++registrationCount;
        })) {
        fail("first registration was rejected");
    }
    if (registrationCount != 1) {
        fail("first registration did not invoke the callback exactly once");
    }

    if (invokeKisImpexRegistrationOnce(registered, [&registrationCount] {
            ++registrationCount;
        })) {
        fail("duplicate registration was accepted");
    }
    if (registrationCount != 1) {
        fail("duplicate registration invoked the callback");
    }

    return 0;
}
