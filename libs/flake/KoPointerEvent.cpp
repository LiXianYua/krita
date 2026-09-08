/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006 C. Boemann Rasmussen <cbo@boemann.dk>
   SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoPointerEvent.h"
#include <PkFlakeBridge.h>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <stdexcept>
#include <boost/variant2/variant.hpp>

#include <ksharedconfig.h>
#include <kconfiggroup.h>
#include <kis_config_notifier.h>
#include <kis_assert.h>

class KisTouchPressureSensitivityOptionContainer : public QObject
{
private:
    Q_OBJECT
public:
    KisTouchPressureSensitivityOptionContainer() {
        PkObject::connect(KisConfigNotifier::instance(), &KisConfigNotifier::configChanged,
                          KisConfigNotifier::instance(), [this]() { slotSettingsChanged(); });
        slotSettingsChanged();
    }

    bool useTouchPressure = true;

private Q_SLOTS:
    void slotSettingsChanged() {

        KConfigGroup group = KSharedConfig::openConfig()->group("");
        useTouchPressure = group.readEntry("useTouchPressureSensitivity", true);
    }
};

Q_GLOBAL_STATIC(KisTouchPressureSensitivityOptionContainer, s_optionContainer)

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
namespace detail {

// Qt's events do not have copy-ctors yet, so we should emulate them
// See https://bugreports.qt.io/browse/QTBUG-72488

template <class Event> void copyEventHack(const Event *src, PkScopedPointer<QEvent> &dst);

template<> void copyEventHack(const QMouseEvent *src, PkScopedPointer<QEvent> &dst) {
    QMouseEvent *tmp = new QMouseEvent(src->type(),
                                       src->localPos(), src->windowPos(), src->screenPos(),
                                       src->button(), src->buttons(), src->modifiers(),
                                       src->source());
    tmp->setTimestamp(src->timestamp());
    dst.reset(tmp);
}

template<> void copyEventHack(const QTabletEvent *src, PkScopedPointer<QEvent> &dst) {
    QTabletEvent *tmp = new QTabletEvent(src->type(),
                                         src->posF(), src->globalPosF(),
                                         src->deviceType(), src->pointerType(),
                                         src->pressure(),
                                         src->xTilt(), src->yTilt(),
                                         src->tangentialPressure(),
                                         src->rotation(),
                                         src->z(),
                                         src->modifiers(),
                                         src->uniqueId(),
                                         src->button(), src->buttons());
    tmp->setTimestamp(src->timestamp());
    dst.reset(tmp);
}

template<> void copyEventHack(const QTouchEvent *src, PkScopedPointer<QEvent> &dst) {
    QTouchEvent *tmp = new QTouchEvent(src->type(),
                                       src->device(),
                                       src->modifiers(),
                                       src->touchPointStates(),
                                       src->touchPoints());
    tmp->setTimestamp(src->timestamp());
    dst.reset(tmp);
}

}
#endif

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
        qreal pressure {1.0};
        qreal rotation {0.0};
        qreal tangentialPressure {0.0};
        qreal xTilt {0.0};
        qreal yTilt {0.0};
        int z {0};
        ulong timestamp {0};
        bool accepted {true};
        bool spontaneous {false};
    };

    template <typename Event>
    Private(Event *event)
        : eventPtr(event)
    {
    }

    explicit Private(NativeState state)
        : eventPtr(static_cast<QMouseEvent *>(nullptr))
        , nativeState(std::move(state))
    {
    }

    boost::variant2::variant<QMouseEvent*, QTabletEvent*, QTouchEvent*> eventPtr;
    std::optional<NativeState> nativeState;
    static bool s_tabletInputReceived;
};

bool KoPointerEvent::Private::s_tabletInputReceived;

KoPointerEvent::KoPointerEvent(QMouseEvent *ev, const PkPointF &pnt)
    : point(pnt),
      d(new Private(ev))
{
}

KoPointerEvent::KoPointerEvent(QTabletEvent *ev, const PkPointF &pnt)
    : point(pnt),
      d(new Private(ev))
{
    if (!Private::s_tabletInputReceived) {
        Private::s_tabletInputReceived = true;
        KisConfigNotifier::instance()->notifyTouchPaintingChanged();
    }
}

