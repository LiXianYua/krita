/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoUpdaterPrivate_p.h"
#include <KoUpdater.h>

KoUpdaterPrivate::KoUpdaterPrivate(int weight, const PkString &name, bool isPersistent)
    : PkObject(0)
    , m_progress(0)
    , m_weight(weight)
    , m_interrupted(false)
    , m_autoNestedName()
    , m_subTaskName(name)
    , m_hasValidRange(true)
    , m_isPersistent(isPersistent)
    , m_connectedUpdater(new KoUpdater(this))
{
}

KoUpdaterPrivate::~KoUpdaterPrivate()
{
    setInterrupted(true);
    m_connectedUpdater->deleteLater();
}

PkString KoUpdaterPrivate::autoNestedName() const
{
    return m_autoNestedName;
}

PkString KoUpdaterPrivate::subTaskName() const
{
    return m_subTaskName;
}

PkString KoUpdaterPrivate::mergedSubTaskName() const
{
   PkString result = m_subTaskName;

   if (!m_autoNestedName.isEmpty()) {
       if (result.isEmpty()) {
           result = m_autoNestedName;
       } else {
           result = PkString("%1: %2").arg(result).arg(m_autoNestedName);
       }
   }

   return result;
}

bool KoUpdaterPrivate::hasValidRange() const
{
    return m_hasValidRange;
}

bool KoUpdaterPrivate::isPersistent() const
{
    return m_isPersistent;
}

bool KoUpdaterPrivate::isCompleted() const
{
    return m_progress >= 100;
}

void KoUpdaterPrivate::cancel()
{
    Q_EMIT sigCancelled();
}

void KoUpdaterPrivate::setInterrupted(bool value)
{
    m_interrupted = value;
    m_connectedUpdater->setInterrupted(true);
}

void KoUpdaterPrivate::setProgress(int percent)
{
    m_progress = percent;
    Q_EMIT sigUpdated();
}

void KoUpdaterPrivate::setAutoNestedName(const PkString &name)
{
    m_autoNestedName = name;
    Q_EMIT sigUpdated();
}

void KoUpdaterPrivate::setHasValidRange(bool value)
{
    m_hasValidRange = value;
    Q_EMIT sigUpdated();
}

PkPointer<KoUpdater> KoUpdaterPrivate::connectedUpdater() const
{
    return m_connectedUpdater;
}
