#include "PkDataStream.h"
#include "PkVariant.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

constexpr std::uint32_t kNullLength = 0xffffffffu;
constexpr std::uint32_t kQt4FloatType = 135u;
constexpr std::uint32_t kQt5UserType = 1024u;
constexpr std::size_t kAllocationOverhead = 2u * sizeof(void *);
constexpr std::size_t kTreeNodeOverhead = 4u * sizeof(void *);
constexpr std::size_t kHashNodeOverhead = 2u * sizeof(void *);
constexpr std::size_t kHashBucketAllowance = 2u * sizeof(void *);
constexpr std::uint32_t kQt4UserType = 127u;
constexpr std::size_t kDefaultAllocationLimit = 64u * 1024u * 1024u;
// Count every recursively decoded QVariant, including the leaf. This keeps a
// hostile chain of one-element containers bounded independently of payload
// allocation size.
constexpr std::size_t kMaximumVariantDecodeDepth = 64u;

// Every non-POD value in the closed A1 set is held as a separate object by
// PkVariant's std::any. Reserve a PkVariant-sized block plus the same
// conservative two-pointer allocator overhead used for payload buffers. The
// assertions make the allowance stay conservative if an approved destination
// object changes size; recursively owned storage is charged at its own decode.
constexpr std::size_t kVariantObjectAllowance = sizeof(PkVariant) + kAllocationOverhead;
static_assert(sizeof(PkString) <= sizeof(PkVariant));
static_assert(sizeof(PkByteArray) <= sizeof(PkVariant));
static_assert(sizeof(PkStringList) <= sizeof(PkVariant));
static_assert(sizeof(PkVariantList) <= sizeof(PkVariant));
static_assert(sizeof(PkVariantMap) <= sizeof(PkVariant));
static_assert(sizeof(PkVariantHash) <= sizeof(PkVariant));
static_assert(sizeof(PkPoint) <= sizeof(PkVariant));
static_assert(sizeof(PkPointF) <= sizeof(PkVariant));
static_assert(sizeof(PkRect) <= sizeof(PkVariant));
static_assert(sizeof(PkRectF) <= sizeof(PkVariant));
static_assert(sizeof(PkSize) <= sizeof(PkVariant));
static_assert(sizeof(PkSizeF) <= sizeof(PkVariant));
static_assert(sizeof(PkLine) <= sizeof(PkVariant));
static_assert(sizeof(PkLineF) <= sizeof(PkVariant));
static_assert(sizeof(PkDate) <= sizeof(PkVariant));
static_assert(sizeof(PkTime) <= sizeof(PkVariant));
static_assert(sizeof(PkDateTime) <= sizeof(PkVariant));

bool isSupportedType(std::uint32_t typeId)
{
    switch (typeId) {
    case PkVariant::Invalid:
    case PkVariant::Bool:
    case PkVariant::Int:
    case PkVariant::UInt:
    case PkVariant::LongLong:
    case PkVariant::ULongLong:
    case PkVariant::Double:
    case PkVariant::Map:
    case PkVariant::List:
    case PkVariant::String:
    case PkVariant::StringList:
    case PkVariant::ByteArray:
    case PkVariant::Date:
    case PkVariant::Time:
    case PkVariant::DateTime:
    case PkVariant::Rect:
    case PkVariant::RectF:
    case PkVariant::Size:
    case PkVariant::SizeF:
    case PkVariant::Line:
    case PkVariant::LineF:
    case PkVariant::Point:
    case PkVariant::PointF:
    case PkVariant::Hash:
    case PkVariant::Float:
        return true;
    default:
        return false;
    }
}

bool hasVariantObjectStorage(std::uint32_t typeId)
{
    switch (typeId) {
    case PkVariant::Invalid:
    case PkVariant::Bool:
    case PkVariant::Int:
    case PkVariant::UInt:
    case PkVariant::LongLong:
    case PkVariant::ULongLong:
    case PkVariant::Double:
    case PkVariant::Float:
        return false;
    default:
        return isSupportedType(typeId);
    }
}

} // namespace

PkDataStream::DecodeScope::DecodeScope(PkDataStream &stream)
    : m_stream(stream)
{
    if (m_stream.m_decodeDepth++ == 0u) {
        m_stream.m_decodeBudgetRemaining = m_stream.m_allocationLimit;
    }
}

PkDataStream::DecodeScope::~DecodeScope()
{
    --m_stream.m_decodeDepth;
}

PkDataStream::VariantDecodeScope::VariantDecodeScope(PkDataStream &stream)
    : m_stream(stream)
{
    if (m_stream.m_variantDecodeDepth >= kMaximumVariantDecodeDepth) {
        m_stream.setStatus(ReadCorruptData);
        return;
    }
    ++m_stream.m_variantDecodeDepth;
    m_entered = true;
}

