// ===========================================================================
// R-53 Task 1 —— SHARED 探针库 pkcrossimage 的**库镜像**一侧。
// ===========================================================================
//
// 本 TU 只编进 pkcrossimage（`add_library(pkcrossimage SHARED ...)`），**不**编进
// 测试 exe。它做的事只有一件：把 cross_image_payload.h 里那些 inline 探针
// 包成显式 default 导出的 extern "C" 桥，让 exe 能跨到库镜像来。
//
// 为什么这些桥体内跑的就是「库镜像的 pk 目标码」：
//   pkcrossimage 静态链了 libpkvariant.a（及其依赖），所以 PkVariant 的拷贝 ctor、
//   rebindDataPointer、value<T> 的模板实例化在**本 dylib 里各有一份**，
//   用的是本 dylib 自己的 &typeid(PkPoint) 等 type_info 副本。
//   隐藏可见性（见 CMakeLists.txt 顶部的目录变量，出处 plan §1.4）保证 dylib
//   不导出任何 pk 符号、exe 不会误绑到 dylib 那份——能跨过来的只有下面这
//   17×3 个显式导出的桥。这就是判据② 要的那条**真·两镜像边界**。
//
// 历史：Task 1 时载体在**未挂属性**的树上先跑出红（那正是判别力的证明）。R-53
// 裁决 A 之后 `PkVariant` 已挂 `PK_TYPE_VISIBILITY`（`pk/variant/PkVariant.h`），
// 情形已变——现在这些桥全绿。

#include "cross_image_payload.h"

#define XIMG_DEFINE_BRIDGE(T, V)                                              \
    extern "C" XIMG_EXPORT PkVariant ximg_lib_make_##T(void)                  \
    {                                                                         \
        return ximg_make_##T();                                               \
    }                                                                         \
    extern "C" XIMG_EXPORT bool ximg_lib_read_##T(const PkVariant &v)         \
    {                                                                         \
        return ximg_read_##T(v);                                              \
    }                                                                         \
    extern "C" XIMG_EXPORT std::uintptr_t ximg_lib_rawname_##T(void)          \
    {                                                                         \
        return ximg_rawname_##T();                                            \
    }
XIMG_PAYLOAD_TYPES(XIMG_DEFINE_BRIDGE)
#undef XIMG_DEFINE_BRIDGE
