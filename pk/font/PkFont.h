#ifndef PK_FONT_H
#define PK_FONT_H

// PkFont —— native value-semantic font description (R-50).
// text/svg 实测用量（Qt替代品选型.md §2）：family / pointSize / setBold /
// weight / italic / setPixelSize 等。PkFont itself remains a lightweight
// value type; PkFontRasterizer resolves and renders it with Fontconfig/FreeType.

#include <cmath>
#include <string>

class PkString;

// Style and weight values keep the legacy wire/API conventions used by text/svg.
enum PkFontStyle { PkFontStyleNormal, PkFontStyleItalic, PkFontStyleOblique };
enum PkFontWeight {
    PkFontWeightThin = 0, PkFontWeightExtraLight = 1, PkFontWeightLight = 2,
    PkFontWeightNormal = 3, PkFontWeightMedium = 4, PkFontWeightDemiBold = 5,
    PkFontWeightBold = 6, PkFontWeightExtraBold = 7, PkFontWeightBlack = 8
};

class PkFont {
public:
    PkFont() = default;
    explicit PkFont(const std::string &family, int pointSize = -1)
        : m_family(family), m_pointSize(pointSize), m_pointSizeF(pointSize) {}
    // Four-argument compatibility constructor used by text property generation.
    PkFont(const std::string &family, int pointSize, int weight, bool italic)
        : m_family(family), m_pointSize(pointSize), m_pointSizeF(pointSize),
          m_weight(weight), m_style(italic ? PkFontStyleItalic : PkFontStyleNormal) {}

    std::string family() const { return m_family; }
    void setFamily(const std::string &f) { m_family = f; }

    int pointSize() const { return m_pointSize; }
    void setPointSize(int s) { m_pointSize = s; m_pointSizeF = s; m_pixelSize = -1; }

    double pointSizeF() const { return m_pointSizeF; }
    void setPointSizeF(double s) {
        m_pointSizeF = s;
        m_pointSize = static_cast<int>(std::lround(s));
        m_pixelSize = -1;
    }

    int pixelSize() const { return m_pixelSize; }
    void setPixelSize(int s) { m_pixelSize = s; m_pointSize = -1; m_pointSizeF = -1.0; }

    int weight() const { return m_weight; }
    void setWeight(int w) { m_weight = w; }

    bool bold() const { return m_weight >= 75; }
    void setBold(bool b) { m_weight = b ? 75 : 50; }

    bool italic() const { return m_style != PkFontStyleNormal; }
    void setItalic(bool i) { m_style = i ? PkFontStyleItalic : PkFontStyleNormal; }

    PkFontStyle style() const { return m_style; }
    void setStyle(PkFontStyle s) { m_style = s; }

    int stretch() const { return m_stretch; }
    void setStretch(int stretch) { m_stretch = stretch; }

    bool strikeOut() const { return m_strikeOut; }
    void setStrikeOut(bool enabled) { m_strikeOut = enabled; }

    bool underline() const { return m_underline; }
    void setUnderline(bool enabled) { m_underline = enabled; }

    bool overline() const { return m_overline; }
    void setOverline(bool enabled) { m_overline = enabled; }

    int dpi() const { return m_dpi; }
    void setDpi(int dpi) { m_dpi = dpi; }

    bool fromString(const PkString &description);
    PkString toString() const;

    bool operator==(const PkFont &o) const {
        return m_family == o.m_family && m_pointSize == o.m_pointSize &&
               m_pointSizeF == o.m_pointSizeF && m_pixelSize == o.m_pixelSize &&
               m_weight == o.m_weight && m_style == o.m_style &&
               m_stretch == o.m_stretch && m_strikeOut == o.m_strikeOut &&
               m_underline == o.m_underline && m_overline == o.m_overline &&
               m_dpi == o.m_dpi && m_styleHint == o.m_styleHint &&
               m_fixedPitch == o.m_fixedPitch &&
               m_rawMode == o.m_rawMode;
    }

private:
    std::string m_family;
    int m_pointSize = -1;
    double m_pointSizeF = -1.0;
    int m_pixelSize = -1;
    int m_weight = 50;
    PkFontStyle m_style = PkFontStyleNormal;
    int m_stretch = 100;
    bool m_overline = false;
    int m_dpi = 96;
    int m_styleHint = 5;
    int m_underline = 0;
    int m_strikeOut = 0;
    int m_fixedPitch = 0;
    int m_rawMode = 0;
};

#endif // PK_FONT_H