PkDataStream::VariantDecodeScope::~VariantDecodeScope()
{
    if (m_entered) --m_stream.m_variantDecodeDepth;
}

bool PkDataStream::VariantDecodeScope::entered() const
{
    return m_entered;
}

PkDataStream::PkDataStream()
    : m_device(nullptr), m_externalBytes(nullptr), m_position(0), m_mode(PkStream::NotOpen),
      m_byteOrder(BigEndian), m_version(Qt_5_15), m_precision(DoublePrecision), m_status(Ok),
      m_allocationLimit(kDefaultAllocationLimit)
{
}

PkDataStream::PkDataStream(const PkByteArray &bytes)
    : m_device(nullptr), m_externalBytes(nullptr), m_bytes(bytes), m_position(0),
      m_mode(PkStream::ReadOnly), m_byteOrder(BigEndian), m_version(Qt_5_15),
      m_precision(DoublePrecision), m_status(Ok), m_allocationLimit(kDefaultAllocationLimit)
{
}

PkDataStream::PkDataStream(PkByteArray *bytes, PkStream::OpenMode mode)
    : m_device(nullptr), m_externalBytes(bytes), m_position(0), m_mode(mode),
      m_byteOrder(BigEndian), m_version(Qt_5_15), m_precision(DoublePrecision), m_status(Ok),
      m_allocationLimit(kDefaultAllocationLimit)
{
    if (bytes && (mode & PkStream::Truncate)) {
        bytes->resize(0);
    }
    if (bytes && (mode & PkStream::Append)) {
        m_position = static_cast<std::size_t>(bytes->size());
    }
}

PkDataStream::PkDataStream(PkStream *device)
    : m_device(device), m_externalBytes(nullptr), m_position(0),
      m_mode(device ? device->openMode() : static_cast<PkStream::OpenMode>(PkStream::NotOpen)), m_byteOrder(BigEndian),
      m_version(Qt_5_15), m_precision(DoublePrecision), m_status(Ok),
      m_allocationLimit(kDefaultAllocationLimit)
{
}

PkDataStream::ByteOrder PkDataStream::byteOrder() const { return m_byteOrder; }
void PkDataStream::setByteOrder(ByteOrder order) { m_byteOrder = order; }
PkDataStream::Version PkDataStream::version() const { return m_version; }
void PkDataStream::setVersion(Version version) { m_version = version; }
PkDataStream::FloatingPointPrecision PkDataStream::floatingPointPrecision() const { return m_precision; }
void PkDataStream::setFloatingPointPrecision(FloatingPointPrecision precision) { m_precision = precision; }
PkDataStream::Status PkDataStream::status() const { return m_status; }
void PkDataStream::setStatus(Status status) { if (m_status == Ok) m_status = status; }
void PkDataStream::resetStatus() { m_status = Ok; }
std::size_t PkDataStream::allocationLimit() const { return m_allocationLimit; }
void PkDataStream::setAllocationLimit(std::size_t bytes) { m_allocationLimit = bytes; }

const PkByteArray &PkDataStream::byteArray() const
{
    return m_externalBytes ? *m_externalBytes : m_bytes;
}

PkByteArray *PkDataStream::writableByteArray()
{
    return m_externalBytes ? m_externalBytes : &m_bytes;
}

bool PkDataStream::writeRaw(const char *data, std::size_t size)
{
    if (m_status != Ok) return false;
    if (m_device) {
        if (m_device->write(data, static_cast<PkStream::pk_int64>(size)) != static_cast<PkStream::pk_int64>(size)) {
            setStatus(WriteFailed);
            return false;
        }
        return true;
    }
    if (!(m_mode & PkStream::WriteOnly) || !m_externalBytes) {
        setStatus(WriteFailed);
        return false;
    }
    PkByteArray *bytes = writableByteArray();
    if (size > static_cast<std::size_t>((std::numeric_limits<int>::max)()) - m_position) {
        setStatus(WriteFailed);
        return false;
    }
    const std::size_t end = m_position + size;
    try {
        if (static_cast<std::size_t>(bytes->size()) < end) bytes->resize(static_cast<int>(end));
    } catch (const std::bad_alloc &) {
        setStatus(WriteFailed);
        return false;
    } catch (const std::length_error &) {
        setStatus(WriteFailed);
        return false;
    }
    if (size) std::memcpy(bytes->data() + m_position, data, size);
    m_position = end;
    return true;
}

