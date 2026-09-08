#pragma once

#include <PkList.h>
#include <PkNamespace.h>
#include <PkPoint.h>

#include <cstdint>
#include <memory>

class PkInputEvent
{
public:
    enum Type {
        None = 0,
        MouseButtonPress = 2,
        MouseButtonRelease = 3,
        MouseButtonDblClick = 4,
        MouseMove = 5,
        Wheel = 31,
        TabletMove = 87,
        TabletPress = 92,
        TabletRelease = 93,
        TouchBegin = 194,
        TouchUpdate = 195,
        TouchEnd = 196,
        NativeGesture = 197,
        TouchCancel = 209
    };

    explicit PkInputEvent(Type type = None,
                          const PkPointF &localPosition = PkPointF(),
                          const PkPointF &windowPosition = PkPointF(),
                          const PkPointF &globalPosition = PkPointF(),
                          Pk::MouseButton button = Pk::NoButton,
                          Pk::MouseButtons buttons = Pk::NoButton,
                          Pk::KeyboardModifiers modifiers = Pk::NoModifier,
                          std::uint64_t timestamp = 0)
        : m_type(type)
        , m_localPosition(localPosition)
        , m_windowPosition(windowPosition)
        , m_globalPosition(globalPosition)
        , m_button(button)
        , m_buttons(buttons)
        , m_modifiers(modifiers)
        , m_timestamp(timestamp)
    {
    }

    virtual ~PkInputEvent() = default;
    PkInputEvent(const PkInputEvent &) = default;
    PkInputEvent &operator=(const PkInputEvent &) = default;

    virtual std::unique_ptr<PkInputEvent> clone() const
    {
        return std::make_unique<PkInputEvent>(*this);
    }

    Type type() const noexcept { return m_type; }
    const PkPointF &localPosition() const noexcept { return m_localPosition; }
    const PkPointF &windowPosition() const noexcept { return m_windowPosition; }
    const PkPointF &globalPosition() const noexcept { return m_globalPosition; }
    Pk::MouseButton button() const noexcept { return m_button; }
    Pk::MouseButtons buttons() const noexcept { return m_buttons; }
    Pk::KeyboardModifiers modifiers() const noexcept { return m_modifiers; }
    std::uint64_t timestamp() const noexcept { return m_timestamp; }

protected:
    void setPositions(const PkPointF &localPosition,
                      const PkPointF &windowPosition,
                      const PkPointF &globalPosition)
    {
        m_localPosition = localPosition;
        m_windowPosition = windowPosition;
        m_globalPosition = globalPosition;
    }

private:
    Type m_type;
    PkPointF m_localPosition;
    PkPointF m_windowPosition;
    PkPointF m_globalPosition;
    Pk::MouseButton m_button;
    Pk::MouseButtons m_buttons;
    Pk::KeyboardModifiers m_modifiers;
    std::uint64_t m_timestamp;
};

class PkTouchPoint
{
public:
    explicit PkTouchPoint(int id = -1)
        : m_id(id)
    {
    }

    int id() const noexcept { return m_id; }
    Pk::TouchPointState state() const noexcept { return m_state; }
    const PkPointF &position() const noexcept { return m_position; }
    const PkPointF &startPosition() const noexcept { return m_startPosition; }
    const PkPointF &globalPosition() const noexcept { return m_globalPosition; }
    double pressure() const noexcept { return m_pressure; }

    void setState(Pk::TouchPointState state) noexcept { m_state = state; }
    void setPosition(const PkPointF &position) noexcept { m_position = position; }
    void setStartPosition(const PkPointF &position) noexcept { m_startPosition = position; }
    void setGlobalPosition(const PkPointF &position) noexcept { m_globalPosition = position; }
    void setPressure(double pressure) noexcept { m_pressure = pressure; }

private:
    int m_id = -1;
    Pk::TouchPointState m_state = Pk::TouchPointStationary;
    PkPointF m_position;
    PkPointF m_startPosition;
    PkPointF m_globalPosition;
    double m_pressure = 1.0;
};

