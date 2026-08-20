#include "PkVariant.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

// ── 辅助函数 ──────────────────────────────────────────────────────────────

bool pkIsNumericType(int type)
{
    switch (type) {
        case PkVariant::Bool:
        case PkVariant::Int:
        case PkVariant::UInt:
        case PkVariant::LongLong:
        case PkVariant::ULongLong:
        case PkVariant::Double:
        case PkVariant::Float:
            return true;
        default:
            return false;
    }
}

bool pkIsPODType(int type)
{
    return pkIsNumericType(type);
}

bool pkCanConvert(int fromType, int toType)
{
    if (fromType == toType) return true;
    if (fromType == PkVariant::Invalid) return false;

    // 数值类型间全部可相互转换
    if (pkIsNumericType(fromType) && pkIsNumericType(toType)) return true;

    // 字符串可转为数值，数值可转为字符串
    if (fromType == PkVariant::String && pkIsNumericType(toType)) return true;
    if (pkIsNumericType(fromType) && toType == PkVariant::String) return true;

    // 以下类型只可转换到自身（已在上面处理）
    return false;
}

// ── 构造 / 析构 / 赋值 ──────────────────────────────────────────────────

PkVariant::PkVariant() : m_type(Invalid), m_isNull(true), m_wireNullFlag(true), m_bool(false), m_data_ptr(&m_bool) {}

PkVariant::~PkVariant() = default;

PkVariant::PkVariant(const PkVariant& other)
    : m_type(other.m_type)
    , m_isNull(other.m_isNull)
    , m_wireNullFlag(other.m_wireNullFlag)
    , m_any(other.m_any)
    , m_stringCodeUnits(other.m_stringCodeUnits)
    , m_dateTimeSpec(other.m_dateTimeSpec)
    , m_dateTimeOffsetSeconds(other.m_dateTimeOffsetSeconds)
    , m_dateTimeZoneId(other.m_dateTimeZoneId)
    , m_data_ptr(nullptr)
{
    if (pkIsPODType(m_type)) {
        std::memcpy(&m_bool, &other.m_bool, sizeof(double)); // 足够大
        // 修正 m_data_ptr
        switch (m_type) {
            case Bool: m_data_ptr = &m_bool; break;
            case Int: m_data_ptr = &m_int; break;
            case UInt: m_data_ptr = &m_uint; break;
            case LongLong: m_data_ptr = &m_ll; break;
            case ULongLong: m_data_ptr = &m_ull; break;
            case Double: m_data_ptr = &m_double; break;
            case Float: m_data_ptr = &m_float; break;
            default: break;
        }
    } else if (m_type == String) {
        m_data_ptr = std::any_cast<PkString>(&m_any);
    } else if (m_type == ByteArray) {
        m_data_ptr = const_cast<PkByteArray*>(std::any_cast<const PkByteArray>(&m_any));
    } else if (m_type == StringList) {
        m_data_ptr = const_cast<PkStringList*>(std::any_cast<const PkStringList>(&m_any));
    } else if (m_type == List) {
        m_data_ptr = const_cast<PkVariantList*>(std::any_cast<const PkVariantList>(&m_any));
    } else if (m_type == Hash) {
        m_data_ptr = const_cast<PkVariantHash*>(std::any_cast<const PkVariantHash>(&m_any));
    } else if (m_type == Map) {
        m_data_ptr = const_cast<PkVariantMap*>(std::any_cast<const PkVariantMap>(&m_any));
    } else if (m_type == Point) {
        m_data_ptr = const_cast<PkPoint*>(std::any_cast<const PkPoint>(&m_any));
    } else if (m_type == PointF) {
        m_data_ptr = const_cast<PkPointF*>(std::any_cast<const PkPointF>(&m_any));
    } else if (m_type == Rect) {
        m_data_ptr = const_cast<PkRect*>(std::any_cast<const PkRect>(&m_any));
    } else if (m_type == RectF) {
        m_data_ptr = const_cast<PkRectF*>(std::any_cast<const PkRectF>(&m_any));
    } else if (m_type == Size) {
        m_data_ptr = const_cast<PkSize*>(std::any_cast<const PkSize>(&m_any));
    } else if (m_type == SizeF) {
        m_data_ptr = const_cast<PkSizeF*>(std::any_cast<const PkSizeF>(&m_any));
    } else if (m_type == Line) {
        m_data_ptr = const_cast<PkLine*>(std::any_cast<const PkLine>(&m_any));
    } else if (m_type == LineF) {
        m_data_ptr = const_cast<PkLineF*>(std::any_cast<const PkLineF>(&m_any));
    } else if (m_type == Date) {
        m_data_ptr = const_cast<PkDate*>(std::any_cast<const PkDate>(&m_any));
    } else if (m_type == Time) {
        m_data_ptr = const_cast<PkTime*>(std::any_cast<const PkTime>(&m_any));
    } else if (m_type == DateTime) {
        m_data_ptr = const_cast<PkDateTime*>(std::any_cast<const PkDateTime>(&m_any));
    } else if (m_type == UserType) {
        // UserType: m_data_ptr 在 fromValue 中设置，复制时来自源
        m_data_ptr = other.m_data_ptr;
    }
}

