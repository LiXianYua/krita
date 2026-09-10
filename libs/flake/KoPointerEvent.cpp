/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006 C. Boemann Rasmussen <cbo@boemann.dk>
   SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoPointerEvent.h"
#include <PkFlakeBridge.h>
#include <PkConfigGroup.h>
#include <PkSharedConfig.h>
#include <PkInputEvent.h>
#include <cmath>

#include <kis_config_notifier.h>
#include <kis_assert.h>

class KisTouchPressureSensitivityOptionContainer : public QObject
{
private:
public:
    KisTouchPressureSensitivityOptionContainer() {
        PkObject::connect(KisConfigNotifier::instance(), &KisConfigNotifier::configChanged,
                          KisConfigNotifier::instance(), [this]() { slotSettingsChanged(); });
        slotSettingsChanged();
    }

    bool useTouchPressure = true;

private:
    void slotSettingsChanged() {

        PkConfigGroup group = PkSharedConfig::openConfig()->group("");
        useTouchPressure = group.readEntry("useTouchPressureSensitivity", true);
    }
};

Q_GLOBAL_STATIC(KisTouchPressureSensitivityOptionContainer, s_optionContainer)

class Q_DECL_HIDDEN KoPointerEvent::Private
{
public:
    struct NativeState {
        enum class Source { Mouse, Tablet, Touch } source {Source::Mouse};
        int button {0};
        int buttons {0};
        int modifiers {0};
        PkPoint globalPosition;
        PkPoint widgetPosition;
        double pressure {1.0};
        double rotation {0.0};
        double tangentialPressure {0.0};
        double xTilt {0.0};
        double yTilt {0.0};
        int z {0};
        std::uint64_t timestamp {0};
        bool accepted {true};
        bool spontaneous {false};
    };

    explicit Private(NativeState state)
        : nativeState(std::move(state))
    {
    }

    std::optional<NativeState> nativeState;
    static bool s_tabletInputReceived;
};

bool KoPointerEvent::Private::s_tabletInputReceived;

KoPointerEvent::KoPointerEvent(const PkInputEvent &ev, const PkPointF &pnt)
    : point(pnt)
    , d(new Private(Private::NativeState {
          Private::NativeState::Source::Mouse,
          static_cast<int>(ev.button()),
          static_cast<int>(ev.buttons()),
          static_cast<int>(ev.modifiers()),
          ev.globalPosition().toPoint(),
          ev.localPosition().toPoint(),
          // A mouse carries no pressure/rotation axis. The state defaults
          // (pressure 1.0, the rest 0) are what the Qt mouse carrier reported.
          1.0,
          0.0,
          0.0,
          0.0,
          0.0,
          0,
          ev.timestamp(),
          true,
          // The host only invests input events that originate from a real
          // device, so such an event is spontaneous. The stroke-lifecycle
          // readers (kis_tool_rectangle_base / KisToolOutlineBase) use this to
          // tell a user-driven release from a programmatic cancellation.
          true}))
{
}

KoPointerEvent::KoPointerEvent(const PkTabletEvent &ev, const PkPointF &pnt)
    : point(pnt)
    , d(new Private(Private::NativeState {
          Private::NativeState::Source::Tablet,
          static_cast<int>(ev.button()),
          static_cast<int>(ev.buttons()),
          static_cast<int>(ev.modifiers()),
          ev.globalPosition().toPoint(),
          ev.localPosition().toPoint(),
          ev.pressure(),
          ev.rotation(),
          // Same rescaling the Qt tablet carrier used to apply: the native
          // tangential pressure is signed [-1, 1], the consumer face is [0, 1).
          std::fmod((ev.tangentialPressure() - (-1.0)) / (1.0 - (-1.0)), 2.0),
          static_cast<double>(ev.xTilt()),
          static_cast<double>(ev.yTilt()),
          ev.z(),
          ev.timestamp()}))
{
    if (!Private::s_tabletInputReceived) {
        Private::s_tabletInputReceived = true;
        KisConfigNotifier::instance()->notifyTouchPaintingChanged();
    }
}