KoPointerEvent::KoPointerEvent(QTouchEvent* ev, const PkPointF &pnt)
    : point(pnt),
      d(new Private(ev))
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

template <typename Event>
KoPointerEventWrapper::KoPointerEventWrapper(Event *_event, const PkPointF &point)
    : event(_event, point),
      baseQtEvent(PkSharedPointer<QEvent>(static_cast<QEvent*>(_event)))
{
}


#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
struct DeepCopyVisitor
{
    PkPointF point;

    template <typename T>
    KoPointerEventWrapper operator() (const T *event) {
        PkScopedPointer<QEvent> baseEvent;
        detail::copyEventHack(event, baseEvent);
        return {static_cast<T*>(baseEvent.take()), point};
    }
};
#endif

KoPointerEventWrapper KoPointerEvent::deepCopyEvent() const
{
    if (d->nativeState) {
        throw std::logic_error("a detached KoPointerEvent has no host event to clone");
    }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    return visit(DeepCopyVisitor{point}, d->eventPtr);
#else
    struct Visitor {

        PkPointF point;

        KoPointerEventWrapper operator() (const QMouseEvent *event) {
            return KoPointerEventWrapper(event->clone(), point);
        }
        KoPointerEventWrapper operator() (const QTabletEvent *event) {
            return KoPointerEventWrapper(event->clone(), point);
        }
        KoPointerEventWrapper operator() (const QTouchEvent *event) {
            return KoPointerEventWrapper(event->clone(), point);
        }
    };
    return visit(Visitor{point}, d->eventPtr);


#endif
}

KoPointerEvent KoPointerEvent::detachedCopy() const
{
    KoPointerEvent copy(pos(), point,
                        static_cast<Pk::MouseButton>(button()),
                        Pk::MouseButtons(static_cast<int>(buttons())),
                        Pk::KeyboardModifiers(static_cast<int>(modifiers())));
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

Qt::MouseButton KoPointerEvent::button() const
{
    if (d->nativeState) {
        return static_cast<Qt::MouseButton>(d->nativeState->button);
    }
    struct Visitor {
        Qt::MouseButton operator() (const QMouseEvent *event) {
            return event->button();
        }
        Qt::MouseButton operator() (const QTabletEvent *event) {
            return event->button();
        }
        Qt::MouseButton operator() (const QTouchEvent *) {
            return Qt::LeftButton;
        }
    };

    return visit(Visitor(), d->eventPtr);
}

Qt::MouseButtons KoPointerEvent::buttons() const
{
    if (d->nativeState) {
        return Qt::MouseButtons(d->nativeState->buttons);
    }
    struct Visitor {
        Qt::MouseButtons operator() (const QMouseEvent *event) {
            return event->buttons();
        }
        Qt::MouseButtons operator() (const QTabletEvent *event) {
            return event->buttons();
        }
        Qt::MouseButtons operator() (const QTouchEvent *) {
            return Qt::LeftButton;
        }
    };

    return visit(Visitor(), d->eventPtr);
}

PkPoint KoPointerEvent::globalPos() const
{
    if (d->nativeState) {
        return d->nativeState->globalPosition;
    }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    struct Visitor {
        PkPoint operator() (const QMouseEvent *event) {
            return toPkPoint(event->globalPos());
        }
        PkPoint operator() (const QTabletEvent *event) {
            return toPkPoint(event->globalPos());
        }
        PkPoint operator() (const QTouchEvent *event) {
            return toPkPoint(event->touchPoints().constFirst().screenPos().toPoint());
        }
#else
    struct Visitor {
        PkPoint operator() (const QMouseEvent *event) {
            return event->globalPosition().toPoint();
        }
        PkPoint operator() (const QTabletEvent *event) {
            return event->globalPosition().toPoint();
        }
        PkPoint operator() (const QTouchEvent *event) {
            return event->points().constFirst().globalPosition().toPoint();
        }
#endif

    };

    return visit(Visitor(), d->eventPtr);
}

PkPoint KoPointerEvent::pos() const
{
    if (d->nativeState) {
        return d->nativeState->widgetPosition;
    }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    struct Visitor {
        PkPoint operator() (const QMouseEvent *event) {
            return toPkPoint(event->pos());
        }
        PkPoint operator() (const QTabletEvent *event) {
            return toPkPoint(event->pos());
        }
        PkPoint operator() (const QTouchEvent *event) {
            return toPkPoint(event->touchPoints().at(0).pos().toPoint());
        }
    };
#else
    struct Visitor {
        PkPoint operator() (const QMouseEvent *event) {
            return event->position().toPoint();
        }
        PkPoint operator() (const QTabletEvent *event) {
            return event->position().toPoint();
        }
        PkPoint operator() (const QTouchEvent *event) {
            return event->points().at(0).position().toPoint();
        }
    };
#endif
    return visit(Visitor(), d->eventPtr);
}

int KoPointerEvent::x() const
{
    return pos().x();
}

int KoPointerEvent::y() const
{
    return pos().y();
}

qreal KoPointerEvent::pressure() const
{
    if (d->nativeState) {
        return d->nativeState->pressure;
    }
    struct Visitor {
        qreal operator() (const QTabletEvent *event) {
            return event->pressure();
        }
        qreal operator() (const QTouchEvent *event) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            return s_optionContainer->useTouchPressure ? event->touchPoints().at(0).pressure() : 1.0;
#else
            return s_optionContainer->useTouchPressure ? event->points().at(0).pressure() : 1.0;
#endif
        }
        qreal operator() (...) {
            return 1.0;
        }
    };

    return visit(Visitor(), d->eventPtr);
}

qreal KoPointerEvent::rotation() const
{
    if (d->nativeState) {
        return d->nativeState->rotation;
    }
    struct Visitor {
        qreal operator() (const QTabletEvent *event) {
            return event->rotation();
        }
        qreal operator() (const QTouchEvent *event) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            return event->touchPoints().at(0).rotation();
#else
            return event->points().at(0).rotation();
#endif
        }
        qreal operator() (...) {
            return 0.0;
        }
    };

    return visit(Visitor(), d->eventPtr);
}