PkVariant::PkVariant(PkVariant&& other) noexcept
    : m_type(Invalid), m_isNull(true), m_wireNullFlag(true), m_bool(false), m_data_ptr(nullptr)
{
    m_type = other.m_type;
    m_isNull = other.m_isNull;
    m_wireNullFlag = other.m_wireNullFlag;
    m_stringCodeUnits = std::move(other.m_stringCodeUnits);
    m_dateTimeSpec = other.m_dateTimeSpec;
    m_dateTimeOffsetSeconds = other.m_dateTimeOffsetSeconds;
    m_dateTimeZoneId = std::move(other.m_dateTimeZoneId);
    if (pkIsPODType(m_type)) {
        std::memcpy(&m_bool, &other.m_bool, sizeof(double));
        switch (m_type) {
            case Bool: m_data_ptr = &m_bool; break;
            case Int: m_data_ptr = &m_int; break;
            case UInt: m_data_ptr = &m_uint; break;
            case LongLong: m_data_ptr = &m_ll; break;
            case ULongLong: m_data_ptr = &m_ull; break;
            case Double: m_data_ptr = &m_double; break;
            case Float: m_data_ptr = &m_float; break;
            default: break;
        }
    } else {
        m_any = std::move(other.m_any);
        m_data_ptr = other.m_data_ptr;
    }
    // 重置源为 null
    other.m_type = Invalid;
    other.m_isNull = true;
    other.m_wireNullFlag = true;
    other.m_data_ptr = nullptr;
}

PkVariant& PkVariant::operator=(const PkVariant& other)
{
    if (this == &other) return *this;
    m_type = other.m_type;
    m_isNull = other.m_isNull;
    m_wireNullFlag = other.m_wireNullFlag;
    m_any = other.m_any;
    m_stringCodeUnits = other.m_stringCodeUnits;
    m_dateTimeSpec = other.m_dateTimeSpec;
    m_dateTimeOffsetSeconds = other.m_dateTimeOffsetSeconds;
    m_dateTimeZoneId = other.m_dateTimeZoneId;
    if (pkIsPODType(m_type)) {
        std::memcpy(&m_bool, &other.m_bool, sizeof(double));
        switch (m_type) {
            case Bool: m_data_ptr = &m_bool; break;
            case Int: m_data_ptr = &m_int; break;
            case UInt: m_data_ptr = &m_uint; break;
            case LongLong: m_data_ptr = &m_ll; break;
            case ULongLong: m_data_ptr = &m_ull; break;
            case Double: m_data_ptr = &m_double; break;
            case Float: m_data_ptr = &m_float; break;
            default: break;
        }
    } else if (m_type == Invalid) {
        m_data_ptr = nullptr;
    } else {
        m_data_ptr = other.m_data_ptr;
    }
    return *this;
}

PkVariant& PkVariant::operator=(PkVariant&& other) noexcept
{
    if (this == &other) return *this;
    m_type = other.m_type;
    m_isNull = other.m_isNull;
    m_wireNullFlag = other.m_wireNullFlag;
    m_data_ptr = other.m_data_ptr;
    m_stringCodeUnits = std::move(other.m_stringCodeUnits);
    m_dateTimeSpec = other.m_dateTimeSpec;
    m_dateTimeOffsetSeconds = other.m_dateTimeOffsetSeconds;
    m_dateTimeZoneId = std::move(other.m_dateTimeZoneId);
    if (pkIsPODType(m_type)) {
        std::memcpy(&m_bool, &other.m_bool, sizeof(double));
        switch (m_type) {
            case Bool: m_data_ptr = &m_bool; break;
            case Int: m_data_ptr = &m_int; break;
            case UInt: m_data_ptr = &m_uint; break;
            case LongLong: m_data_ptr = &m_ll; break;
            case ULongLong: m_data_ptr = &m_ull; break;
            case Double: m_data_ptr = &m_double; break;
            case Float: m_data_ptr = &m_float; break;
            default: break;
        }
    } else {
        m_any = std::move(other.m_any);
    }
    other.m_type = Invalid;
    other.m_isNull = true;
    other.m_wireNullFlag = true;
    other.m_data_ptr = nullptr;
    return *this;
}