bool PkDataStream::readRaw(char *data, std::size_t size)
{
    if (m_status != Ok) {
        if (size) std::memset(data, 0, size);
        return false;
    }
    if (m_device) {
        const PkStream::pk_int64 count = m_device->read(data, static_cast<PkStream::pk_int64>(size));
        if (count != static_cast<PkStream::pk_int64>(size)) {
            if (size) std::memset(data, 0, size);
            setStatus(ReadPastEnd);
            return false;
        }
        return true;
    }
    if (!(m_mode & PkStream::ReadOnly)) {
        if (size) std::memset(data, 0, size);
        setStatus(ReadPastEnd);
        return false;
    }
    const PkByteArray &bytes = byteArray();
    if (m_position > static_cast<std::size_t>(bytes.size()) ||
        size > static_cast<std::size_t>(bytes.size()) - m_position) {
        m_position = static_cast<std::size_t>(bytes.size());
        if (size) std::memset(data, 0, size);
        setStatus(ReadPastEnd);
        return false;
    }
    if (size) std::memcpy(data, bytes.constData() + m_position, size);
    m_position += size;
    return true;
}

template<typename T>
PkDataStream &PkDataStream::writeInteger(T value)
{
    using U = std::make_unsigned_t<T>;
    U bits = static_cast<U>(value);
    unsigned char bytes[sizeof(T)];
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        const std::size_t shiftIndex = m_byteOrder == BigEndian ? sizeof(T) - 1 - i : i;
        bytes[i] = static_cast<unsigned char>(bits >> (shiftIndex * 8));
    }
    writeRaw(reinterpret_cast<const char *>(bytes), sizeof(bytes));
    return *this;
}

template<typename T>
PkDataStream &PkDataStream::readInteger(T &value)
{
    using U = std::make_unsigned_t<T>;
    unsigned char bytes[sizeof(T)]{};
    if (!readRaw(reinterpret_cast<char *>(bytes), sizeof(bytes))) {
        value = 0;
        return *this;
    }
    U bits = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        const std::size_t shiftIndex = m_byteOrder == BigEndian ? sizeof(T) - 1 - i : i;
        bits |= static_cast<U>(bytes[i]) << (shiftIndex * 8);
    }
    value = static_cast<T>(bits);
    return *this;
}

#define PK_DATASTREAM_INTEGER_OVERLOADS(Type) \
    PkDataStream &PkDataStream::operator<<(Type value) { return writeInteger(value); } \
    PkDataStream &PkDataStream::operator>>(Type &value) { return readInteger(value); }

PK_DATASTREAM_INTEGER_OVERLOADS(std::int8_t)
PK_DATASTREAM_INTEGER_OVERLOADS(std::uint8_t)
PK_DATASTREAM_INTEGER_OVERLOADS(std::int16_t)
PK_DATASTREAM_INTEGER_OVERLOADS(std::uint16_t)
PK_DATASTREAM_INTEGER_OVERLOADS(std::int32_t)
PK_DATASTREAM_INTEGER_OVERLOADS(std::uint32_t)
PK_DATASTREAM_INTEGER_OVERLOADS(std::int64_t)
PK_DATASTREAM_INTEGER_OVERLOADS(std::uint64_t)

#undef PK_DATASTREAM_INTEGER_OVERLOADS

PkDataStream &PkDataStream::operator<<(bool value)
{
    return *this << static_cast<std::int8_t>(value ? 1 : 0);
}

PkDataStream &PkDataStream::operator>>(bool &value)
{
    std::uint8_t encoded = 0;
    *this >> encoded;
    if (m_status == Ok && encoded > 1u) {
        setStatus(ReadCorruptData);
        value = false;
        return *this;
    }
    value = m_status == Ok && encoded == 1u;
    return *this;
}

PkDataStream &PkDataStream::operator<<(float value)
{
    if (m_version >= Qt_4_6 && m_precision == DoublePrecision) return *this << static_cast<double>(value);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return *this << bits;
}

PkDataStream &PkDataStream::operator>>(float &value)
{
    if (m_version >= Qt_4_6 && m_precision == DoublePrecision) {
        double wide = 0.0;
        *this >> wide;
        value = static_cast<float>(wide);
        return *this;
    }
    std::uint32_t bits = 0;
    *this >> bits;
    std::memcpy(&value, &bits, sizeof(value));
    return *this;
}

PkDataStream &PkDataStream::operator<<(double value)
{
    if (m_version >= Qt_4_6 && m_precision == SinglePrecision) return *this << static_cast<float>(value);
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return *this << bits;
}