qreal KoPointerEvent::tangentialPressure() const
{
    if (d->nativeState) {
        return d->nativeState->tangentialPressure;
    }
    struct Visitor {
        qreal operator() (const QTabletEvent *event) {
            return std::fmod((event->tangentialPressure() - (-1.0)) / (1.0 - (-1.0)), 2.0);
        }
        qreal operator() (...) {
            return 0.0;
        }
    };

    return visit(Visitor(), d->eventPtr);
}

qreal KoPointerEvent::xTilt() const
{
    if (d->nativeState) {
        return d->nativeState->xTilt;
    }
    struct Visitor {
        int operator() (const QTabletEvent *event) {
            return event->xTilt();
        }
        int operator() (...) {
            return 0;
        }
    };

    return visit(Visitor(), d->eventPtr);
}


qreal KoPointerEvent::yTilt() const
{
    if (d->nativeState) {
        return d->nativeState->yTilt;
    }
    struct Visitor {
        int operator() (const QTabletEvent *event) {
            return event->yTilt();
        }
        int operator() (...) {
            return 0;
        }
    };

    return visit(Visitor(), d->eventPtr);
}

int KoPointerEvent::z() const
{
    if (d->nativeState) {
        return d->nativeState->z;
    }
    struct Visitor {
        int operator() (const QTabletEvent *event) {
            return event->z();
        }
        int operator() (...) {
            return 0;
        }
    };

    return visit(Visitor(), d->eventPtr);
}

ulong KoPointerEvent::time() const
{
    if (d->nativeState) {
        return d->nativeState->timestamp;
    }
    struct Visitor {
        ulong operator() (const QInputEvent *event) {
            return event->timestamp();
        }
    };

    return visit(Visitor(), d->eventPtr);
}