// ── 基础类型构造 ────────────────────────────────────────────────────────

PkVariant::PkVariant(bool b) : m_type(Bool), m_isNull(false), m_bool(b), m_data_ptr(&m_bool) {}
PkVariant::PkVariant(int i) : m_type(Int), m_isNull(false), m_int(i), m_data_ptr(&m_int) {}
PkVariant::PkVariant(unsigned int ui) : m_type(UInt), m_isNull(false), m_uint(ui), m_data_ptr(&m_uint) {}
PkVariant::PkVariant(long long ll) : m_type(LongLong), m_isNull(false), m_ll(ll), m_data_ptr(&m_ll) {}
PkVariant::PkVariant(unsigned long long ull) : m_type(ULongLong), m_isNull(false), m_ull(ull), m_data_ptr(&m_ull) {}
PkVariant::PkVariant(double d) : m_type(Double), m_isNull(false), m_double(d), m_data_ptr(&m_double) {}
PkVariant::PkVariant(float f) : m_type(Float), m_isNull(false), m_float(f), m_data_ptr(&m_float) {}

PkVariant::PkVariant(const char* s) : PkVariant(PkString(s)) {}
PkVariant::PkVariant(const PkString& s) : m_type(String), m_isNull(false), m_stringCodeUnits(s.PkToU16()) { m_any = s; m_data_ptr = std::any_cast<PkString>(&m_any); }
PkVariant::PkVariant(const PkByteArray& ba) : m_type(ByteArray), m_isNull(false) { m_any = ba; m_data_ptr = const_cast<PkByteArray*>(std::any_cast<const PkByteArray>(&m_any)); }
PkVariant::PkVariant(const PkStringList& sl) : m_type(StringList), m_isNull(false) { m_any = sl; m_data_ptr = const_cast<PkStringList*>(std::any_cast<const PkStringList>(&m_any)); }
PkVariant::PkVariant(const PkVariantList& vl) : m_type(List), m_isNull(false) { m_any = vl; m_data_ptr = const_cast<PkVariantList*>(std::any_cast<const PkVariantList>(&m_any)); }
PkVariant::PkVariant(const PkVariantHash& vh) : m_type(Hash), m_isNull(false) { m_any = vh; m_data_ptr = const_cast<PkVariantHash*>(std::any_cast<const PkVariantHash>(&m_any)); }
PkVariant::PkVariant(const PkVariantMap& vm) : m_type(Map), m_isNull(false) { m_any = vm; m_data_ptr = const_cast<PkVariantMap*>(std::any_cast<const PkVariantMap>(&m_any)); }
PkVariant::PkVariant(const PkPoint& pt) : m_type(Point), m_isNull(false) { m_any = pt; m_data_ptr = const_cast<PkPoint*>(std::any_cast<const PkPoint>(&m_any)); }
PkVariant::PkVariant(const PkPointF& pt) : m_type(PointF), m_isNull(false) { m_any = pt; m_data_ptr = const_cast<PkPointF*>(std::any_cast<const PkPointF>(&m_any)); }
PkVariant::PkVariant(const PkRect& r) : m_type(Rect), m_isNull(false) { m_any = r; m_data_ptr = const_cast<PkRect*>(std::any_cast<const PkRect>(&m_any)); }
PkVariant::PkVariant(const PkRectF& r) : m_type(RectF), m_isNull(false) { m_any = r; m_data_ptr = const_cast<PkRectF*>(std::any_cast<const PkRectF>(&m_any)); }
PkVariant::PkVariant(const PkSize& s) : m_type(Size), m_isNull(false) { m_any = s; m_data_ptr = const_cast<PkSize*>(std::any_cast<const PkSize>(&m_any)); }
PkVariant::PkVariant(const PkSizeF& s) : m_type(SizeF), m_isNull(false) { m_any = s; m_data_ptr = const_cast<PkSizeF*>(std::any_cast<const PkSizeF>(&m_any)); }
PkVariant::PkVariant(const PkLine& l) : m_type(Line), m_isNull(false) { m_any = l; m_data_ptr = const_cast<PkLine*>(std::any_cast<const PkLine>(&m_any)); }
PkVariant::PkVariant(const PkLineF& l) : m_type(LineF), m_isNull(false) { m_any = l; m_data_ptr = const_cast<PkLineF*>(std::any_cast<const PkLineF>(&m_any)); }
PkVariant::PkVariant(const PkDate& d) : m_type(Date), m_isNull(d.isNull()) { m_any = d; m_data_ptr = const_cast<PkDate*>(std::any_cast<const PkDate>(&m_any)); }
PkVariant::PkVariant(const PkTime& t) : m_type(Time), m_isNull(t.isNull()) { m_any = t; m_data_ptr = const_cast<PkTime*>(std::any_cast<const PkTime>(&m_any)); }
PkVariant::PkVariant(const PkDateTime& dt) : m_type(DateTime), m_isNull(dt.isNull()) { m_any = dt; m_data_ptr = const_cast<PkDateTime*>(std::any_cast<const PkDateTime>(&m_any)); }