PkDataStream &PkDataStream::operator>>(double &value)
{
    if (m_version >= Qt_4_6 && m_precision == SinglePrecision) {
        float narrow = 0.0f;
        *this >> narrow;
        value = static_cast<double>(narrow);
        return *this;
    }
    std::uint64_t bits = 0;
    *this >> bits;
    std::memcpy(&value, &bits, sizeof(value));
    return *this;
}

PkDataStream &PkDataStream::operator<<(const PkString &value)
{
    writeStringCodeUnits(value.PkToU16());
    return *this;
}

bool PkDataStream::writeStringCodeUnits(const std::u16string &units, bool isNull)
{
    if (isNull) {
        *this << kNullLength;
        return m_status == Ok;
    }
    if (units.size() > (std::numeric_limits<std::uint32_t>::max)() / 2u) {
        setStatus(WriteFailed);
        return false;
    }
    *this << static_cast<std::uint32_t>(units.size() * 2u);
    for (char16_t unit : units) *this << static_cast<std::uint16_t>(unit);
    return m_status == Ok;
}

PkDataStream &PkDataStream::operator>>(PkString &value)
{
    DecodeScope decode(*this);
    std::u16string units;
    bool isNull = false;
    if (!readStringCodeUnits(units, isNull)) {
        value = PkString();
        return *this;
    }
    value = PkVariant::PkFromStringCodeUnits(units).toString();
    return *this;
}

bool PkDataStream::validateReadAllocation(std::uint64_t byteCount)
{
    if (byteCount > static_cast<std::uint64_t>(m_allocationLimit)) {
        setStatus(ReadCorruptData);
        return false;
    }
    if (m_device) {
        if (!m_device->isSequential()) {
            const PkStream::pk_int64 available = m_device->bytesAvailable();
            if (available < 0 || byteCount > static_cast<std::uint64_t>(available)) {
                setStatus(ReadPastEnd);
                return false;
            }
        }
    } else if (m_position > static_cast<std::size_t>(byteArray().size()) ||
               byteCount > static_cast<std::uint64_t>(static_cast<std::size_t>(byteArray().size()) - m_position)) {
        setStatus(ReadPastEnd);
        return false;
    }
    return true;
}

bool PkDataStream::validateContainerCount(std::uint32_t count, std::size_t minimumWireBytes,
                                          std::size_t decodedElementBytes)
{
    const std::uint64_t decodedBytes = static_cast<std::uint64_t>(count) * decodedElementBytes;
    if (!chargeDecodedBytes(decodedBytes)) {
        setStatus(ReadCorruptData);
        return false;
    }
    const std::uint64_t minimum = static_cast<std::uint64_t>(count) * minimumWireBytes;
    return validateReadAllocation(minimum);
}

bool PkDataStream::chargeDecodedBytes(std::uint64_t byteCount)
{
    if (m_decodeDepth == 0u) return true;
    if (byteCount > static_cast<std::uint64_t>(m_decodeBudgetRemaining)) {
        setStatus(ReadCorruptData);
        return false;
    }
    m_decodeBudgetRemaining -= static_cast<std::size_t>(byteCount);
    return true;
}

bool PkDataStream::readStringCodeUnits(std::u16string &units, bool &isNull)
{
    std::uint32_t byteCount = 0;
    *this >> byteCount;
    units.clear();
    isNull = false;
    if (m_status != Ok) return false;
    if (byteCount == kNullLength) { isNull = true; return true; }
    if ((byteCount & 1u) != 0u || byteCount > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        setStatus(ReadCorruptData);
        return false;
    }
    if (!validateReadAllocation(byteCount)
        || (byteCount != 0u
            && !chargeDecodedBytes(static_cast<std::uint64_t>(byteCount) + kAllocationOverhead))) {
        return false;
    }
    try {
        units.assign(byteCount / 2u, u'\0');
    } catch (const std::bad_alloc &) {
        setStatus(ReadCorruptData);
        return false;
    } catch (const std::length_error &) {
        setStatus(ReadCorruptData);
        return false;
    }
    for (char16_t &unit : units) {
        std::uint16_t raw = 0;
        *this >> raw;
        if (m_status != Ok) { units.clear(); return false; }
        unit = static_cast<char16_t>(raw);
    }
    return true;
}

PkDataStream &PkDataStream::operator<<(const PkByteArray &value)
{
    *this << static_cast<std::uint32_t>(value.size());
    writeRaw(value.constData(), static_cast<std::size_t>(value.size()));
    return *this;
}

