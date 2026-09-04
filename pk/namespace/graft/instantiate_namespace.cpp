// instantiate_namespace.cpp —— R-27 Task 2 的 graft 试接证据（调用形状复刻）。
//
// 这不是真实生产文件，是**复刻 4 个真实调用点形状的 driver**。真实文件
// （KoToolBase.cpp / KoToolProxy.cpp / KoShapeRubberSelectStrategy.cpp /
// DefaultTool.cpp）在 libs/flake 与 plugins/tools 大模块，编它们需要 QDebug、
// kritaflake_export.h、KoInteractionTool.h 等几十个未交付类型（依赖墙），本任务
// locks=pk/namespace 物理编不过。见 R线-spec「依赖墙挡住真实测试类时」的降级
// 四条件：这里只证明本任务交付的枚举族 + pk/flags 的 PkFlags 模板，在真实调用
// 形状下可用且语义正确。
//
// 复刻的调用形状（逐条对应真实文件，行号见头注释）：
//   1. KoToolBase.cpp:152 —— `QKeyEvent(..., QFlags<Pk::KeyboardModifier>(), ...)`
//      空 flags 作为实参。
//   2. KoShapeRubberSelectStrategy.cpp:26 —— `snap(clicked, QFlags<Pk::KeyboardModifier>())`
//      空 flags 作为实参。
//   3. KoToolProxy.cpp:77 —— `QMouseEvent(..., Pk::LeftButton, Pk::LeftButton,
//      QFlags<Pk::KeyboardModifier>())` 单个 MouseButton 枚举作实参（隐式转复数）。
//   4. DefaultTool.cpp:145 —— `Pk::KeyboardModifiers modifiers = QFlags<Pk::KeyboardModifier>()`
//      KeyboardModifiers 复数类型用空 QFlags 作默认实参。
#include <QFlags>          // → pk/flags/compat/QFlags（-I pk/flags/compat），#define QFlags PkFlags
#include "PkNamespace.h"   // → Pk::KeyboardModifier / Pk::MouseButton / Pk::KeyboardModifiers

// shape 4（DefaultTool.cpp:145）：KeyboardModifiers 的默认实参是空 QFlags。
static void finishInteraction(Pk::KeyboardModifiers modifiers = QFlags<Pk::KeyboardModifier>())
{
    Q_UNUSED(modifiers);
}

// shape 1/2 的实参接收器（真实代码是 QKeyEvent/QPointF 这类未交付类型，这里只
// 复刻「把 QFlags<Pk::KeyboardModifier>() 当实参传」的形状）。
static void takesKeyboardFlags(QFlags<Pk::KeyboardModifier>) {}

// shape 3 的按钮实参接收器（真实代码是 QMouseEvent 构造的 Pk::MouseButtons 参数）。
static void takesMouseButtons(Pk::MouseButtons buttons)
{
    Q_UNUSED(buttons);
}

int main()
{
    // shape 1：KoToolBase.cpp:152 的空 flags 实参。
    takesKeyboardFlags(QFlags<Pk::KeyboardModifier>());

    // shape 2：KoShapeRubberSelectStrategy.cpp:26 的空 flags 实参。
    takesKeyboardFlags(QFlags<Pk::KeyboardModifier>());

    // shape 3：KoToolProxy.cpp:77 的 `Pk::LeftButton` 单枚举 → Pk::MouseButtons 复数。
    takesMouseButtons(Pk::LeftButton);
    takesMouseButtons(Pk::LeftButton | Pk::RightButton);

    // shape 4：默认实参走空 QFlags → KeyboardModifiers。
    finishInteraction();
    finishInteraction(Pk::KeyboardModifiers());

    // ── 语义核对（值来自真 Qt 探针）──────────────────────────────────────
    if (int(QFlags<Pk::KeyboardModifier>()) != 0) return 1;           // 空 flags = 0
    if (int(Pk::KeyboardModifiers()) != 0) return 2;                  // 复数默认构造 = 0

    Pk::KeyboardModifiers mods = Pk::ControlModifier | Pk::ShiftModifier;
    if (int(mods) != 0x06000000) return 3;                            // enum|enum 自由 operator|
    if (!mods.testFlag(Pk::ControlModifier)) return 4;
    if (!mods.testFlag(Pk::ShiftModifier)) return 5;
    if (mods.testFlag(Pk::AltModifier)) return 6;

    mods |= Pk::AltModifier;                                          // operator|=(Enum)
    if (int(mods) != 0x0e000000) return 7;
    mods.setFlag(Pk::ShiftModifier, false);
    if (int(mods) != 0x0c000000) return 8;

    Pk::MouseButtons btns = Pk::LeftButton | Pk::MiddleButton;        // MouseButton 复数
    if (int(btns) != 0x00000005) return 9;
    if (!btns.testFlag(Pk::LeftButton)) return 10;
    if (btns.testFlag(Pk::RightButton)) return 11;

    return 0;
}
