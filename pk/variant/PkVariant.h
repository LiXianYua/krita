#pragma once

#include <any>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "PkString.h"
#include "PkStringList.h"
#include "PkPoint.h"
#include "PkSize.h"
#include "PkRect.h"
#include "PkLine.h"
#include "PkAuxTypes.h"
#include "../time/PkDateTime.h"   // pk/time：PkDate/PkTime/PkDateTime + Qt::DateFormat

// PkString 的 std::hash 特化 —— PkVariantHash (= std::unordered_map<PkString, PkVariant>) 需要
// PkString 是 COW 类，FNV-1a 逐码元哈希。
namespace std {
template<>
struct hash<PkString>
{
    std::size_t operator()(const PkString& key) const noexcept
    {
        std::size_t h = 2166136261u;
        const int n = key.size();
        for (int i = 0; i < n; ++i) {
            h ^= static_cast<std::size_t>(key.at(i));
            h *= 16777619u;
        }
        return h;
    }
};
}

// ── 前向声明 PkVariant，以便定义类型别名 ────────────────────────────────

class PkVariant;

// PkVariantList / PkVariantHash / PkVariantMap  —— 与 QVariant 兼容的类型别名
// PkVariant 尚未完整定义，但 std::vector<T> 的前向声明合法（T 只需是不完整类型）。
using PkVariantList = std::vector<PkVariant>;
using PkVariantHash = std::unordered_map<PkString, PkVariant>;
using PkVariantMap = std::map<PkString, PkVariant>;

// ── PkVariant 类定义 ─────────────────────────────────────────────────────

class PkVariant
{
public:
    // Type 枚举 —— 尽量对齐 Qt 的 QVariant::Type 数值（探针实测）
    enum Type : int {
        Invalid = 0,
        Bool = 1,
        Int = 2,
        UInt = 3,
        LongLong = 4,
        ULongLong = 5,
        Double = 6,
        // 7 = Char (未实现)
        Map = 8,
        List = 9,
        String = 10,
        StringList = 11,
        ByteArray = 12,
        // 13 = BitArray (未实现)
        Date = 14,
        Time = 15,
        DateTime = 16,
        // 17 = Url (未实现)
        // 18 = Locale (未实现)
        Rect = 19,
        RectF = 20,
        Size = 21,
        SizeF = 22,
        Line = 23,
        LineF = 24,
        Point = 25,
        PointF = 26,
        // 27 = Palette (未实现)
        Hash = 28,
        // 29-37 = 各种未实现类型
        Float = 38,
        // 39-126 = 未实现
        UserType = 127
    };

    // QDateTime's serialized time-spec state. PkDateTime intentionally keeps
    // calendar fields only; QVariant wire persistence still has to retain the
    // state that follows those fields so a decode/encode cycle is lossless.
    enum class DateTimeSpec : std::int8_t {
        LocalTime = 0,
        UTC = 1,
        OffsetFromUTC = 2,
        TimeZone = 3
    };

    // ── 构造 / 析构 / 赋值 ──────────────────────────────────────────────

    PkVariant();
    ~PkVariant();

    PkVariant(const PkVariant& other);
    PkVariant(PkVariant&& other) noexcept;
    PkVariant& operator=(const PkVariant& other);
    PkVariant& operator=(PkVariant&& other) noexcept;

    // ── 基础类型构造 ────────────────────────────────────────────────────

    PkVariant(bool b);
    PkVariant(int i);
    PkVariant(unsigned int ui);
    PkVariant(long long ll);
    PkVariant(unsigned long long ull);
    PkVariant(double d);
    PkVariant(float f);
    PkVariant(const char* s);
    PkVariant(const PkString& s);
    PkVariant(const PkByteArray& ba);
    PkVariant(const PkStringList& sl);
    PkVariant(const PkVariantList& vl);
    PkVariant(const PkVariantHash& vh);
    PkVariant(const PkVariantMap& vm);
    PkVariant(const PkPoint& pt);
    PkVariant(const PkPointF& pt);
    PkVariant(const PkRect& r);
    PkVariant(const PkRectF& r);
    PkVariant(const PkSize& s);
    PkVariant(const PkSizeF& s);
    PkVariant(const PkLine& l);
    PkVariant(const PkLineF& l);
    PkVariant(const PkDate& d);
    PkVariant(const PkTime& t);
    PkVariant(const PkDateTime& dt);

    static PkVariant PkTypedNull(Type type);
    static PkVariant PkFromStringCodeUnits(const std::u16string& codeUnits);
    static PkVariant PkFromDateTime(const PkDateTime& dt, DateTimeSpec spec,
                                    int offsetSeconds = 0,
                                    const PkString& timeZoneId = PkString());

    // ── 查询 ────────────────────────────────────────────────────────────

    bool isNull() const;
    bool isValid() const;
    Type type() const;
    int userType() const;
    const char* typeName() const;
    void clear();

    // ── 转换 ────────────────────────────────────────────────────────────

