#pragma once

// ===========================================================================
// R-53 Task 1 —— 跨镜像 any_cast 回归载体：两侧共用的类型清单与每镜像一份的探针
// ===========================================================================
//
// 机制（plan §1，本机实测原文）：
//   PkVariant 把非 POD 负载塞进 std::any（PkVariant.h:223），读取侧用
//   std::any_cast<T>(&m_any) 做**类型身份**判等（PkVariant.cpp:rebindDataPointer、
//   PkVariantImpl.h:value<T>）。Apple arm64 的 libc++ 是 impl 3
//   （__non_unique_arm_rtti_bit_impl）：type_info::__eq 只在**两侧都不是 unique**
//   时才回退到 strcmp。而 -fvisibility=hidden 让每个类型都被判成 unique
//   ⇒ 跨镜像只比地址 ⇒ 在 A 镜像造的 std::any，到 B 镜像 any_cast 不出来。
//
// 失败形态与守卫（plan §5 Task 1）：
//   any_cast 失败时 rebindDataPointer() 把 m_data_ptr 置成 nullptr，即
//   copy.constData() == nullptr。若直接 value<T>()，会在
//   `*std::any_cast<const T>(&m_any)` 上解引用空指针——**段错，会带走整个测试
//   进程**（S-17 的原症状就是 signal 11）。所以 ximg_read_<T>() **先拷贝、
//   再判 constData()**，失败变成干净的 false。判据一点没放松：拷贝这一动作
//   本身就跑在判等路径上（拷贝 ctor 走 rebindDataPointer ⇒ 同一条 any_cast），
//   而且这正是真实调用路径（KoProperties::property(name) 按值返回 PkVariant，
//   lib 侧拷贝天天在发生）。
//
// 「每镜像一份」是怎么做到的：
//   下面 ximg_make_<T> / ximg_read_<T> / ximg_rawname_<T> 都是 **inline**。
//   隐藏可见性下（见 CMakeLists.txt 顶部的目录变量）inline 函数是 weak private
//   ⇒ 只属于本镜像，不会被另一镜像的副本介入。它们各自的实体落在哪个镜像，
//   取决于**调用点在哪个 TU**：库镜像的调用点是 cross_image_payload.cpp 里的
//   ximg_lib_* 桥，exe 镜像的调用点是 test_cross_image_payload.cpp 里的测试槽。
//   真正承担边界的是它们调用的 PkVariant 目标码（拷贝 ctor / rebindDataPointer
//   在 libpkvariant.a 里，被**两个镜像各静态链一份**；value<T>() 是头里的模板
//   实例化，同样各镜像一份）。
//
// Task 1 不挂任何属性：本头所在的 pk/variant 并不持有那 11 个几何/时间类型，
// 属性落点在 pk/geometry、pk/time 的类型定义上（plan §4.2），Task 2 才动。
// 所以本载体在 Task 1 的树上**必须跑出红**——这一条正是它有没有判别力的证明。

#include "../PkVariant.h"

#include <cstdint>
#include <cstdio>
#include <typeinfo>

// ---------------------------------------------------------------------------
// 负载类型清单 —— plan §4.1 现场枚举出的 14 个（枚举点：PkVariant.h 的非模板
// 构造声明 + PkVariant.cpp:rebindDataPointer 的 case 分支）。
// X-macro，两侧逐字共用同一份清单：改名/增删只改这里，不会有第二份漂移。
//
//   前 3 个（PkString / PkStringList / PkByteArray）= S-17 已修，本任务只做
//   回归覆盖，是**对照组**：两段都必须绿。
//   后 11 个 = 本任务范围，Task 1 未挂属性 ⇒ 期望红；Task 2 挂属性后转绿。
//
// 第二个实参是各类型的规范值：**每个值都用括号包住**，因为预处理器只把
// 圆括号当分组、不认花括号——`PkStringList{...}` 里那个逗号会撕开宏实参。
// ---------------------------------------------------------------------------
#define XIMG_PAYLOAD_TYPES(X)                                                 \
    X(PkString, (PkString("r53-ximg")))                                       \
    X(PkStringList, (PkStringList{PkString("r53"), PkString("ximg")}))        \
    X(PkByteArray, (PkByteArray("r53-ximg")))                                 \
    X(PkPoint, (PkPoint(3, 4)))                                               \
    X(PkPointF, (PkPointF(3.5, 4.5)))                                         \
    X(PkRect, (PkRect(1, 2, 30, 40)))                                         \
    X(PkRectF, (PkRectF(1.5, 2.5, 30.5, 40.5)))                               \
    X(PkSize, (PkSize(30, 40)))                                               \
    X(PkSizeF, (PkSizeF(30.5, 40.5)))                                         \
    X(PkLine, (PkLine(1, 2, 3, 4)))                                           \
    X(PkLineF, (PkLineF(1.5, 2.5, 3.5, 4.5)))                                 \
    X(PkDate, (PkDate(2026, 9, 12)))                                          \
    X(PkTime, (PkTime(13, 14, 15)))                                           \
    X(PkDateTime, (PkDateTime(PkDate(2026, 9, 12), PkTime(13, 14, 15))))

