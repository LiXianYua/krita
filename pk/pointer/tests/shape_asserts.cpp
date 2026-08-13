// 编译期断言，覆盖注释声称但没被运行期断言覆盖到的性质（R线-spec「变异测试的
// 注入清单」点名的第一类：类型/签名形状的性质该用 static_assert 钉，不该只写
// 在注释里）。本文件没有 main、没有运行期代码，只要能编过就是全部断言通过。
#include "../PkSharedPointer.h"
#include "../PkScopedPointer.h"
#include <type_traits>
#include <utility>

// 类型名唯一化（ShapeB/ShapeD 而非裸 B/D）：跟 test_shared.cpp/test_weak.cpp/
// test_scoped.cpp 里做的一致。static_assert 只看类型特征、不受跨 TU 的运行期
// 符号折叠影响（本文件目前无害），但裸名字是给以后复制粘贴出真正的运行期用例
// 埋一个跟三份测试文件曾经踩过的同一个坑（细节见 task-1-report.md「遇到的
// 问题」一节）。
struct ShapeB { int v = 0; virtual ~ShapeB() {} };

// 判据 C：Qt 的 QScopedPointer 四项全 0（探针 P10）。
static_assert(!std::is_copy_constructible<PkScopedPointer<ShapeB>>::value, "");
static_assert(!std::is_move_constructible<PkScopedPointer<ShapeB>>::value, "");
static_assert(!std::is_copy_assignable<PkScopedPointer<ShapeB>>::value, "");
static_assert(!std::is_move_assignable<PkScopedPointer<ShapeB>>::value, "");
static_assert(!std::is_copy_constructible<PkScopedArrayPointer<ShapeB>>::value, "");
static_assert(!std::is_move_constructible<PkScopedArrayPointer<ShapeB>>::value, "");

// PkSharedPointer 跟 Qt 一样可拷贝可移动（探针 P10 最后一行）。
static_assert(std::is_copy_constructible<PkSharedPointer<ShapeB>>::value, "");
static_assert(std::is_move_constructible<PkSharedPointer<ShapeB>>::value, "");

// 判据 B：布尔转换是隐式的（Qt implicit=1），而不是 explicit。
static_assert(std::is_convertible<PkSharedPointer<ShapeB>, bool>::value, "");
static_assert(std::is_convertible<PkScopedPointer<ShapeB>, bool>::value, "");
static_assert(std::is_convertible<PkWeakPointer<ShapeB>, bool>::value, "");

// 探针 P16：派生→基类可以，反向不行。
struct ShapeD : ShapeB {};
static_assert(std::is_convertible<PkSharedPointer<ShapeD>, PkSharedPointer<ShapeB>>::value, "");
static_assert(!std::is_convertible<PkSharedPointer<ShapeB>, PkSharedPointer<ShapeD>>::value, "");

// ---------------------------------------------------------------------------
// 判据 B 的负向一半：`p == 1` 必须编不过。
//
// 现在只由「没有能接住 int 的重载」这个结构事实保证，没有断言钉住它——而判据 B
// 的全部意义就是「用 safe-bool（operator RestrictedBool）而不是裸
// operator bool，后者会让 p == 1 编过」（探针编译矩阵实测：Qt 上
// `QSharedPointer p==1` 是 FAIL）。用检测惯用法（std::void_t + 偏特化）探测
// `declval<T>() == declval<U>()` 是否良构。
// ---------------------------------------------------------------------------
template <class T, class U, class = void>
struct PkIsEqComparable : std::false_type {};

template <class T, class U>
struct PkIsEqComparable<T, U,
    std::void_t<decltype(std::declval<T>() == std::declval<U>())>> : std::true_type {};

// 对照组：trait 本身要能识别「确实可比」，否则 trait 写错成恒 false 时下面的
// 三条断言会假通过而实际没验证到该验证的事——负向断言必须配一条正向对照，
// 不然"写错成恒 false"与"写对了"在输出上无法区分。
static_assert(PkIsEqComparable<int, int>::value, "trait 本身必须能识别可比较的情形");

static_assert(!PkIsEqComparable<PkSharedPointer<ShapeB>, int>::value,
              "判据 B：PkSharedPointer 不能与 int 比较（safe-bool，不是裸 operator bool）");
static_assert(!PkIsEqComparable<PkScopedPointer<ShapeB>, int>::value,
              "判据 B：PkScopedPointer 不能与 int 比较");
static_assert(!PkIsEqComparable<PkWeakPointer<ShapeB>, int>::value,
              "判据 B：PkWeakPointer 不能与 int 比较");
