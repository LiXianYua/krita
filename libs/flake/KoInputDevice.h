/*
 *  SPDX-FileCopyrightText: 2006 Adrian Page <adrian@pagenet.plus.com>
 *  SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KO_INPUT_DEVICE_H_
#define KO_INPUT_DEVICE_H_

#include "kritaflake_export.h"

#include <PkDebug.h>
#include <PkFlakeBridge.h>
#include <boost/operators.hpp>

/**
 * This class represents an input device.
 * A user can manipulate flake-shapes using a large variety of input devices. This ranges from
 * a mouse to a paintbrush-like tool connected to a tablet. */
class KRITAFLAKE_EXPORT KoInputDevice : public boost::equality_comparable<KoInputDevice>
{
public:

    enum class Pointer {
        Unknown = 0,
        Generic = 0x0001,   // mouse or similar
        Finger = 0x0002,    // touchscreen or pad
        Pen = 0x0004,       // stylus on a tablet
        Eraser = 0x0008,    // eraser end of a stylus
        Cursor = 0x0010,    // digitizer with crosshairs
        AllPointerTypes = 0x7FFF
    };

    enum class InputDevice {
        Unknown = 0x0000,
        Mouse = 0x0001,
        TouchScreen = 0x0002,
        TouchPad = 0x0004,
        Puck = 0x0008,
        Stylus = 0x0010,
        Airbrush = 0x0020,
        Keyboard = 0x1000,
        AllDevices = 0x7FFFFFFF
    };

    /**
     * Copy constructor.
     */
    KoInputDevice(const KoInputDevice &other);

    /**
     * Constructor for a tablet.
     * Create a new input device with one of the many types that the tablet can have.
     * The host classifies its own platform event and supplies the result here.
     * @param device the device reported by the host for the current input
     * @param pointer the pointer reported by the host for the current input
     * @param uniqueTabletId the unique id reported by the host for the current input
     */
    explicit KoInputDevice(InputDevice device, Pointer pointer, qint64 uniqueTabletId = -1);

    /**
     * Constructor for the mouse as input device.
     */
    KoInputDevice();

    ~KoInputDevice();

    /**
     * Return the tablet device used
     */
    InputDevice device() const;

    /**
     * Return the pointer used
     */
    Pointer pointer() const;

    /**
     * Return the unique tablet id as registered by the host for the current input. Note that this
     * id can change randomly, so it's not dependable.
     *
     * See https://bugs.kde.org/show_bug.cgi?id=407659
     */
    qint64 uniqueTabletId() const;

    /**
     * Return if this is a mouse device.
     */
    bool isMouse() const;

    /// equal
    bool operator==(const KoInputDevice&) const;
    /// assignment
    KoInputDevice & operator=(const KoInputDevice &);

    static KoInputDevice invalid();   ///< invalid input device
    static KoInputDevice mouse();     ///< Standard mouse
    static KoInputDevice stylus();    ///< Wacom style/pen
    static KoInputDevice eraser();    ///< Wacom eraser


private:
    class Private;
    Private * const d;
};

Q_DECLARE_METATYPE(KoInputDevice)

KRITAFLAKE_EXPORT PkDebug operator<<(PkDebug debug, const KoInputDevice &device);

inline uint qHash(const KoInputDevice &key)
{
    return pkHash(toQString(PkString(":%1:%2:%3:%4")
                     .arg(int(key.device()))
                     .arg(int(key.pointer()))
                     .arg(int(key.uniqueTabletId()))
                     .arg(int(key.isMouse()))));
}

#endif