bool KoPointerEvent::isTabletEvent() const
{
    if (d->nativeState) {
        return d->nativeState->source == Private::NativeState::Source::Tablet;
    }
    return d->eventPtr.index() == 1;
}

bool KoPointerEvent::isTouchEvent() const
{
    if (d->nativeState) {
        return d->nativeState->source == Private::NativeState::Source::Touch;
    }
    return d->eventPtr.index() == 2;
}

bool KoPointerEvent::tabletInputReceived()
{
    return Private::s_tabletInputReceived;
}

Qt::KeyboardModifiers KoPointerEvent::modifiers() const
{
    if (d->nativeState) {
        return Qt::KeyboardModifiers(d->nativeState->modifiers);
    }
    struct Visitor {
        Qt::KeyboardModifiers operator() (const QInputEvent *event) {
            return event->modifiers();
        }
    };

    return visit(Visitor(), d->eventPtr);
}

void KoPointerEvent::accept()
{
    if (d->nativeState) {
        d->nativeState->accepted = true;
        return;
    }
    struct Visitor {
        void operator() (QInputEvent *event) {
            event->accept();
        }
    };

    return visit(Visitor(), d->eventPtr);
}

void KoPointerEvent::ignore()
{
    if (d->nativeState) {
        d->nativeState->accepted = false;
        return;
    }
    struct Visitor {
        void operator() (QInputEvent *event) {
            event->ignore();
        }
    };

    return visit(Visitor(), d->eventPtr);
}

bool KoPointerEvent::isAccepted() const
{
    if (d->nativeState) {
        return d->nativeState->accepted;
    }
    struct Visitor {
        bool operator() (const QInputEvent *event) {
            return event->isAccepted();
        }
    };

    return visit(Visitor(), d->eventPtr);
}

bool KoPointerEvent::spontaneous() const
{
    if (d->nativeState) {
        return d->nativeState->spontaneous;
    }
    struct Visitor {
        bool operator() (const QInputEvent *event) {
            return event->spontaneous();
        }
    };

    return visit(Visitor(), d->eventPtr);
}

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
void KoPointerEvent::copyQtPointerEvent(const QMouseEvent *event, PkScopedPointer<QEvent> &dst)
{
    detail::copyEventHack(event, dst);
}

void KoPointerEvent::copyQtPointerEvent(const QTabletEvent *event, PkScopedPointer<QEvent> &dst)
{
    detail::copyEventHack(event, dst);
}

void KoPointerEvent::copyQtPointerEvent(const QTouchEvent *event, PkScopedPointer<QEvent> &dst)
{
    detail::copyEventHack(event, dst);
}
#endif

std::optional<PkPointF> KoPointerEvent::fetchGlobalPositionFromPointerEvent(QEvent *event)
{
    if (event == nullptr) {
        return std::nullopt;
    }

    if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd) {
        const QTouchEvent *touchEvent = static_cast<const QTouchEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const PkList<QEventPoint> &touchPoints = touchEvent->points();
#else
        const QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
#endif
        if (touchPoints.isEmpty()) {
            // Getting zero touch points can happen on Android when pressing
            // on the screen with an entire palm. Punt to using the cursor
            // position after all.
            return std::nullopt;
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            return touchPoints.constFirst().globalPosition();
#else
            return toPkPointF(touchPoints.constFirst().screenPos());
#endif
        }
    } else if (event->type() == QEvent::TabletPress || event->type() == QEvent::TabletRelease) {
        const QTabletEvent *tabletEvent = static_cast<const QTabletEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return tabletEvent->globalPosition();
#else
        return toPkPoint(tabletEvent->globalPos());
#endif
    } else if (event->type() == QEvent::MouseButtonPress ||
               event->type() == QEvent::MouseButtonRelease ||
               event->type() == QEvent::MouseMove) {
        const QMouseEvent *mouseEvent = static_cast<const QMouseEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return mouseEvent->globalPosition();
#else
        return toPkPoint(mouseEvent->globalPos());
#endif
    }

    return std::nullopt;
}

#include <KoPointerEvent.moc>
// [migrate] missing include for Pk/Qt type
#include <PkList.h>
