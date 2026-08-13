/*
 * SPDX-FileCopyrightText: 2008, 2011 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_assistant_handle_p.h"

#include "kis_debug.h"

KisPaintingAssistantHandle::KisPaintingAssistantHandle(double x, double y)
    : QPointF(x, y)
    , d(new Private)
{
}

KisPaintingAssistantHandle::KisPaintingAssistantHandle(QPointF p)
    : QPointF(p)
    , d(new Private)
{
}

KisPaintingAssistantHandle::KisPaintingAssistantHandle(const KisPaintingAssistantHandle &rhs)
    : QPointF(rhs)
    , KisShared()
    , d(new Private)
{
    dbgUI << "KisPaintingAssistantHandle ctor";
}

KisPaintingAssistantHandle &KisPaintingAssistantHandle::operator=(const QPointF &pt)
{
    setX(pt.x());
    setY(pt.y());
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
    Q_ASSERT(d->assistants.empty());
    delete d;
}

void KisPaintingAssistantHandle::registerAssistant(KisPaintingAssistant *assistant)
{
    Q_ASSERT(!d->assistants.contains(assistant));
    d->assistants.append(assistant);
}

void KisPaintingAssistantHandle::unregisterAssistant(KisPaintingAssistant *assistant)
{
    d->assistants.removeOne(assistant);
    Q_ASSERT(!d->assistants.contains(assistant));
}

bool KisPaintingAssistantHandle::containsAssistant(KisPaintingAssistant *assistant) const
{
    return d->assistants.contains(assistant);
}