PkVariant PkVariant::PkTypedNull(Type type)
{
    PkVariant value;
    switch (type) {
    case Invalid: return value;
    case Bool: value = PkVariant(false); break;
    case Int: value = PkVariant(0); break;
    case UInt: value = PkVariant(0u); break;
    case LongLong: value = PkVariant(0LL); break;
    case ULongLong: value = PkVariant(0ULL); break;
    case Double: value = PkVariant(0.0); break;
    case Float: value = PkVariant(0.0f); break;
    case String: value = PkVariant(PkString()); break;
    case ByteArray: value = PkVariant(PkByteArray()); break;
    case StringList: value = PkVariant(PkStringList()); break;
    case List: value = PkVariant(PkVariantList()); break;
    case Hash: value = PkVariant(PkVariantHash()); break;
    case Map: value = PkVariant(PkVariantMap()); break;
    case Point: value = PkVariant(PkPoint()); break;
    case PointF: value = PkVariant(PkPointF()); break;
    case Rect: value = PkVariant(PkRect()); break;
    case RectF: value = PkVariant(PkRectF()); break;
    case Size: value = PkVariant(PkSize()); break;
    case SizeF: value = PkVariant(PkSizeF()); break;
    case Line: value = PkVariant(PkLine()); break;
    case LineF: value = PkVariant(PkLineF()); break;
    case Date: value = PkVariant(PkDate()); break;
    case Time: value = PkVariant(PkTime()); break;
    case DateTime: value = PkVariant(PkDateTime()); break;
    case UserType: return PkVariant();
    }
    value.m_isNull = true;
    value.m_wireNullFlag = true;
    return value;
}

PkVariant PkVariant::PkFromStringCodeUnits(const std::u16string& codeUnits)
{
    const auto encodeCodePoint = [](std::uint32_t cp) {
        std::string bytes;
        if (cp <= 0x7fu) bytes.push_back(static_cast<char>(cp));
        else if (cp <= 0x7ffu) {
            bytes.push_back(static_cast<char>(0xc0u | (cp >> 6)));
            bytes.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
        } else if (cp <= 0xffffu) {
            bytes.push_back(static_cast<char>(0xe0u | (cp >> 12)));
            bytes.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
            bytes.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
        } else {
            bytes.push_back(static_cast<char>(0xf0u | (cp >> 18)));
            bytes.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3fu)));
            bytes.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
            bytes.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
        }
        return bytes;
    };

    PkString exact;
    for (std::size_t i = 0; i < codeUnits.size(); ++i) {
        const std::uint32_t unit = codeUnits[i];
        std::uint32_t cp = unit;
        enum class Slice { All, High, Low } slice = Slice::All;
        if (unit >= 0xd800u && unit <= 0xdbffu) {
            if (i + 1 < codeUnits.size() && codeUnits[i + 1] >= 0xdc00u
                && codeUnits[i + 1] <= 0xdfffu) {
                cp = 0x10000u + ((unit - 0xd800u) << 10)
                    + (static_cast<std::uint32_t>(codeUnits[++i]) - 0xdc00u);
            } else {
                // PkString has no raw UTF-16 constructor. Build a valid pair
                // with this high surrogate, then keep only its first unit.
                cp = 0x10000u + ((unit - 0xd800u) << 10);
                slice = Slice::High;
            }
        } else if (unit >= 0xdc00u && unit <= 0xdfffu) {
            // Symmetric trick for an isolated low surrogate.
            cp = 0x10000u + (unit - 0xdc00u);
            slice = Slice::Low;
        }
        const std::string utf8 = encodeCodePoint(cp);
        PkString piece = PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
        if (slice == Slice::High) piece = piece.left(1);
        else if (slice == Slice::Low) piece = piece.right(1);
        exact += piece;
    }
    PkVariant value(exact);
    value.m_stringCodeUnits = codeUnits;
    return value;
}

