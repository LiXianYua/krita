/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_COLOR_MANAGER_H
#define KIS_COLOR_MANAGER_H

#include <compat/QObject>
#include "pk/string/PkString.h"
#include "pk/container/PkStringList.h"
#include "pk/variant/PkAuxTypes.h"

#include "kritacolor_export.h"
/**
 * @brief The KisColorManager class can be used as a cross-platform way to get the
 * display profile associated with a device.
 *
 * TODO: support other devices than monitors
 */
class KRITACOLOR_EXPORT KisColorManager : public PkObject
{
    Q_OBJECT

public:
    explicit KisColorManager();
    ~KisColorManager() override;

    enum DeviceType {
        screen,
        printer,
        camera,
        scanner
    };

    /// Return the user-visible name for the given device
    PkString deviceName(const PkString &id);

    /// Return a list of device id's for the specified type
    PkStringList devices(DeviceType type = screen) const;

    /// Return the icc profile for the given device and index (if a device has more than one profile)
    PkByteArray displayProfile(const PkString &device, int profile = 0) const;

    static KisColorManager *instance();

Q_SIGNALS:

    void changed(const PkString device);

public Q_SLOTS:

private:

    KisColorManager(const KisColorManager&);
    KisColorManager operator=(const KisColorManager&);

    class Private;
    const Private *const d;
};

#endif // KIS_COLOR_MANAGER_H