// ---------------------------------------------------------------------------
// 平台门（plan §8）
// 本修法只在 Apple arm64（libc++ impl 3）有意义：Apple x86_64 是 impl 1
// （只比地址、没有名字回退），那里没有可见性层面的解法；libstdc++（Linux）
// 的 type_info::operator== 本就带 strcmp 回退，属性是 no-op。
// ⇒ 非 Apple arm64 上**打印 SKIP 并返回**（不是静默跳过、也不是断言）。
// ---------------------------------------------------------------------------
inline bool ximgPlatformSupported()
{
#if defined(__APPLE__) && defined(__aarch64__)
    return true;
#else
    static bool announced = false;
    if (!announced) {
        announced = true;
        std::fprintf(stderr,
                     "SKIP: 本修法在该平台无效/不必需（见 PkTypeVisibility.h）\n");
    }
    return false;
#endif
}

// ---------------------------------------------------------------------------
// unique 位读数（plan §1.3 的方法，**运行期实测**，不是读 nm 的分类）
// type_info 是多态类，布局 [vptr][__type_name]；__type_name 是 uintptr_t，
// 最高位即 libc++ impl 3 的 non-unique 位。置位 = 属性生效（会走 strcmp 回退）。
// ---------------------------------------------------------------------------
inline std::uintptr_t ximgRawTypeName(const std::type_info &ti)
{
    return reinterpret_cast<const std::uintptr_t *>(&ti)[1];
}

inline bool ximgIsNonUnique(std::uintptr_t rawTypeName)
{
    const std::uintptr_t top = std::uintptr_t(1) << (sizeof(std::uintptr_t) * 8 - 1);
    return (rawTypeName & top) != 0;
}

// ---------------------------------------------------------------------------
// 每镜像一份的探针（inline ⇒ 每个镜像各有一份私有实体，见文件头）
// ---------------------------------------------------------------------------
#define XIMG_DEFINE_IMAGE_PROBE(T, V)                                         \
    /* 在本镜像里造一个装着规范值的 PkVariant */                              \
    inline PkVariant ximg_make_##T() { return PkVariant(V); }                 \
    /* 本镜像里 typeid(T) 那枚 __type_name 的原始值（unique 位读数用） */     \
    inline std::uintptr_t ximg_rawname_##T() { return ximgRawTypeName(typeid(T)); } \
    /* 在本镜像里拷贝一份再核对：拷贝跑在本镜像 ⇒ 判等也在本镜像 */          \
    inline bool ximg_read_##T(const PkVariant &v)                             \
    {                                                                         \
        const PkVariant copy(v);                                              \
        if (copy.constData() == nullptr) {                                    \
            return false;  /* 跨镜像读不到 ⇒ 红判据就是这一行 */              \
        }                                                                     \
        if (copy.type() != v.type()) {                                        \
            return false;                                                     \
        }                                                                     \
        return copy.value<T>() == (V);                                        \
    }
XIMG_PAYLOAD_TYPES(XIMG_DEFINE_IMAGE_PROBE)
#undef XIMG_DEFINE_IMAGE_PROBE

// ---------------------------------------------------------------------------
// SHARED 库导出的桥（exe → lib 镜像的唯一入口）
// 隐藏可见性下必须逐个显式 default 导出，否则 exe 根本看不见它们。
// 返回 std::uintptr_t 的那个是**证据通道**：把库镜像里的 unique 位原始值
// 原样递出来，供红证据引用（plan §5 Task 1 的验收项）。
// ---------------------------------------------------------------------------
#define XIMG_EXPORT __attribute__((visibility("default")))

#define XIMG_DECLARE_BRIDGE(T, V)                                             \
    extern "C" XIMG_EXPORT PkVariant ximg_lib_make_##T(void);                 \
    extern "C" XIMG_EXPORT bool ximg_lib_read_##T(const PkVariant &v);        \
    extern "C" XIMG_EXPORT std::uintptr_t ximg_lib_rawname_##T(void);
XIMG_PAYLOAD_TYPES(XIMG_DECLARE_BRIDGE)
#undef XIMG_DECLARE_BRIDGE