PkDataStream &PkDataStream::operator>>(PkByteArray &value)
{
    DecodeScope decode(*this);
    std::uint32_t size = 0;
    *this >> size;
    if (m_status != Ok || size == kNullLength) { value = PkByteArray(); return *this; }
    if (size > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        setStatus(ReadCorruptData);
        value = PkByteArray();
        return *this;
    }
    if (!validateReadAllocation(size)
        || (size != 0u
            && !chargeDecodedBytes(static_cast<std::uint64_t>(size) + kAllocationOverhead))) {
        value = PkByteArray();
        return *this;
    }
    try {
        value.resize(static_cast<int>(size));
    } catch (const std::bad_alloc &) {
        setStatus(ReadCorruptData);
        value = PkByteArray();
        return *this;
    } catch (const std::length_error &) {
        setStatus(ReadCorruptData);
        value = PkByteArray();
        return *this;
    }
    if (!readRaw(value.data(), size)) value = PkByteArray();
    return *this;
}

bool PkDataStream::writeVariantList(const PkVariantList &values)
{
    *this << static_cast<std::uint32_t>(values.size());
    for (const PkVariant &value : values) *this << value;
    return m_status == Ok;
}

bool PkDataStream::readVariantList(PkVariantList &values)
{
    std::uint32_t size = 0;
    *this >> size;
    if (m_status != Ok || size > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        if (m_status == Ok) setStatus(ReadCorruptData);
        return false;
    }
    if (!validateContainerCount(size, 5u, sizeof(PkVariant))) return false;
    values.clear();
    try {
        values.reserve(size);
        for (std::uint32_t i = 0; i < size; ++i) {
            PkVariant value;
            *this >> value;
            if (m_status != Ok) return false;
            values.push_back(std::move(value));
        }
    } catch (const std::bad_alloc &) {
        setStatus(ReadCorruptData);
        return false;
    } catch (const std::length_error &) {
        setStatus(ReadCorruptData);
        return false;
    }
    return true;
}

bool PkDataStream::writeStringList(const PkStringList &values)
{
    *this << static_cast<std::uint32_t>(values.size());
    for (int i = 0; i < values.size(); ++i) *this << values.at(i);
    return m_status == Ok;
}

bool PkDataStream::readStringList(PkStringList &values)
{
    std::uint32_t size = 0;
    *this >> size;
    if (m_status != Ok || size > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        if (m_status == Ok) setStatus(ReadCorruptData);
        return false;
    }
    if (!validateContainerCount(size, 4u, sizeof(PkString))) return false;
    values.clear();
    try {
        values.reserve(static_cast<int>(size));
        for (std::uint32_t i = 0; i < size; ++i) {
            PkString value;
            *this >> value;
            if (m_status != Ok) return false;
            values.append(value);
        }
    } catch (const std::bad_alloc &) {
        setStatus(ReadCorruptData);
        return false;
    } catch (const std::length_error &) {
        setStatus(ReadCorruptData);
        return false;
    }
    return true;
}

bool PkDataStream::writeVariantMap(const PkVariantMap &values)
{
    *this << static_cast<std::uint32_t>(values.size());
    // Qt 5.15 的 writeAssociativeContainer 从 constEnd() 反向写出；这是为
    // QMultiMap 的重复键保持“最近插入值”语义，QVariantMap 也走同一模板。
    for (auto it = values.rbegin(); it != values.rend(); ++it) *this << it->first << it->second;
    return m_status == Ok;
}

bool PkDataStream::readVariantMap(PkVariantMap &values)
{
    std::uint32_t size = 0;
    *this >> size;
    if (m_status != Ok || size > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        if (m_status == Ok) setStatus(ReadCorruptData);
        return false;
    }
    if (!validateContainerCount(size, 9u,
                                sizeof(PkVariantMap::value_type) + kTreeNodeOverhead)) {
        return false;
    }
    values.clear();
    try {
        for (std::uint32_t i = 0; i < size; ++i) {
            PkString key;
            PkVariant value;
            *this >> key >> value;
            if (m_status != Ok) return false;
            values.emplace(std::move(key), std::move(value));
        }
    } catch (const std::bad_alloc &) {
        setStatus(ReadCorruptData);
        return false;
    } catch (const std::length_error &) {
        setStatus(ReadCorruptData);
        return false;
    }
    return true;
}

bool PkDataStream::writeVariantHash(const PkVariantHash &values)
{
    *this << static_cast<std::uint32_t>(values.size());
    // QHash iteration order is intentionally unspecified (and salted), so a
    // multi-entry QVariantHash has no contractual byte order. The oracle
    // validates this output by having real Qt decode it and compare hash
    // semantics; single-entry fixtures remain byte-exact.
    for (const auto &entry : values) *this << entry.first << entry.second;
    return m_status == Ok;
}