PkVariant PkVariant::PkFromDateTime(const PkDateTime& dateTime, DateTimeSpec spec,
                                    int offsetSeconds, const PkString& timeZoneId)
{
    PkVariant value(dateTime);
    value.setDateTimeWireState(spec, offsetSeconds, timeZoneId);
    return value;
}

void PkVariant::setDateTimeWireState(DateTimeSpec spec, int offsetSeconds,
                                     const PkString& timeZoneId)
{
    m_dateTimeSpec = spec;
    m_dateTimeOffsetSeconds = spec == DateTimeSpec::OffsetFromUTC ? offsetSeconds : 0;
    m_dateTimeZoneId = spec == DateTimeSpec::TimeZone ? timeZoneId : PkString();
}

// ── 查询 ─────────────────────────────────────────────────────────────────

bool PkVariant::isNull() const
{
    return m_isNull;
}

bool PkVariant::isValid() const
{
    return m_type != Invalid;
}

PkVariant::Type PkVariant::type() const
{
    return m_type;
}

int PkVariant::userType() const
{
    return static_cast<int>(m_type);
}

const char* PkVariant::typeName() const
{
    return typeNameForType(m_type);
}

const char* PkVariant::typeNameForTypeHelper(Type t)
{
    switch (t) {
        case Invalid:     return "Invalid";
        case Bool:        return "bool";
        case Int:         return "int";
        case UInt:        return "uint";
        case LongLong:    return "qlonglong";
        case ULongLong:   return "qulonglong";
        case Double:      return "double";
        case Map:         return "PkVariantMap";
        case List:        return "PkVariantList";
        case String:      return "PkString";
        case StringList:  return "PkStringList";
        case ByteArray:   return "PkByteArray";
        case Date:        return "PkDate";
        case Time:        return "PkTime";
        case DateTime:    return "PkDateTime";
        case Rect:        return "PkRect";
        case RectF:       return "PkRectF";
        case Size:        return "PkSize";
        case SizeF:       return "PkSizeF";
        case Line:        return "PkLine";
        case LineF:       return "PkLineF";
        case Point:       return "PkPoint";
        case PointF:      return "PkPointF";
        case Hash:        return "PkVariantHash";
        case Float:       return "float";
        case UserType:    return "UserType";
        default:          return "Unknown";
    }
}

const char* PkVariant::typeNameForType(Type t) const
{
    return typeNameForTypeHelper(t);
}

void PkVariant::clear()
{
    m_type = Invalid;
    m_isNull = true;
    m_wireNullFlag = true;
    m_any.reset();
    m_stringCodeUnits.clear();
    m_dateTimeSpec = DateTimeSpec::LocalTime;
    m_dateTimeOffsetSeconds = 0;
    m_dateTimeZoneId = PkString();
    m_data_ptr = &m_bool;
}

// ── 转换 ─────────────────────────────────────────────────────────────────

bool PkVariant::toBool() const
{
    switch (m_type) {
        case Invalid: return false;
        case Bool: return m_bool;
        case Int: return m_int != 0;
        case UInt: return m_uint != 0;
        case LongLong: return m_ll != 0;
        case ULongLong: return m_ull != 0;
        case Double: {
            if (std::isnan(m_double)) return false;
            if (std::isinf(m_double)) return true;
            return m_double != 0.0;
        }
        case Float: {
            if (std::isnan(m_float)) return false;
            if (std::isinf(m_float)) return true;
            return m_float != 0.0f;
        }
        case String: {
            const PkString& s = *std::any_cast<const PkString>(&m_any);
            if (s.isEmpty()) return false;
            // Qt treats "0", "false" as false (case-insensitive)
            PkString lower;
            // Simple case fold: convert ASCII uppercase to lowercase
            std::string utf8 = s.PkToUtf8();
            std::string folded;
            for (char c : utf8) {
                if (c >= 'A' && c <= 'Z') folded += (c - 'A' + 'a');
                else folded += c;
            }
            if (folded == "0" || folded == "false") return false;
            return true;
        }
        default: return false;
    }
}

