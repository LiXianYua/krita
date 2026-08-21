/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_COMBO_BASED_PAINTOP_PROPERTY_H
#define __KIS_COMBO_BASED_PAINTOP_PROPERTY_H

#include <PkScopedPointer.h>
#include <PkObject.h>
#include <PkList.h>
#include <PkString.h>

#include "kis_image_export.h"
#include "kis_types.h"
#include "kis_uniform_paintop_property.h"


class KRITAIMAGE_EXPORT KisComboBasedPaintOpProperty : public KisUniformPaintOpProperty
{
public:
    KisComboBasedPaintOpProperty(const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent);
    ~KisComboBasedPaintOpProperty() override;

    // callback-compatible c-tor
    KisComboBasedPaintOpProperty(Type type, SubType subType, const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent);
    KisComboBasedPaintOpProperty(Type type, const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent);

    PkList<PkString> items() const;
    void setItems(const PkList<PkString> &list);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#include "kis_callback_based_paintop_property.h"
extern template class KisCallbackBasedPaintopProperty<KisComboBasedPaintOpProperty>;
using KisComboBasedPaintOpPropertyCallback =
    KisCallbackBasedPaintopProperty<KisComboBasedPaintOpProperty>;

#endif /* __KIS_COMBO_BASED_PAINTOP_PROPERTY_H */
