/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoToolSelection.h"

#include "KoToolBase.h"

KoToolSelection::KoToolSelection(QObject *parent)
        : QObject(parent)
{
}

KoToolSelection::~KoToolSelection()
{
}

// Bucket-agnostic forward declared in KoToolBase.h. Defined here, in the Qt
// translation unit, so that the dereference of KoToolSelection uses the real
// (Qt) layout — KoToolSelection's base list is spelled per configuration.
bool KoToolBase::selectionHasSelection()
{
    KoToolSelection *sel = selection();
    return (sel && sel->hasSelection());
}
