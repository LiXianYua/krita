/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoSelectedShapesProxy.h"

KoSelectedShapesProxy::KoSelectedShapesProxy(PkObject *parent)
    : PkObject(parent)
{
}

void KoSelectedShapesProxy::selectionChanged()
{
    activateSignal<>(this, PkMemberFnKey::from(&KoSelectedShapesProxy::selectionChanged));
}

void KoSelectedShapesProxy::selectionContentChanged()
{
    activateSignal<>(this, PkMemberFnKey::from(&KoSelectedShapesProxy::selectionContentChanged));
}

void KoSelectedShapesProxy::currentLayerChanged(const KoShapeLayer *layer)
{
    activateSignal<const KoShapeLayer *>(
        this, PkMemberFnKey::from(&KoSelectedShapesProxy::currentLayerChanged), layer);
}

bool KoSelectedShapesProxy::isRequestingToBeEdited()
{
    return m_isRequestingEditing;
}

void KoSelectedShapesProxy::setRequestingToBeEdited(bool value)
{
    m_isRequestingEditing = value;
}
