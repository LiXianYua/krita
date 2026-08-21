/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_UNIFORM_PAINT_OP_PROPERTY_H
#define __KIS_UNIFORM_PAINT_OP_PROPERTY_H

#include <PkScopedPointer.h>
#include <PkObject.h>
#include <PkVariant.h>
#include <PkString.h>

#include "kis_image_export.h"
#include "kis_types.h"

class KRITAIMAGE_EXPORT KisUniformPaintOpProperty : public PkObject
{
    Q_OBJECT
public:
    enum Type {
        Int = 0,
        Double,
        Bool,
        Combo
    };

    /**
     * @brief Hint to guess what this property is used for
     */
    enum SubType {
        SubType_None = 0,
        SubType_Angle
    };

public:
    KisUniformPaintOpProperty(Type type, SubType subType, const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent);
    KisUniformPaintOpProperty(Type type, const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent);
    KisUniformPaintOpProperty(const KoID &id, KisPaintOpSettingsRestrictedSP settings, PkObject *parent);
    ~KisUniformPaintOpProperty() override;

    PkString id() const;
    PkString name() const;
    Type type() const;
    SubType subType() const;

    PkVariant value() const;

    KisPaintOpSettingsSP settings() const;

    virtual bool isVisible() const;

public Q_SLOTS:
    void setValue(const PkVariant &value);
    void requestReadValue();

Q_SIGNALS:
    void valueChanged(const PkVariant &value);

protected:
    virtual void readValueImpl();
    virtual void writeValueImpl();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

template<class T> class PkSharedPointer;
template<class T> class PkWeakPointer;
template<class T> class PkList;

using KisUniformPaintOpPropertySP = PkSharedPointer<KisUniformPaintOpProperty>;
using KisUniformPaintOpPropertyWSP = PkWeakPointer<KisUniformPaintOpProperty>;

#include "kis_callback_based_paintop_property.h"
extern template class KisCallbackBasedPaintopProperty<
    KisUniformPaintOpProperty>;
using KisUniformPaintOpPropertyCallback =
    KisCallbackBasedPaintopProperty<KisUniformPaintOpProperty>;

#endif /* __KIS_UNIFORM_PAINT_OP_PROPERTY_H */
