#ifndef PK_FONTDATABASE_H
#define PK_FONTDATABASE_H

// PkFontDatabase —— QFontDatabase 的零 Qt 替代（R-50 锁内文件）。
// 后续接 fontconfig 枚举/注册字体；当前为占位实现。

#include <string>
#include <vector>
#include "PkFont.h"

class PkFontDatabase {
public:
    static PkFontDatabase *instance() {
        static PkFontDatabase db;
        return &db;
    }

    std::vector<std::string> families() const { return m_families; }

    // 注册一个应用内字体文件（.ttf/.otf）；返回字体 ID（>=0 成功）。
    int addApplicationFont(const std::string &path) {
        m_families.push_back(path);
        return int(m_families.size()) - 1;
    }

    bool isFixedPitch(const std::string &) const { return false; }
    std::vector<int> pointSizes(const std::string &) const { return {9, 10, 11, 12, 14}; }

private:
    std::vector<std::string> m_families;
};

#endif // PK_FONTDATABASE_H
