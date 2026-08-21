/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoFakeProgressProxy.h"

#include <cstdint>
#include <algorithm>
#include <cmath>

static KoFakeProgressProxy *s_instance()
{
    static KoFakeProgressProxy instance;
    return &instance;
}

int KoFakeProgressProxy::maximum() const
{
    return 100;
}

void KoFakeProgressProxy::setValue(int value)
{
    (void)value;
}

void KoFakeProgressProxy::setRange(int minimum, int maximum)
{
    (void)minimum;
    (void)maximum;
}

void KoFakeProgressProxy::setFormat(const PkString &format)
{
    (void)format;
}

void KoFakeProgressProxy::setAutoNestedName(const PkString &name)
{
    (void)name;
}

KoProgressProxy *KoFakeProgressProxy::instance()
{
    return s_instance();
}
