/*
 * SPDX-FileCopyrightText: 2008, 2011 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_assistant_handle_p.h"


KisPaintingAssistantHandle::KisPaintingAssistantHandle(double x, double y)
    : PkPointF(x, y)
    , d(new Private)
{
}

KisPaintingAssistantHandle::KisPaintingAssistantHandle(PkPointF p)
    : PkPointF(p)
    , d(new Private)
{
}

KisPaintingAssistantHandle::KisPaintingAssistantHandle(const KisPaintingAssistantHandle &rhs)
    : PkPointF(rhs)
    , KisShared()
    , d(new Private)
{
}

KisPaintingAssistantHandle &KisPaintingAssistantHandle::operator=(const PkPointF &pt)
{
    setX(pt.x());
    setY(pt.y());
    uncache();
    return *this;
}

void KisPaintingAssistantHandle::setType(char type)
{
    d->handleType = type;
}

char KisPaintingAssistantHandle::handleType() const
{
    return d->handleType;
}

KisPaintingAssistant *KisPaintingAssistantHandle::chiefAssistant() const
{
    return !d->assistants.isEmpty() ? d->assistants.first() : nullptr;
}

KisPaintingAssistantHandle::~KisPaintingAssistantHandle()
{
    assert(d->assistants.empty());
    delete d;
}

void KisPaintingAssistantHandle::registerAssistant(KisPaintingAssistant *assistant)
{
    assert(!d->assistants.contains(assistant));
    d->assistants.append(assistant);
}

void KisPaintingAssistantHandle::unregisterAssistant(KisPaintingAssistant *assistant)
{
    d->assistants.removeOne(assistant);
    assert(!d->assistants.contains(assistant));
}

bool KisPaintingAssistantHandle::containsAssistant(KisPaintingAssistant *assistant) const
{
    return d->assistants.contains(assistant);
}
