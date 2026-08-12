#include "PkFontProvider.h"

// 纯接口：全部能力方法都是纯虚，这个 .cpp 只给构造/析构一个平凡定义（同
// PkResourceStorage.cpp 的做法）。
PkFontProvider::PkFontProvider() = default;
PkFontProvider::~PkFontProvider() = default;