KoPointerEvent::KoPointerEvent(const PkTouchEvent &ev, const PkPointF &pnt)
    : point(pnt)
    , d(new Private(Private::NativeState {
          Private::NativeState::Source::Touch,
          // A touch point is dispatched as a primary-button drag: the Qt touch
          // carrier hardcoded LeftButton for button()/buttons() and the tool
          // dispatch depends on it. PkTouchEvent's own base defaults to
          // NoButton, so it has to be set explicitly here.
          static_cast<int>(Pk::LeftButton),
          static_cast<int>(Pk::LeftButton),
          static_cast<int>(ev.modifiers()),
          ev.touchPoints().at(0).globalPosition().toPoint(),
          ev.touchPoints().at(0).position().toPoint(),
          s_optionContainer->useTouchPressure ? ev.touchPoints().at(0).pressure() : 1.0,
          ev.touchPoints().at(0).rotation(),
          // Touch carries no tangential pressure, tilt or z; the state
          // defaults match what the Qt touch carrier reported.
          0.0,
          0.0,
          0.0,
          0,
          ev.timestamp(),
          true,
          // Same reasoning as the mouse constructor: host-invested input is
          // spontaneous, and the stroke-lifecycle readers depend on it.
          true}))
{
}

KoPointerEvent::KoPointerEvent(const PkPoint &widgetPosition,
                               const PkPointF &documentPosition,
                               Pk::MouseButton button,
                               Pk::MouseButtons buttons,
                               Pk::KeyboardModifiers modifiers)
    : point(documentPosition)
    , d(new Private(Private::NativeState {
          Private::NativeState::Source::Mouse,
          static_cast<int>(button),
          static_cast<int>(buttons),
          static_cast<int>(modifiers),
          widgetPosition,
          widgetPosition}))
{
}

KoPointerEvent::KoPointerEvent(KoPointerEvent *event, const PkPointF &point)
    : point(point)
    , d(new Private(*(event->d)))
{
}

KoPointerEvent::KoPointerEvent(const KoPointerEvent &rhs)
    : point(rhs.point)
    , d(new Private(*(rhs.d)))
{
}

KoPointerEvent &KoPointerEvent::operator=(const KoPointerEvent &rhs)
{
    if (&rhs != this) {
        *d = *rhs.d;
        point = rhs.point;
    }

    return *this;
}

KoPointerEvent::~KoPointerEvent()
{
}

KoPointerEvent KoPointerEvent::detachedCopy() const
{
    KoPointerEvent copy(pos(), point,
                        button(), buttons(), modifiers());
    Private::NativeState &state = *copy.d->nativeState;
    state.source = isTabletEvent() ? Private::NativeState::Source::Tablet
                                   : isTouchEvent() ? Private::NativeState::Source::Touch
                                                    : Private::NativeState::Source::Mouse;
    state.globalPosition = globalPos();
    state.pressure = pressure();
    state.rotation = rotation();
    state.tangentialPressure = tangentialPressure();
    state.xTilt = xTilt();
    state.yTilt = yTilt();
    state.z = z();
    state.timestamp = time();
    state.accepted = isAccepted();
    state.spontaneous = spontaneous();
    return copy;
}

Pk::MouseButton KoPointerEvent::button() const
{
    return static_cast<Pk::MouseButton>(d->nativeState->button);
}

Pk::MouseButtons KoPointerEvent::buttons() const
{
    return Pk::MouseButtons(d->nativeState->buttons);
}

PkPoint KoPointerEvent::globalPos() const
{
    return d->nativeState->globalPosition;
}

PkPoint KoPointerEvent::pos() const
{
    return d->nativeState->widgetPosition;
}

int KoPointerEvent::x() const
{
    return pos().x();
}

int KoPointerEvent::y() const
{
    return pos().y();
}

double KoPointerEvent::pressure() const
{
    return d->nativeState->pressure;
}

double KoPointerEvent::rotation() const
{
    return d->nativeState->rotation;
}

double KoPointerEvent::tangentialPressure() const
{
    return d->nativeState->tangentialPressure;
}

double KoPointerEvent::xTilt() const
{
    return d->nativeState->xTilt;
}


double KoPointerEvent::yTilt() const
{
    return d->nativeState->yTilt;
}

int KoPointerEvent::z() const
{
    return d->nativeState->z;
}

std::uint64_t KoPointerEvent::time() const
{
    return d->nativeState->timestamp;
}

bool KoPointerEvent::isTabletEvent() const
{
    return d->nativeState->source == Private::NativeState::Source::Tablet;
}

bool KoPointerEvent::isTouchEvent() const
{
    return d->nativeState->source == Private::NativeState::Source::Touch;
}

bool KoPointerEvent::tabletInputReceived()
{
    return Private::s_tabletInputReceived;
}

Pk::KeyboardModifiers KoPointerEvent::modifiers() const
{
    return Pk::KeyboardModifiers(d->nativeState->modifiers);
}

void KoPointerEvent::accept()
{
    d->nativeState->accepted = true;
}

void KoPointerEvent::ignore()
{
    d->nativeState->accepted = false;
}

bool KoPointerEvent::isAccepted() const
{
    return d->nativeState->accepted;
}

bool KoPointerEvent::spontaneous() const
{
    return d->nativeState->spontaneous;
}