bool PkDataStream::readVariantHash(PkVariantHash &values)
{
    std::uint32_t size = 0;
    *this >> size;
    if (m_status != Ok || size > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        if (m_status == Ok) setStatus(ReadCorruptData);
        return false;
    }
    if (!validateContainerCount(size, 9u,
                                sizeof(PkVariantHash::value_type) + kHashNodeOverhead
                                    + kHashBucketAllowance)) {
        return false;
    }
    values.clear();
    try {
        values.reserve(size);
        for (std::uint32_t i = 0; i < size; ++i) {
            PkString key;
            PkVariant value;
            *this >> key >> value;
            if (m_status != Ok) return false;
            values.emplace(std::move(key), std::move(value));
        }
    } catch (const std::bad_alloc &) {
        setStatus(ReadCorruptData);
        return false;
    } catch (const std::length_error &) {
        setStatus(ReadCorruptData);
        return false;
    }
    return true;
}

PkDataStream &PkDataStream::operator<<(const PkVariant &value)
{
    if (value.type() == PkVariant::UserType || !isSupportedType(static_cast<std::uint32_t>(value.type()))) {
        setStatus(WriteFailed);
        return *this;
    }
    std::uint32_t typeId = static_cast<std::uint32_t>(value.type());
    if (m_version <= Qt_4_6 && value.type() == PkVariant::Float) typeId = kQt4FloatType;
    *this << typeId << static_cast<std::uint8_t>(value.m_wireNullFlag ? 1u : 0u);
    if (m_status == Ok) writeVariantPayload(value);
    return *this;
}

bool PkDataStream::writeVariantPayload(const PkVariant &value)
{
    switch (value.type()) {
    case PkVariant::Invalid:
        if (m_version <= Qt_4_6) *this << kNullLength;
        break;
    case PkVariant::Bool: *this << static_cast<std::uint8_t>(value.toBool()); break;
    case PkVariant::Int: *this << static_cast<std::int32_t>(value.toInt()); break;
    case PkVariant::UInt: *this << static_cast<std::uint32_t>(value.toUInt()); break;
    case PkVariant::LongLong: *this << static_cast<std::int64_t>(value.toLongLong()); break;
    case PkVariant::ULongLong: *this << static_cast<std::uint64_t>(value.toULongLong()); break;
    case PkVariant::Double: *this << value.toDouble(); break;
    case PkVariant::Float: *this << value.toFloat(); break;
    case PkVariant::String:
        writeStringCodeUnits(value.PkStringCodeUnits(), value.isNull());
        break;
    case PkVariant::ByteArray:
        if (value.isNull()) *this << kNullLength;
        else *this << value.toByteArray();
        break;
    case PkVariant::StringList: return writeStringList(value.toStringList());
    case PkVariant::List: return writeVariantList(value.toList());
    case PkVariant::Map: return writeVariantMap(value.toMap());
    case PkVariant::Hash: return writeVariantHash(value.toHash());
    case PkVariant::Date:
        if (m_version <= Qt_4_6) {
            *this << static_cast<std::int32_t>(value.toDate().isValid()
                ? value.toDate().toJulianDay() : 0);
        } else {
            *this << static_cast<std::int64_t>(value.toDate().toJulianDay());
        }
        break;
    case PkVariant::Time:
        *this << static_cast<std::uint32_t>(value.toTime().isValid()
            ? value.toTime().msecsSinceStartOfDay() : kNullLength);
        break;
    case PkVariant::DateTime: {
        const PkDateTime dateTime = value.toDateTime();
        if (m_version <= Qt_4_6) {
            *this << static_cast<std::int32_t>(dateTime.date().isValid()
                ? dateTime.date().toJulianDay() : 0);
        } else {
            *this << static_cast<std::int64_t>(dateTime.date().toJulianDay());
        }
        *this << static_cast<std::uint32_t>(dateTime.time().isValid()
            ? dateTime.time().msecsSinceStartOfDay() : kNullLength);
        const PkVariant::DateTimeSpec spec = value.PkDateTimeSpec();
        if (m_version <= Qt_4_6) {
            const std::int8_t legacySpec = spec == PkVariant::DateTimeSpec::LocalTime
                ? static_cast<std::int8_t>(-1)
                : static_cast<std::int8_t>(static_cast<int>(spec) + 1);
            *this << legacySpec;
        } else {
            *this << static_cast<std::int8_t>(spec);
            if (spec == PkVariant::DateTimeSpec::OffsetFromUTC) {
                *this << static_cast<std::int32_t>(value.PkDateTimeOffsetSeconds());
            } else if (spec == PkVariant::DateTimeSpec::TimeZone) {
                *this << value.PkDateTimeZoneId();
            }
        }
        break;
    }
    case PkVariant::Rect: {
        const PkRect r = value.toRect();
        *this << static_cast<std::int32_t>(r.left()) << static_cast<std::int32_t>(r.top())
              << static_cast<std::int32_t>(r.right()) << static_cast<std::int32_t>(r.bottom());
        break;
    }
    case PkVariant::RectF: {
        const PkRectF r = value.toRectF();
        *this << r.x() << r.y() << r.width() << r.height();
        break;
    }
    case PkVariant::Size: {
        const PkSize s = value.toSize(); *this << static_cast<std::int32_t>(s.width()) << static_cast<std::int32_t>(s.height()); break;
    }
    case PkVariant::SizeF: {
        const PkSizeF s = value.toSizeF(); *this << s.width() << s.height(); break;
    }
    case PkVariant::Line: {
        const PkLine l = value.toLine();
        *this << static_cast<std::int32_t>(l.x1()) << static_cast<std::int32_t>(l.y1())
              << static_cast<std::int32_t>(l.x2()) << static_cast<std::int32_t>(l.y2());
        break;
    }
    case PkVariant::LineF: {
        const PkLineF l = value.toLineF(); *this << l.x1() << l.y1() << l.x2() << l.y2(); break;
    }
    case PkVariant::Point: {
        const PkPoint p = value.toPoint(); *this << static_cast<std::int32_t>(p.x()) << static_cast<std::int32_t>(p.y()); break;
    }
    case PkVariant::PointF: {
        const PkPointF p = value.toPointF(); *this << p.x() << p.y(); break;
    }
    default: setStatus(WriteFailed); break;
    }
    return m_status == Ok;
}

