#include "PkFont.h"

#include "PkString.h"

#include <sstream>
#include <string>
#include <vector>

bool PkFont::fromString(const PkString &description)
{
    const std::string wire = description.PkToUtf8();
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (begin <= wire.size()) {
        const std::size_t comma = wire.find(',', begin);
        fields.push_back(wire.substr(begin, comma == std::string::npos ? comma : comma - begin));
        if (comma == std::string::npos) break;
        begin = comma + 1;
    }
    if (fields.size() < 10) return false;

    try {
        m_family = fields[0];
        m_pointSize = std::stoi(fields[1]);
        m_pointSizeF = m_pointSize;
        m_pixelSize = std::stoi(fields[2]);
        m_styleHint = std::stoi(fields[3]);
        m_weight = std::stoi(fields[4]);
        m_style = static_cast<PkFontStyle>(std::stoi(fields[5]));
        m_underline = std::stoi(fields[6]);
        m_strikeOut = std::stoi(fields[7]);
        m_fixedPitch = std::stoi(fields[8]);
        m_rawMode = std::stoi(fields[9]);
    } catch (...) {
        return false;
    }
    return true;
}

PkString PkFont::toString() const
{
    std::ostringstream stream;
    stream << m_family << ',' << m_pointSize << ',' << m_pixelSize << ','
           << m_styleHint << ',' << m_weight << ',' << static_cast<int>(m_style) << ','
           << m_underline << ',' << m_strikeOut << ',' << m_fixedPitch << ',' << m_rawMode;
    return PkString(stream.str().c_str());
}