    bool toBool() const;
    int toInt() const;
    unsigned int toUInt() const;
    long long toLongLong() const;
    unsigned long long toULongLong() const;
    double toDouble() const;
    float toFloat() const;
    double toReal() const;
    PkString toString() const;
    PkByteArray toByteArray() const;
    PkStringList toStringList() const;
    PkVariantList toList() const;
    PkVariantHash toHash() const;
    PkVariantMap toMap() const;
    PkPoint toPoint() const;
    PkPointF toPointF() const;
    PkRect toRect() const;
    PkRectF toRectF() const;
    PkSize toSize() const;
    PkSizeF toSizeF() const;
    PkLine toLine() const;
    PkLineF toLineF() const;
    PkDate toDate() const;
    PkTime toTime() const;
    PkDateTime toDateTime() const;
    std::u16string PkStringCodeUnits() const;
    DateTimeSpec PkDateTimeSpec() const;
    int PkDateTimeOffsetSeconds() const;
    PkString PkDateTimeZoneId() const;

    // ── 数据指针 ────────────────────────────────────────────────────────

    void* data();
    const void* constData() const;

    // ── 模板方法（定义在 PkVariantImpl.h）───────────────────────────────

    template<typename T> static PkVariant fromValue(const T& value);
    template<typename T> T value() const;
    template<typename T> bool canConvert() const;
    template<typename T> void setValue(const T& value);

    // ── 比较 ────────────────────────────────────────────────────────────

    bool operator==(const PkVariant& other) const;
    bool operator!=(const PkVariant& other) const;

private:
    friend class PkDataStream;

    Type m_type;
    bool m_isNull = true;
    bool m_wireNullFlag = false;

    // POD 快速路径：tagged union
    union {
        bool m_bool;
        int m_int;
        unsigned int m_uint;
        long long m_ll;
        unsigned long long m_ull;
        double m_double;
        float m_float;
    };

    // 慢速路径：非 POD 类型（PkString、集合、几何、时间、自定义）走 std::any
    std::any m_any;

    DateTimeSpec m_dateTimeSpec = DateTimeSpec::LocalTime;
    int m_dateTimeOffsetSeconds = 0;
    PkString m_dateTimeZoneId;

    // data() 指针：POD 类型指向 union 成员，非 POD 类型指向 std::any 内容
    using AnyDataAccessor = void *(*)(std::any &);
    AnyDataAccessor m_anyDataAccessor = nullptr;
    void* m_data_ptr;

    void initFromPOD();
    void rebindDataPointer();
    void setDateTimeWireState(DateTimeSpec spec, int offsetSeconds, const PkString& timeZoneId);
    const char* typeNameForType(Type t) const;
    static const char* typeNameForTypeHelper(Type t);
};

// ── 类型映射 trait（归 PkVariantImpl.h 使用）────────────────────────────

template<typename T> struct PkVariantTypeId { static constexpr int value = PkVariant::UserType; };
template<> struct PkVariantTypeId<bool> { static constexpr int value = PkVariant::Bool; };
template<> struct PkVariantTypeId<int> { static constexpr int value = PkVariant::Int; };
template<> struct PkVariantTypeId<unsigned int> { static constexpr int value = PkVariant::UInt; };
template<> struct PkVariantTypeId<long long> { static constexpr int value = PkVariant::LongLong; };
template<> struct PkVariantTypeId<unsigned long long> { static constexpr int value = PkVariant::ULongLong; };
template<> struct PkVariantTypeId<double> { static constexpr int value = PkVariant::Double; };
template<> struct PkVariantTypeId<float> { static constexpr int value = PkVariant::Float; };
template<> struct PkVariantTypeId<PkString> { static constexpr int value = PkVariant::String; };
template<> struct PkVariantTypeId<PkByteArray> { static constexpr int value = PkVariant::ByteArray; };
template<> struct PkVariantTypeId<PkStringList> { static constexpr int value = PkVariant::StringList; };
template<> struct PkVariantTypeId<PkVariantList> { static constexpr int value = PkVariant::List; };
template<> struct PkVariantTypeId<PkVariantHash> { static constexpr int value = PkVariant::Hash; };
template<> struct PkVariantTypeId<PkVariantMap> { static constexpr int value = PkVariant::Map; };
template<> struct PkVariantTypeId<PkPoint> { static constexpr int value = PkVariant::Point; };
template<> struct PkVariantTypeId<PkPointF> { static constexpr int value = PkVariant::PointF; };
template<> struct PkVariantTypeId<PkRect> { static constexpr int value = PkVariant::Rect; };
template<> struct PkVariantTypeId<PkRectF> { static constexpr int value = PkVariant::RectF; };
template<> struct PkVariantTypeId<PkSize> { static constexpr int value = PkVariant::Size; };
template<> struct PkVariantTypeId<PkSizeF> { static constexpr int value = PkVariant::SizeF; };
template<> struct PkVariantTypeId<PkLine> { static constexpr int value = PkVariant::Line; };
template<> struct PkVariantTypeId<PkLineF> { static constexpr int value = PkVariant::LineF; };
template<> struct PkVariantTypeId<PkDate> { static constexpr int value = PkVariant::Date; };
template<> struct PkVariantTypeId<PkTime> { static constexpr int value = PkVariant::Time; };
template<> struct PkVariantTypeId<PkDateTime> { static constexpr int value = PkVariant::DateTime; };

// ── canConvert 辅助函数（在 PkVariant.cpp 实现）─────────────────────────

bool pkCanConvert(int fromType, int toType);

// 辅助：判断一个 Type 值是否为 POD 类型（存在 union 里）
bool pkIsPODType(int type);

// 辅助：判断一个 Type 值是否为数值类型
bool pkIsNumericType(int type);

#include "PkVariantImpl.h"
