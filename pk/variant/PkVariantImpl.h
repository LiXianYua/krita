#pragma once

#include <type_traits>

// ── fromValue<T>() ────────────────────────────────────────────────────────

template<typename T>
PkVariant PkVariant::fromValue(const T& value)
{
    constexpr int typeId = PkVariantTypeId<T>::value;
    if constexpr (typeId != PkVariant::UserType) {
        return PkVariant(value);
    } else {
        PkVariant v;
        v.m_type = UserType;
        v.m_isNull = false;
        v.m_wireNullFlag = false;
        v.m_any = value;
        if (v.m_any.has_value()) {
            v.m_data_ptr = std::any_cast<T>(&v.m_any);
        } else {
            v.m_data_ptr = nullptr;
        }
        return v;
    }
}

// ── value<T>() ────────────────────────────────────────────────────────────

// 辅助：根据当前variant的类型，只对匹配的T执行对应转换
template<typename T>
T PkVariant::value() const
{
    constexpr int typeId = PkVariantTypeId<T>::value;
    if constexpr (typeId != PkVariant::UserType) {
        // 已知类型：只有当前variant存储的类型与T匹配时才返回具体值
        switch (static_cast<int>(m_type)) {
            case Bool:        if constexpr (typeId == Bool) return m_bool; break;
            case Int:         if constexpr (typeId == Int) return m_int; break;
            case UInt:        if constexpr (typeId == UInt) return m_uint; break;
            case LongLong:    if constexpr (typeId == LongLong) return m_ll; break;
            case ULongLong:   if constexpr (typeId == ULongLong) return m_ull; break;
            case Double:      if constexpr (typeId == Double) return m_double; break;
            case Float:       if constexpr (typeId == Float) return m_float; break;
            case String:      if constexpr (typeId == String) return *std::any_cast<const PkString>(&m_any); break;
            case ByteArray:   if constexpr (typeId == ByteArray) return *std::any_cast<const PkByteArray>(&m_any); break;
            case StringList:  if constexpr (typeId == StringList) return *std::any_cast<const PkStringList>(&m_any); break;
            case List:        if constexpr (typeId == List) return *std::any_cast<const PkVariantList>(&m_any); break;
            case Hash:        if constexpr (typeId == Hash) return *std::any_cast<const PkVariantHash>(&m_any); break;
            case Map:         if constexpr (typeId == Map) return *std::any_cast<const PkVariantMap>(&m_any); break;
            case Point:       if constexpr (typeId == Point) return *std::any_cast<const PkPoint>(&m_any); break;
            case PointF:      if constexpr (typeId == PointF) return *std::any_cast<const PkPointF>(&m_any); break;
            case Rect:        if constexpr (typeId == Rect) return *std::any_cast<const PkRect>(&m_any); break;
            case RectF:       if constexpr (typeId == RectF) return *std::any_cast<const PkRectF>(&m_any); break;
            case Size:        if constexpr (typeId == Size) return *std::any_cast<const PkSize>(&m_any); break;
            case SizeF:       if constexpr (typeId == SizeF) return *std::any_cast<const PkSizeF>(&m_any); break;
            case Line:        if constexpr (typeId == Line) return *std::any_cast<const PkLine>(&m_any); break;
            case LineF:       if constexpr (typeId == LineF) return *std::any_cast<const PkLineF>(&m_any); break;
            case Date:        if constexpr (typeId == Date) return *std::any_cast<const PkDate>(&m_any); break;
            case Time:        if constexpr (typeId == Time) return *std::any_cast<const PkTime>(&m_any); break;
            case DateTime:    if constexpr (typeId == DateTime) return *std::any_cast<const PkDateTime>(&m_any); break;
            default: break;
        }
        // 如果当前类型与T不匹配，但T是数值类型，则尝试数值转换
        if constexpr (std::is_arithmetic_v<T>) {
            if (pkIsNumericType(static_cast<int>(m_type))) {
                // 数值类型间转换：走对应的toXxx()
                if constexpr (std::is_same_v<T, bool>) return toBool();
                else if constexpr (std::is_same_v<T, int>) return static_cast<T>(toInt());
                else if constexpr (std::is_same_v<T, unsigned int>) return static_cast<T>(toUInt());
                else if constexpr (std::is_same_v<T, long long>) return static_cast<T>(toLongLong());
                else if constexpr (std::is_same_v<T, unsigned long long>) return static_cast<T>(toULongLong());
                else if constexpr (std::is_same_v<T, double>) return static_cast<T>(toDouble());
                else if constexpr (std::is_same_v<T, float>) return static_cast<T>(toFloat());
            }
            if (m_type == String) {
                // 字符串→数值转换
                if constexpr (std::is_same_v<T, bool>) return toBool();
                else if constexpr (std::is_same_v<T, int>) return static_cast<T>(toInt());
                else if constexpr (std::is_same_v<T, unsigned int>) return static_cast<T>(toUInt());
                else if constexpr (std::is_same_v<T, long long>) return static_cast<T>(toLongLong());
                else if constexpr (std::is_same_v<T, unsigned long long>) return static_cast<T>(toULongLong());
                else if constexpr (std::is_same_v<T, double>) return static_cast<T>(toDouble());
                else if constexpr (std::is_same_v<T, float>) return static_cast<T>(toFloat());
            }
        }
        if constexpr (std::is_same_v<T, PkString>) {
            if (pkIsNumericType(static_cast<int>(m_type))) return toString();
        }
        return T();
    } else {
        // UserType: 从 std::any 中取出
        if (m_type == UserType && m_any.has_value() && m_any.type() == typeid(T)) {
            return std::any_cast<T>(m_any);
        }
        return T();
    }
}

// ── canConvert<T>() ───────────────────────────────────────────────────────

template<typename T>
bool PkVariant::canConvert() const
{
    constexpr int toTypeId = PkVariantTypeId<T>::value;
    if (toTypeId == PkVariant::UserType) {
        return m_type == UserType && m_any.has_value() && m_any.type() == typeid(T);
    }
    return pkCanConvert(static_cast<int>(m_type), toTypeId);
}

// ── setValue<T>() ─────────────────────────────────────────────────────────

template<typename T>
void PkVariant::setValue(const T& value)
{
    constexpr int typeId = PkVariantTypeId<T>::value;
    if constexpr (typeId != PkVariant::UserType) {
        *this = PkVariant(value);
    } else {
        m_type = UserType;
        m_isNull = false;
        m_wireNullFlag = false;
        m_any = value;
        if (m_any.has_value()) {
            m_data_ptr = std::any_cast<T>(&m_any);
        } else {
            m_data_ptr = nullptr;
        }
    }
}