int PkVariant::toInt() const
{
    switch (m_type) {
        case Invalid: return 0;
        case Bool: return m_bool ? 1 : 0;
        case Int: return m_int;
        case UInt: return static_cast<int>(m_uint);
        case LongLong: return static_cast<int>(m_ll);
        case ULongLong: return static_cast<int>(m_ull);
        case Double: {
            if (std::isnan(m_double) || std::isinf(m_double)) return 0;
            return qRound(m_double);
        }
        case Float: {
            if (std::isnan(m_float) || std::isinf(m_float)) return 0;
            return qRound(static_cast<double>(m_float));
        }
        case String: {
            const PkString& s = *std::any_cast<const PkString>(&m_any);
            bool ok = false;
            int v = s.toInt(&ok);
            return ok ? v : 0;
        }
        default: return 0;
    }
}

unsigned int PkVariant::toUInt() const
{
    switch (m_type) {
        case Invalid: return 0;
        case Bool: return m_bool ? 1u : 0u;
        case Int: return static_cast<unsigned int>(m_int);
        case UInt: return m_uint;
        case LongLong: return static_cast<unsigned int>(m_ll);
        case ULongLong: return static_cast<unsigned int>(m_ull);
        case Double: {
            if (std::isnan(m_double) || std::isinf(m_double)) return 0;
            return static_cast<unsigned int>(qRound(m_double));
        }
        case Float: {
            if (std::isnan(m_float) || std::isinf(m_float)) return 0;
            return static_cast<unsigned int>(qRound(static_cast<double>(m_float)));
        }
        case String: {
            const PkString& s = *std::any_cast<const PkString>(&m_any);
            bool ok = false;
            int v = s.toInt(&ok);
            return ok ? static_cast<unsigned int>(v) : 0u;
        }
        default: return 0;
    }
}

long long PkVariant::toLongLong() const
{
    switch (m_type) {
        case Invalid: return 0;
        case Bool: return m_bool ? 1LL : 0LL;
        case Int: return static_cast<long long>(m_int);
        case UInt: return static_cast<long long>(m_uint);
        case LongLong: return m_ll;
        case ULongLong: return static_cast<long long>(m_ull);
        case Double: {
            if (std::isnan(m_double) || std::isinf(m_double)) return 0;
            return static_cast<long long>(qRound(m_double));
        }
        case Float: {
            if (std::isnan(m_float) || std::isinf(m_float)) return 0;
            return static_cast<long long>(qRound(static_cast<double>(m_float)));
        }
        case String: {
            const PkString& s = *std::any_cast<const PkString>(&m_any);
            bool ok = false;
            int v = s.toInt(&ok);
            return ok ? static_cast<long long>(v) : 0LL;
        }
        default: return 0;
    }
}

unsigned long long PkVariant::toULongLong() const
{
    switch (m_type) {
        case Invalid: return 0;
        case Bool: return m_bool ? 1ULL : 0ULL;
        case Int: return static_cast<unsigned long long>(m_int);
        case UInt: return static_cast<unsigned long long>(m_uint);
        case LongLong: return static_cast<unsigned long long>(m_ll);
        case ULongLong: return m_ull;
        case Double: {
            if (std::isnan(m_double) || std::isinf(m_double)) return 0;
            return static_cast<unsigned long long>(qRound(m_double));
        }
        case Float: {
            if (std::isnan(m_float) || std::isinf(m_float)) return 0;
            return static_cast<unsigned long long>(qRound(static_cast<double>(m_float)));
        }
        case String: {
            const PkString& s = *std::any_cast<const PkString>(&m_any);
            bool ok = false;
            int v = s.toInt(&ok);
            return ok ? static_cast<unsigned long long>(v) : 0ULL;
        }
        default: return 0;
    }
}

double PkVariant::toDouble() const
{
    switch (m_type) {
        case Invalid: return 0.0;
        case Bool: return m_bool ? 1.0 : 0.0;
        case Int: return static_cast<double>(m_int);
        case UInt: return static_cast<double>(m_uint);
        case LongLong: return static_cast<double>(m_ll);
        case ULongLong: return static_cast<double>(m_ull);
        case Double: return m_double;
        case Float: return static_cast<double>(m_float);
        case String: {
            const PkString& s = *std::any_cast<const PkString>(&m_any);
            bool ok = false;
            double v = s.toDouble(&ok);
            return ok ? v : 0.0;
        }
        default: return 0.0;
    }
}

