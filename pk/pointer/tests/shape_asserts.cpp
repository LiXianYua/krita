// 编译期断言，覆盖注释声称但没被运行期断言覆盖到的性质（R线-spec「变异测试的
// 注入清单」点名的第一类：类型/签名形状的性质该用 static_assert 钉，不该只写
// 在注释里）。本文件没有 main、没有运行期代码，只要能编过就是全部断言通过。
#include "../PkSharedPointer.h"
#include "../PkScopedPointer.h"
#include <type_traits>

struct B { int v = 0; virtual ~B() {} };

// 判据 C：Qt 的 QScopedPointer 四项全 0（探针 P10）。
static_assert(!std::is_copy_constructible<PkScopedPointer<B>>::value, "");
static_assert(!std::is_move_constructible<PkScopedPointer<B>>::value, "");
static_assert(!std::is_copy_assignable<PkScopedPointer<B>>::value, "");
static_assert(!std::is_move_assignable<PkScopedPointer<B>>::value, "");
static_assert(!std::is_copy_constructible<PkScopedArrayPointer<B>>::value, "");
static_assert(!std::is_move_constructible<PkScopedArrayPointer<B>>::value, "");

// PkSharedPointer 跟 Qt 一样可拷贝可移动（探针 P10 最后一行）。
static_assert(std::is_copy_constructible<PkSharedPointer<B>>::value, "");
static_assert(std::is_move_constructible<PkSharedPointer<B>>::value, "");

// 判据 B：布尔转换是隐式的（Qt implicit=1），而不是 explicit。
static_assert(std::is_convertible<PkSharedPointer<B>, bool>::value, "");
static_assert(std::is_convertible<PkScopedPointer<B>, bool>::value, "");
static_assert(std::is_convertible<PkWeakPointer<B>, bool>::value, "");

// 探针 P16：派生→基类可以，反向不行。
struct D : B {};
static_assert(std::is_convertible<PkSharedPointer<D>, PkSharedPointer<B>>::value, "");
static_assert(!std::is_convertible<PkSharedPointer<B>, PkSharedPointer<D>>::value, "");