PkDataStream &PkDataStream::operator>>(PkVariant &value)
{
    DecodeScope decode(*this);
    VariantDecodeScope nesting(*this);
    value.clear();
    if (!nesting.entered()) return *this;
    std::uint32_t typeId = 0;
    std::uint8_t nullFlag = 0;
    *this >> typeId >> nullFlag;
    if (m_status != Ok) return *this;
    if (nullFlag > 1u) {
        setStatus(ReadCorruptData);
        return *this;
    }
    if (m_version <= Qt_4_6 && typeId == kQt4FloatType) typeId = PkVariant::Float;
    const bool isUserType = (m_version <= Qt_4_6 && typeId == kQt4UserType)
                         || (m_version > Qt_4_6 && typeId >= kQt5UserType);
    if (isUserType) {
        PkByteArray ignoredTypeName;
        *this >> ignoredTypeName;
        if (m_status == Ok) setStatus(ReadCorruptData);
        return *this;
    }
    if (!isSupportedType(typeId)) {
        setStatus(ReadCorruptData);
        return *this;
    }
    if (hasVariantObjectStorage(typeId) && !chargeDecodedBytes(kVariantObjectAllowance)) {
        return *this;
    }
    try {
        if (readVariantPayload(typeId, value)) {
            value.m_wireNullFlag = nullFlag != 0;
            value.m_isNull = value.m_isNull || value.m_wireNullFlag;
        }
    } catch (const std::bad_alloc &) {
        setStatus(ReadCorruptData);
        value.clear();
    } catch (const std::length_error &) {
        setStatus(ReadCorruptData);
        value.clear();
    }
    return *this;
}