float PkVariant::toFloat() const
{
    switch (m_type) {
        case Invalid: return 0.0f;
        case Bool: return m_bool ? 1.0f : 0.0f;
        case Int: return static_cast<float>(m_int);
        case UInt: return static_cast<float>(m_uint);
        case LongLong: return static_cast<float>(m_ll);
        case ULongLong: return static_cast<float>(m_ull);
        case Double: return static_cast<float>(m_double);
        case Float: return m_float;
        case String: {
            const PkString& s = *std::any_cast<const PkString>(&m_any);
            bool ok = false;
            double v = s.toDouble(&ok);
            return ok ? static_cast<float>(v) : 0.0f;
        }
        default: return 0.0f;
    }
}

double PkVariant::toReal() const
{
    return toDouble();
}

PkString PkVariant::toString() const
{
    switch (m_type) {
        case Invalid: return PkString();
        case Bool: return PkString(m_bool ? "true" : "false");
        case Int: return PkString(std::to_string(m_int).c_str());
        case UInt: return PkString(std::to_string(m_uint).c_str());
        case LongLong: return PkString(std::to_string(m_ll).c_str());
        case ULongLong: return PkString(std::to_string(m_ull).c_str());
        case Double: {
            if (std::isnan(m_double)) return PkString("nan");
            if (std::isinf(m_double)) return PkString(m_double > 0 ? "inf" : "-inf");
            return PkString(std::to_string(m_double).c_str());
        }
        case Float: {
            if (std::isnan(m_float)) return PkString("nan");
            if (std::isinf(m_float)) return PkString(m_float > 0 ? "inf" : "-inf");
            return PkString(std::to_string(m_float).c_str());
        }
        case String: return *std::any_cast<const PkString>(&m_any);
        default: return PkString();
    }
}

PkByteArray PkVariant::toByteArray() const
{
    if (m_type == ByteArray) {
        return *std::any_cast<const PkByteArray>(&m_any);
    }
    return PkByteArray();
}

PkStringList PkVariant::toStringList() const
{
    if (m_type == StringList) {
        return *std::any_cast<const PkStringList>(&m_any);
    }
    return PkStringList();
}

PkVariantList PkVariant::toList() const
{
    if (m_type == List) {
        return *std::any_cast<const PkVariantList>(&m_any);
    }
    return PkVariantList();
}

PkVariantHash PkVariant::toHash() const
{
    if (m_type == Hash) {
        return *std::any_cast<const PkVariantHash>(&m_any);
    }
    return PkVariantHash();
}

PkVariantMap PkVariant::toMap() const
{
    if (m_type == Map) {
        return *std::any_cast<const PkVariantMap>(&m_any);
    }
    return PkVariantMap();
}

PkPoint PkVariant::toPoint() const
{
    if (m_type == Point) {
        return *std::any_cast<const PkPoint>(&m_any);
    }
    return PkPoint();
}

PkPointF PkVariant::toPointF() const
{
    if (m_type == PointF) {
        return *std::any_cast<const PkPointF>(&m_any);
    }
    return PkPointF();
}

PkRect PkVariant::toRect() const
{
    if (m_type == Rect) {
        return *std::any_cast<const PkRect>(&m_any);
    }
    return PkRect();
}

PkRectF PkVariant::toRectF() const
{
    if (m_type == RectF) {
        return *std::any_cast<const PkRectF>(&m_any);
    }
    return PkRectF();
}

PkSize PkVariant::toSize() const
{
    if (m_type == Size) {
        return *std::any_cast<const PkSize>(&m_any);
    }
    return PkSize();
}

PkSizeF PkVariant::toSizeF() const
{
    if (m_type == SizeF) {
        return *std::any_cast<const PkSizeF>(&m_any);
    }
    return PkSizeF();
}

PkLine PkVariant::toLine() const
{
    if (m_type == Line) {
        return *std::any_cast<const PkLine>(&m_any);
    }
    return PkLine();
}

PkLineF PkVariant::toLineF() const
{
    if (m_type == LineF) {
        return *std::any_cast<const PkLineF>(&m_any);
    }
    return PkLineF();
}

PkDate PkVariant::toDate() const
{
    if (m_type == Date) {
        return *std::any_cast<const PkDate>(&m_any);
    }
    return PkDate();
}

