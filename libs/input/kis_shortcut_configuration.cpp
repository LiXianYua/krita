/*
 * This file is part of the KDE project
 * SPDX-FileCopyrightText: 2013 Arjen Hiemstra <ahiemstra@heimr.nl>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_shortcut_configuration.h"

#include <PkString.h>
#include <PkList.h>
#include <PkNamespace.h>

#include <vector>

namespace {

// PkString 无 Qt 的 number(v, 16) base-16 等价（migrate 表缺口，本任务登记）：
// serialize() 的十六进制写出。值类型（int/uint/enum/PkFlags）统一折算 unsigned int，
// 行为与 Qt 的 number(v, 16) 一致（无前导零、小写 hex）。
PkString pkIntToHex(unsigned int v)
{
    static const char hexdigits[] = "0123456789abcdef";
    char buf[32];
    int pos = 0;
    if (v == 0) {
        buf[pos++] = '0';
    }
    while (v > 0) {
        buf[pos++] = hexdigits[v & 0xF];
        v >>= 4;
    }
    char out[33];
    for (int i = 0; i < pos; ++i) {
        out[i] = buf[pos - 1 - i];
    }
    out[pos] = '\0';
    return PkString(out);
}

// PkString 无 toUInt(nullptr, 16)（migrate 表缺口，本任务登记）：unserialize() 的
// 十六进制读入。与 Qt 的 toUInt 一致：停在不合法字符、返回已解析部分；空串 0。
unsigned int pkHexToUInt(const PkString &s)
{
    unsigned int v = 0;
    for (int i = 0; i < s.size(); ++i) {
        const char16_t c = s.at(i);
        int d = -1;
        if (c >= u'0' && c <= u'9') {
            d = c - u'0';
        } else if (c >= u'a' && c <= u'f') {
            d = c - u'a' + 10;
        } else if (c >= u'A' && c <= u'F') {
            d = c - u'A' + 10;
        } else {
            break;
        }
        v = v * 16 + static_cast<unsigned int>(d);
    }
    return v;
}

// PkString 无 Qt 的 remove(char)（migrate 表缺口，本任务登记）：删除全部 sep 字符
// （split+join 实现，保留其余顺序）。
PkString pkRemoveChar(const PkString &s, char16_t sep)
{
    const std::vector<PkString> parts = s.split(sep);
    PkString out;
    for (const PkString &p : parts) {
        out.append(p);
    }
    return out;
}

// Qt::Key → 显示文本。可打印 ASCII 直接映射为单字符（Key_Space = 0x20 → " "），
// 其余回退占位。原 Qt 的 key→text 完整符号表在剥 Qt 后不可用——这是
// 零调用方死代码，显示行为变化登记显式接受（S-08-a brief Task 4 Step 3）。
PkString pkKeyToText(Qt::Key key)
{
    const char16_t c = static_cast<char16_t>(static_cast<int>(key));
    if (c >= 0x20 && c <= 0x7E) {
        char buf[2] = {static_cast<char>(c), '\0'};
        return PkString(buf);
    }
    return PkString("?");
}

} // namespace

class KisShortcutConfiguration::Private
{
public:
    Private()
        : action(0),
          type(UnknownType),
          mode(0),
          wheel(NoMovement),
          gesture(NoGesture)
    { }

    KisAbstractInputAction *action;
    ShortcutType type;
    uint mode;

    PkList<Qt::Key> keys;
    Qt::MouseButtons buttons;
    MouseWheelMovement wheel;
    GestureAction gesture;
};

KisShortcutConfiguration::KisShortcutConfiguration()
    : d(new Private)
{

}

KisShortcutConfiguration::KisShortcutConfiguration(const KisShortcutConfiguration &other)
    : d(new Private)
{
    d->action = other.action();
    d->type = other.type();
    d->mode = other.mode();
    d->keys = other.keys();
    d->buttons = other.buttons();
    d->wheel = other.wheel();
    d->gesture = other.gesture();
}

KisShortcutConfiguration &KisShortcutConfiguration::operator=(const KisShortcutConfiguration &other)
{
    d->action = other.action();
    d->type = other.type();
    d->mode = other.mode();
    d->keys = other.keys();
    d->buttons = other.buttons();
    d->wheel = other.wheel();
    d->gesture = other.gesture();

    return *this;
}

bool KisShortcutConfiguration::operator==(const KisShortcutConfiguration &other) const
{
    return d->type == other.d->type && d->keys == other.d->keys && d->buttons == other.d->buttons
        && d->wheel == other.d->wheel && d->gesture == other.d->gesture;
}

KisShortcutConfiguration::~KisShortcutConfiguration()
{
    delete d;
}

PkString KisShortcutConfiguration::serialize()
{
    PkString serialized("{");

    serialized.append(pkIntToHex(d->mode));
    serialized.append(PkString(";"));
#ifdef Q_OS_MACOS
    if (d->type == GestureType) {
        serialized.append(pkIntToHex(MacOSGestureType));
    } else {
        serialized.append(pkIntToHex(d->type));
    }
#else
    serialized.append(pkIntToHex(d->type));
#endif
    serialized.append(PkString(";["));

    for (PkList<Qt::Key>::iterator itr = d->keys.begin(); itr != d->keys.end(); ++itr) {
        serialized.append(pkIntToHex(*itr));

        if (itr + 1 != d->keys.end()) {
            serialized.append(PkString(","));
        }
    }

    serialized.append(PkString("];"));

    serialized.append(pkIntToHex(d->buttons));
    serialized.append(PkString(";"));
    serialized.append(pkIntToHex(d->wheel));
    serialized.append(PkString(";"));
    serialized.append(pkIntToHex(d->gesture));
    serialized.append(PkString("}"));

    return serialized;
}

bool KisShortcutConfiguration::unserialize(const PkString &serialized)
{
    if (!serialized.startsWith(PkString("{")))
        return false;

    //Parse the serialized data and apply it to the current shortcut
    PkString remainder = serialized;

    //Remove brackets
    remainder = pkRemoveChar(pkRemoveChar(remainder, '{'), '}');

    //Split the remainder by ;
    const std::vector<PkString> parts = remainder.split(';');

    if (parts.size() < 6)
        return false; //Invalid input, abort

    //First entry in the list is the mode
    d->mode = pkHexToUInt(parts.at(0));

    //Second entry is the shortcut type
    d->type = static_cast<ShortcutType>(pkHexToUInt(parts.at(1)));

    if (d->type == UnknownType) {
        //Reject input that would set this shortcut to "Unknown"
        return false;
    }

#ifdef Q_OS_MACOS
    // On MacOS, the GestureType gestures aren't handled. But! MacOSGestureType gestures are handled as
    // GestureTypes. Confusing? Yes, but this is done only here (and when serializing).
    if (d->type == GestureType) {
        return false;
    }
    if (d->type == MacOSGestureType) {
        d->type = GestureType;
    }
#else
    // only macOS platform handles these gestures
    if (d->type == MacOSGestureType) {
        return false;
    }
#endif

    //Third entry is the list of keys
    PkString serializedKeys = parts.at(2);
    //Remove brackets
    serializedKeys = pkRemoveChar(pkRemoveChar(serializedKeys, '['), ']');
    //Split by , and add each entry as a key
    const std::vector<PkString> keylist = serializedKeys.split(',');
    for (const PkString &key : keylist) {
        if (!key.isEmpty()) {
            d->keys.append(static_cast<Qt::Key>(pkHexToUInt(key)));
        }
    }

    //Fourth entry is the button mask
    d->buttons = static_cast<Qt::MouseButtons>(pkHexToUInt(parts.at(3)));
    d->wheel = static_cast<MouseWheelMovement>(pkHexToUInt(parts.at(4)));
    d->gesture = static_cast<GestureAction>(pkHexToUInt(parts.at(5)));

    return true;
}

KisAbstractInputAction *KisShortcutConfiguration::action() const
{
    return d->action;
}

void KisShortcutConfiguration::setAction(KisAbstractInputAction *newAction)
{
    if (d->action != newAction) {
        d->action = newAction;
    }
}

KisShortcutConfiguration::ShortcutType KisShortcutConfiguration::type() const
{
    return d->type;
}

void KisShortcutConfiguration::setType(KisShortcutConfiguration::ShortcutType newType)
{
    if (d->type != newType) {
        d->type = newType;
    }
}

uint KisShortcutConfiguration::mode() const
{
    return d->mode;
}

void KisShortcutConfiguration::setMode(uint newMode)
{
    if (d->mode != newMode) {
        d->mode = newMode;
    }
}

PkList< Qt::Key > KisShortcutConfiguration::keys() const
{
    return d->keys;
}

void KisShortcutConfiguration::setKeys(const PkList< Qt::Key > &newKeys)
{
    if (d->keys != newKeys) {
        d->keys = newKeys;
    }
}

Qt::MouseButtons KisShortcutConfiguration::buttons() const
{
    return d->buttons;
}

void KisShortcutConfiguration::setButtons(Qt::MouseButtons newButtons)
{
    if (d->buttons != newButtons) {
        d->buttons = newButtons;
    }
}

KisShortcutConfiguration::MouseWheelMovement KisShortcutConfiguration::wheel() const
{
    return d->wheel;
}

void KisShortcutConfiguration::setWheel(KisShortcutConfiguration::MouseWheelMovement type)
{
    if (d->wheel != type) {
        d->wheel = type;
    }
}

KisShortcutConfiguration::GestureAction KisShortcutConfiguration::gesture() const
{
    return d->gesture;
}

void KisShortcutConfiguration::setGesture(KisShortcutConfiguration::GestureAction type)
{
    if (d->gesture != type) {
        d->gesture = type;
    }
}

bool KisShortcutConfiguration::isNoOp() const
{
    return d->type == UnknownType || (d->type == KeyCombinationType && d->keys.isEmpty())
        || (d->type == MouseButtonType && d->buttons.testFlag(Qt::NoButton))
        || (d->type == MouseWheelType && d->wheel == NoMovement)
        || ((d->type == GestureType || d->type == MacOSGestureType)
            && (d->gesture == NoGesture || d->gesture == MaxGesture));
}

PkString KisShortcutConfiguration::getInputText() const
{
    switch (d->type) {
        case KeyCombinationType:
            return keysToText(d->keys);
        case MouseButtonType:
            return buttonsInputToText(d->keys, d->buttons);
        case MouseWheelType:
            return wheelInputToText(d->keys, d->wheel);
        case GestureType:
        case MacOSGestureType:
            return gestureToText(d->gesture);
        default:
            return PkString();
    }
}

PkString KisShortcutConfiguration::buttonsToText(Qt::MouseButtons buttons)
{
    PkString text;
    PkString sep(" + ");

    int buttonCount = 0;

    if (buttons & Qt::LeftButton) {
        text.append(PkString("Left"));
        buttonCount++;
    }

    if (buttons & Qt::RightButton) {
        if (buttonCount++ > 0) {
            text.append(sep);
        }

        text.append(PkString("Right"));
    }

    if (buttons & Qt::MiddleButton) {
        if (buttonCount++ > 0) {
            text.append(sep);
        }

        text.append(PkString("Middle"));
    }

    if (buttons & Qt::BackButton) {
        if (buttonCount++ > 0) {
            text.append(sep);
        }

        text.append(PkString("Back"));
    }

    if (buttons & Qt::ForwardButton) {
        if (buttonCount++ > 0) {
            text.append(sep);
        }

        text.append(PkString("Forward"));
    }

    if (buttons & Qt::TaskButton) {
        if (buttonCount++ > 0) {
            text.append(sep);
        }

        text.append(PkString("Task"));
    }

// Qt supports up to ExtraButton24 so include those
// BOOST_PP_REPEAT_FROM_TO(4, 25, EXTRA_BUTTON, _) 换手动循环：PkNamespace 的位值
// ExtraButton4 = 0x40 = 1<<6、ExtraButton24 = 0x04000000 = 1<<26，故取 1 << (n + 2)。
    for (int n = 4; n < 25; ++n) {
        if (buttons & (Qt::MouseButton)(1 << (n + 2))) {
            if (buttonCount++ > 0) {
                text.append(sep);
            }
            text.append(PkString("Mouse %1").arg(n + 3));
        }
    }

    if (buttonCount == 0) {
        text.append(PkString("None"));
    }
    else {
        text = PkString(buttonCount == 1 ? "%1 Button" : "%1 Buttons").arg(text);
    }

    return text;
}

PkString KisShortcutConfiguration::keysToText(const PkList<Qt::Key> &keys)
{
    PkString output;

    for (Qt::Key key : keys) {
#if defined(Q_OS_MAC)
        // This works for modifier keys on macOS but not other platforms.
        // They are shown with symbols, so no translation or separators needed.
        output.append(pkKeyToText(key));
#else
        if (output.size() > 0) {
            output.append(PkString(" + "));
        }

        switch (key) { //Because Qt's key→text mapping fails for Ctrl, Alt, Shift and Meta
        case Qt::Key_Control:
            output.append(PkString("Ctrl"));
            break;

        case Qt::Key_Meta:
            output.append(PkString("Meta"));
            break;

        case Qt::Key_Alt:
            output.append(PkString("Alt"));
            break;

        case Qt::Key_Shift:
            output.append(PkString("Shift"));
            break;

        default:
            output.append(pkKeyToText(key));
            break;
        }
#endif
    }

    if (output.size() == 0) {
        output = PkString("None");
    }

    return output;
}

PkString KisShortcutConfiguration::wheelToText(KisShortcutConfiguration::MouseWheelMovement wheel)
{
    switch (wheel) {
    case KisShortcutConfiguration::WheelUp:
        return PkString("Mouse Wheel Up");
        break;

    case KisShortcutConfiguration::WheelDown:
        return PkString("Mouse Wheel Down");
        break;

    case KisShortcutConfiguration::WheelLeft:
        return PkString("Mouse Wheel Left");
        break;

    case KisShortcutConfiguration::WheelRight:
        return PkString("Mouse Wheel Right");
        break;

    case KisShortcutConfiguration::WheelTrackpad:
        return PkString("Trackpad Pan");
        break;

    default:
        return PkString("None");
        break;
    }
}

PkString KisShortcutConfiguration::buttonsInputToText(const PkList<Qt::Key> &keys, Qt::MouseButtons buttons)
{
    PkString buttonsText = KisShortcutConfiguration::buttonsToText(buttons);

    if (keys.size() > 0) {
        return PkString("%1 + %2").arg(KisShortcutConfiguration::keysToText(keys), buttonsText);
    }
    else {
        return buttonsText;
    }
}

PkString KisShortcutConfiguration::wheelInputToText(const PkList<Qt::Key> &keys, KisShortcutConfiguration::MouseWheelMovement wheel)
{
    PkString wheelText = KisShortcutConfiguration::wheelToText(wheel);

    if (keys.size() > 0) {
        return PkString("%1 + %2").arg(KisShortcutConfiguration::keysToText(keys), wheelText);
    }
    else {
        return wheelText;
    }
}

PkString KisShortcutConfiguration::gestureToText(GestureAction action)
{
    switch (action) {
#ifdef Q_OS_MACOS
    case PinchGesture:
        return PkString("Pinch Gesture");
    case PanGesture:
        return PkString("Pan Gesture");
    case RotateGesture:
        return PkString("Rotate Gesture");
    case SmartZoomGesture:
        return PkString("Smart Zoom Gesture");
#else
    case OneFingerTap:
        return PkString("One Finger Tap");
    case TwoFingerTap:
        return PkString("Two Finger Tap");
    case ThreeFingerTap:
        return PkString("Three Finger Tap");
    case FourFingerTap:
        return PkString("Four Finger Tap");
    case FiveFingerTap:
        return PkString("Five Finger Tap");
    case OneFingerDrag:
        return PkString("One Finger Drag");
    case TwoFingerDrag:
        return PkString("Two Finger Drag");
    case ThreeFingerDrag:
        return PkString("Three Finger Drag");
    case FourFingerDrag:
        return PkString("Four Finger Drag");
    case FiveFingerDrag:
        return PkString("Five Finger Drag");
    case OneFingerHold:
        return PkString("One Finger Hold");
#endif
    default:
        return PkString("No Gesture");
    }
}