bool PkDataStream::readVariantPayload(std::uint32_t typeId, PkVariant &value)
{
    switch (typeId) {
    case PkVariant::Invalid:
        if (m_version <= Qt_4_6) { std::uint32_t ignored = 0; *this >> ignored; }
        value.clear();
        break;
    case PkVariant::Bool: {
        bool v = false;
        *this >> v;
        if (m_status == Ok) value = PkVariant(v);
        break;
    }
    case PkVariant::Int: { std::int32_t v = 0; *this >> v; value = PkVariant(static_cast<int>(v)); break; }
    case PkVariant::UInt: { std::uint32_t v = 0; *this >> v; value = PkVariant(static_cast<unsigned int>(v)); break; }
    case PkVariant::LongLong: { std::int64_t v = 0; *this >> v; value = PkVariant(static_cast<long long>(v)); break; }
    case PkVariant::ULongLong: { std::uint64_t v = 0; *this >> v; value = PkVariant(static_cast<unsigned long long>(v)); break; }
    case PkVariant::Double: { double v = 0; *this >> v; value = PkVariant(v); break; }
    case PkVariant::Float: { float v = 0; *this >> v; value = PkVariant(v); break; }
    case PkVariant::String: {
        std::u16string units;
        bool payloadNull = false;
        if (readStringCodeUnits(units, payloadNull)) {
            value = PkVariant::PkFromStringCodeUnits(units);
            value.m_isNull = payloadNull;
        }
        break;
    }
    case PkVariant::ByteArray: { PkByteArray v; *this >> v; value = PkVariant(v); break; }
    case PkVariant::StringList: { PkStringList v; if (readStringList(v)) value = PkVariant(v); break; }
    case PkVariant::List: { PkVariantList v; if (readVariantList(v)) value = PkVariant(v); break; }
    case PkVariant::Map: { PkVariantMap v; if (readVariantMap(v)) value = PkVariant(v); break; }
    case PkVariant::Hash: { PkVariantHash v; if (readVariantHash(v)) value = PkVariant(v); break; }
    case PkVariant::Date: {
        std::int64_t jd = 0;
        if (m_version <= Qt_4_6) { std::int32_t oldJd = 0; *this >> oldJd; jd = oldJd; }
        else *this >> jd;
        value = PkVariant(m_version <= Qt_4_6 && jd == 0
            ? PkDate() : PkDate::fromJulianDay(jd));
        break;
    }
    case PkVariant::Time: { std::uint32_t msecs = 0; *this >> msecs; value = PkVariant(PkTime::fromMSecsSinceStartOfDay(static_cast<int>(msecs))); break; }
    case PkVariant::DateTime: {
        std::int64_t jd = 0;
        if (m_version <= Qt_4_6) { std::int32_t oldJd = 0; *this >> oldJd; jd = oldJd; }
        else *this >> jd;
        std::uint32_t msecs = 0;
        std::int8_t wireSpec = 0;
        *this >> msecs >> wireSpec;
        PkVariant::DateTimeSpec spec = PkVariant::DateTimeSpec::LocalTime;
        int offsetSeconds = 0;
        PkString timeZoneId;
        if (m_status == Ok && m_version <= Qt_4_6) {
            if (wireSpec == -1) spec = PkVariant::DateTimeSpec::LocalTime;
            else if (wireSpec >= 2 && wireSpec <= 4) {
                spec = static_cast<PkVariant::DateTimeSpec>(wireSpec - 1);
            } else {
                setStatus(ReadCorruptData);
            }
        } else if (m_status == Ok) {
            if (wireSpec < 0 || wireSpec > 3) {
                setStatus(ReadCorruptData);
            } else {
                spec = static_cast<PkVariant::DateTimeSpec>(wireSpec);
                if (spec == PkVariant::DateTimeSpec::OffsetFromUTC) {
                    std::int32_t wireOffset = 0;
                    *this >> wireOffset;
                    offsetSeconds = wireOffset;
                } else if (spec == PkVariant::DateTimeSpec::TimeZone) {
                    *this >> timeZoneId;
                }
            }
        }
        if (m_status == Ok) {
            const PkTime time = msecs == kNullLength
                ? PkTime() : PkTime::fromMSecsSinceStartOfDay(static_cast<int>(msecs));
            const PkDate date = m_version <= Qt_4_6 && jd == 0
                ? PkDate() : PkDate::fromJulianDay(jd);
            value = PkVariant::PkFromDateTime(
                PkDateTime(date, time), spec, offsetSeconds, timeZoneId);
        }
        break;
    }
    case PkVariant::Rect: {
        std::int32_t x1=0,y1=0,x2=0,y2=0; *this >> x1 >> y1 >> x2 >> y2;
        value = PkVariant(PkRect(PkPoint(x1, y1), PkPoint(x2, y2))); break;
    }
    case PkVariant::RectF: {
        double x=0,y=0,w=0,h=0; *this >> x >> y >> w >> h; value = PkVariant(PkRectF(x,y,w,h)); break;
    }
    case PkVariant::Size: { std::int32_t w=0,h=0; *this >> w >> h; value = PkVariant(PkSize(w,h)); break; }
    case PkVariant::SizeF: { double w=0,h=0; *this >> w >> h; value = PkVariant(PkSizeF(w,h)); break; }
    case PkVariant::Line: { std::int32_t x1=0,y1=0,x2=0,y2=0; *this >> x1 >> y1 >> x2 >> y2; value = PkVariant(PkLine(x1,y1,x2,y2)); break; }
    case PkVariant::LineF: { double x1=0,y1=0,x2=0,y2=0; *this >> x1 >> y1 >> x2 >> y2; value = PkVariant(PkLineF(x1,y1,x2,y2)); break; }
    case PkVariant::Point: { std::int32_t x=0,y=0; *this >> x >> y; value = PkVariant(PkPoint(x,y)); break; }
    case PkVariant::PointF: { double x=0,y=0; *this >> x >> y; value = PkVariant(PkPointF(x,y)); break; }
    default: setStatus(ReadCorruptData); break;
    }
    if (m_status != Ok) value.clear();
    return m_status == Ok;
}