PkTime PkVariant::toTime() const
{
    if (m_type == Time) {
        return *std::any_cast<const PkTime>(&m_any);
    }
    return PkTime();
}

PkDateTime PkVariant::toDateTime() const
{
    if (m_type == DateTime) {
        return *std::any_cast<const PkDateTime>(&m_any);
    }
    return PkDateTime();
}

std::u16string PkVariant::PkStringCodeUnits() const
{
    return m_type == String ? m_stringCodeUnits : std::u16string();
}

PkVariant::DateTimeSpec PkVariant::PkDateTimeSpec() const
{
    return m_type == DateTime ? m_dateTimeSpec : DateTimeSpec::LocalTime;
}

int PkVariant::PkDateTimeOffsetSeconds() const
{
    return m_type == DateTime && m_dateTimeSpec == DateTimeSpec::OffsetFromUTC
        ? m_dateTimeOffsetSeconds : 0;
}

PkString PkVariant::PkDateTimeZoneId() const
{
    return m_type == DateTime && m_dateTimeSpec == DateTimeSpec::TimeZone
        ? m_dateTimeZoneId : PkString();
}

// ── 数据指针 ──────────────────────────────────────────────────────────────

void* PkVariant::data()
{
    return m_data_ptr;
}

const void* PkVariant::constData() const
{
    return m_data_ptr;
}

// ── 比较 ─────────────────────────────────────────────────────────────────

bool PkVariant::operator==(const PkVariant& other) const
{
    if (m_type != other.m_type) return false;
    if (m_isNull != other.m_isNull) return false;
    switch (m_type) {
        case Invalid: return true;
        case Bool: return m_bool == other.m_bool;
        case Int: return m_int == other.m_int;
        case UInt: return m_uint == other.m_uint;
        case LongLong: return m_ll == other.m_ll;
        case ULongLong: return m_ull == other.m_ull;
        case Double: return m_double == other.m_double;
        case Float: return m_float == other.m_float;
        case String: return m_stringCodeUnits == other.m_stringCodeUnits;
        case ByteArray: return *std::any_cast<const PkByteArray>(&m_any) == *std::any_cast<const PkByteArray>(&other.m_any);
        case StringList: return *std::any_cast<const PkStringList>(&m_any) == *std::any_cast<const PkStringList>(&other.m_any);
        case List: return *std::any_cast<const PkVariantList>(&m_any) == *std::any_cast<const PkVariantList>(&other.m_any);
        case Hash: return *std::any_cast<const PkVariantHash>(&m_any) == *std::any_cast<const PkVariantHash>(&other.m_any);
        case Map: return *std::any_cast<const PkVariantMap>(&m_any) == *std::any_cast<const PkVariantMap>(&other.m_any);
        case Point: return *std::any_cast<const PkPoint>(&m_any) == *std::any_cast<const PkPoint>(&other.m_any);
        case PointF: return *std::any_cast<const PkPointF>(&m_any) == *std::any_cast<const PkPointF>(&other.m_any);
        case Rect: return *std::any_cast<const PkRect>(&m_any) == *std::any_cast<const PkRect>(&other.m_any);
        case RectF: return *std::any_cast<const PkRectF>(&m_any) == *std::any_cast<const PkRectF>(&other.m_any);
        case Size: return *std::any_cast<const PkSize>(&m_any) == *std::any_cast<const PkSize>(&other.m_any);
        case SizeF: return *std::any_cast<const PkSizeF>(&m_any) == *std::any_cast<const PkSizeF>(&other.m_any);
        case Line: return *std::any_cast<const PkLine>(&m_any) == *std::any_cast<const PkLine>(&other.m_any);
        case LineF: return *std::any_cast<const PkLineF>(&m_any) == *std::any_cast<const PkLineF>(&other.m_any);
        case Date: return *std::any_cast<const PkDate>(&m_any) == *std::any_cast<const PkDate>(&other.m_any);
        case Time: return *std::any_cast<const PkTime>(&m_any) == *std::any_cast<const PkTime>(&other.m_any);
        case DateTime:
            return *std::any_cast<const PkDateTime>(&m_any) == *std::any_cast<const PkDateTime>(&other.m_any)
                && m_dateTimeSpec == other.m_dateTimeSpec
                && m_dateTimeOffsetSeconds == other.m_dateTimeOffsetSeconds
                && m_dateTimeZoneId == other.m_dateTimeZoneId;
        default: return false;
    }
}

bool PkVariant::operator!=(const PkVariant& other) const
{
    return !(*this == other);
}
