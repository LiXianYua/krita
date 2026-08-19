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
    Q_UNUSED(value);
}

void KoFakeProgressProxy::setRange(int minimum, int maximum)
{
    Q_UNUSED(minimum);
    Q_UNUSED(maximum);
}

void KoFakeProgressProxy::setFormat(const PkString &format)
{
    Q_UNUSED(format);
}

void KoFakeProgressProxy::setAutoNestedName(const PkString &name)
{
    Q_UNUSED(name);
}

KoProgressProxy *KoFakeProgressProxy::instance()
{
    return s_instance();
}