class PkTabletEvent final : public PkInputEvent
{
public:
    PkTabletEvent(Type type,
                  const PkPointF &localPosition,
                  const PkPointF &globalPosition,
                  Pk::MouseButton button,
                  Pk::MouseButtons buttons,
                  Pk::KeyboardModifiers modifiers,
                  double pressure,
                  int xTilt,
                  int yTilt,
                  double tangentialPressure,
                  double rotation,
                  int z,
                  std::int64_t uniqueId,
                  std::uint64_t timestamp = 0)
        : PkInputEvent(type,
                       localPosition,
                       localPosition,
                       globalPosition,
                       button,
                       buttons,
                       modifiers,
                       timestamp)
        , m_pressure(pressure)
        , m_xTilt(xTilt)
        , m_yTilt(yTilt)
        , m_tangentialPressure(tangentialPressure)
        , m_rotation(rotation)
        , m_z(z)
        , m_uniqueId(uniqueId)
    {
    }

    std::unique_ptr<PkInputEvent> clone() const override
    {
        return std::make_unique<PkTabletEvent>(*this);
    }

    double pressure() const noexcept { return m_pressure; }
    int xTilt() const noexcept { return m_xTilt; }
    int yTilt() const noexcept { return m_yTilt; }
    double tangentialPressure() const noexcept { return m_tangentialPressure; }
    double rotation() const noexcept { return m_rotation; }
    int z() const noexcept { return m_z; }
    std::int64_t uniqueId() const noexcept { return m_uniqueId; }

private:
    double m_pressure;
    int m_xTilt;
    int m_yTilt;
    double m_tangentialPressure;
    double m_rotation;
    int m_z;
    std::int64_t m_uniqueId;
};

class PkTouchEvent final : public PkInputEvent
{
public:
    PkTouchEvent(Type type,
                 Pk::KeyboardModifiers modifiers,
                 Pk::TouchPointStates states,
                 const PkList<PkTouchPoint> &points,
                 std::uint64_t timestamp = 0)
        : PkInputEvent(type,
                       points.isEmpty() ? PkPointF() : points.first().position(),
                       points.isEmpty() ? PkPointF() : points.first().position(),
                       points.isEmpty() ? PkPointF() : points.first().globalPosition(),
                       Pk::NoButton,
                       Pk::NoButton,
                       modifiers,
                       timestamp)
        , m_states(states)
        , m_points(points)
    {
    }

    std::unique_ptr<PkInputEvent> clone() const override
    {
        return std::make_unique<PkTouchEvent>(*this);
    }

    const PkList<PkTouchPoint> &touchPoints() const noexcept { return m_points; }
    Pk::TouchPointStates touchPointStates() const noexcept { return m_states; }

    void setTouchPoints(const PkList<PkTouchPoint> &points)
    {
        m_points = points;
        setPositions(points.isEmpty() ? PkPointF() : points.first().position(),
                     points.isEmpty() ? PkPointF() : points.first().position(),
                     points.isEmpty() ? PkPointF() : points.first().globalPosition());
    }

private:
    Pk::TouchPointStates m_states;
    PkList<PkTouchPoint> m_points;
};

class PkNativeGestureEvent final : public PkInputEvent
{
public:
    PkNativeGestureEvent(Pk::NativeGestureType gestureType,
                         const PkPointF &localPosition,
                         const PkPointF &globalPosition = PkPointF(),
                         Pk::KeyboardModifiers modifiers = Pk::NoModifier,
                         std::uint64_t timestamp = 0)
        : PkInputEvent(NativeGesture,
                       localPosition,
                       localPosition,
                       globalPosition,
                       Pk::NoButton,
                       Pk::NoButton,
                       modifiers,
                       timestamp)
        , m_gestureType(gestureType)
    {
    }

    std::unique_ptr<PkInputEvent> clone() const override
    {
        return std::make_unique<PkNativeGestureEvent>(*this);
    }

    Pk::NativeGestureType gestureType() const noexcept { return m_gestureType; }

private:
    Pk::NativeGestureType m_gestureType;
};
