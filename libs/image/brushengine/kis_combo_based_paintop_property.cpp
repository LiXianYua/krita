/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_combo_based_paintop_property.h"
#include "kis_paintop_settings.h"


struct KisComboBasedPaintOpProperty::Private
{
    PkList<PkString> items;
};

KisComboBasedPaintOpProperty::KisComboBasedPaintOpProperty(const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent)
    : KisUniformPaintOpProperty(Combo, id, settings, parent)
    , m_d(new Private)
{
}

KisComboBasedPaintOpProperty::KisComboBasedPaintOpProperty(Type type, const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent)
    : KisUniformPaintOpProperty(Combo, id, settings, parent)
    , m_d(new Private)
{
    KIS_ASSERT_RECOVER_RETURN(type == Combo);
}

KisComboBasedPaintOpProperty::KisComboBasedPaintOpProperty(Type type, SubType subType, const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent)
    : KisUniformPaintOpProperty(Combo, subType, id, settings, parent)
    , m_d(new Private)
{
    KIS_ASSERT_RECOVER_RETURN(type == Combo);
}

KisComboBasedPaintOpProperty::~KisComboBasedPaintOpProperty()
{
}

PkList<PkString> KisComboBasedPaintOpProperty::items() const
{
    return m_d->items;
}

void KisComboBasedPaintOpProperty::setItems(const PkList<PkString> &list)
{
    m_d->items = list;
}

#include "kis_callback_based_paintop_property_impl.h"
template class KRITAIMAGE_EXPORT_INSTANCE
    KisCallbackBasedPaintopProperty<KisComboBasedPaintOpProperty>;
