/*
 *  SPDX-FileCopyrightText: 2024 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoAbstractCanvasResourceInterface.h"
// [migrate] missing include for Pk/Qt type
#include <PkString.h>
#include <PkVariant.h>

KoAbstractCanvasResourceInterface::KoAbstractCanvasResourceInterface(int key, const PkString debugTag)
    : m_key(key)
    , m_debugTag(debugTag)
{
}

int KoAbstractCanvasResourceInterface::key() const {
    return m_key;
}

void KoAbstractCanvasResourceInterface::sigResourceChangedExternal(
    int key, const PkVariant &value)
{
    activateSignal<int, const PkVariant &>(
        this,
        PkMemberFnKey::from(&KoAbstractCanvasResourceInterface::sigResourceChangedExternal),
        key,
        value);
}
