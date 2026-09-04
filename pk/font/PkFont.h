#ifndef PK_FONT_H
#define PK_FONT_H

// PkFont —— QFont 的零 Qt 替代（R-50）。
// text/svg 实测用量（Qt替代品选型.md §2）：family / pointSize / setBold /
// weight / italic / setPixelSize 等。后续接 fontconfig/freetype 做真实度量，
// 当前为值语义的轻量承载。

#include <string>

// QFont::Style / QFont::Weight 的零 Qt 等价枚举（值照抄 Qt，便于 text/svg 调用点
// 机械替换；见 libs/flake/text 对 QFont::StyleItalic/StyleOblique/ Weight 的大量使用）。
enum PkFontStyle { PkFontStyleNormal, PkFontStyleItalic, PkFontStyleOblique };
enum PkFontWeight {
    PkFontWeightThin = 0, PkFontWeightExtraLight = 1, PkFontWeightLight = 2,
    PkFontWeightNormal = 3, PkFontWeightMedium = 4, PkFontWeightDemiBold = 5,
    PkFontWeightBold = 6, PkFontWeightExtraBold = 7, PkFontWeightBlack = 8
};

class PkFont {
public:
    PkFont() = default;
    explicit PkFont(const std::string &family, int pointSize = -1) : m_family(family), m_pointSize(pointSize) {}
    // 对齐 QFont(family, pointSize, weight, italic)（KoSvgTextProperties::generateFont 用到）
    PkFont(const std::string &family, int pointSize, int weight, bool italic)
        : m_family(family), m_pointSize(pointSize), m_weight(weight), m_italic(italic) {}

    std::string family() const { return m_family; }
    void setFamily(const std::string &f) { m_family = f; }

    int pointSize() const { return m_pointSize; }
    void setPointSize(int s) { m_pointSize = s; }

    int pixelSize() const { return m_pixelSize; }
    void setPixelSize(int s) { m_pixelSize = s; }

    int weight() const { return m_weight; }
    void setWeight(int w) { m_weight = w; }

    bool bold() const { return m_weight >= 75; }
    void setBold(bool b) { m_weight = b ? 75 : 50; }

    bool italic() const { return m_italic; }
    void setItalic(bool i) { m_italic = i; }

    PkFontStyle style() const { return m_italic ? PkFontStyleItalic : PkFontStyleNormal; }
    void setStyle(PkFontStyle s) { m_italic = (s != PkFontStyleNormal); }

    bool operator==(const PkFont &o) const {
        return m_family == o.m_family && m_pointSize == o.m_pointSize &&
               m_pixelSize == o.m_pixelSize && m_weight == o.m_weight && m_italic == o.m_italic;
    }

private:
    std::string m_family;
    int m_pointSize = -1;
    int m_pixelSize = -1;
    int m_weight = 50;
    bool m_italic = false;
};

#endif // PK_FONT_H
