#include "PkDataStream.h"
#include "PkColor.h"

PkDataStream &PkDataStream::operator<<(const PkColor &value)
{
    const PkColor::WireState state = value.wireState();
    *this << static_cast<std::int8_t>(state.spec);
    for (quint16 channel : state.channels) *this << static_cast<std::uint16_t>(channel);
    return *this;
}

PkDataStream &PkDataStream::operator>>(PkColor &value)
{
    std::int8_t rawSpec = 0;
    PkColor::WireState state{PkColor::Invalid, {0u, 0u, 0u, 0u, 0u}};
    *this >> rawSpec;
    for (quint16 &channel : state.channels) {
        std::uint16_t rawChannel = 0;
        *this >> rawChannel;
        channel = rawChannel;
    }
    if (status() != Ok) {
        value = PkColor();
        return *this;
    }
    if (rawSpec < static_cast<std::int8_t>(PkColor::Invalid)
        || rawSpec > static_cast<std::int8_t>(PkColor::ExtendedRgb)) {
        setStatus(ReadCorruptData);
        value = PkColor();
        return *this;
    }
    state.spec = static_cast<PkColor::Spec>(rawSpec);
    value = PkColor::fromWireState(state);
    return *this;
}
