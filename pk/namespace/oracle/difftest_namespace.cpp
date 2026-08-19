// difftest_namespace.cpp —— pk/namespace 的枚举位值与真 Qt5 的对拍。
//
// 与 pk/global 的 global_difftest.cpp 同一骨架：**单 TU 双侧**，Q 侧在全局作用域
// include 真 Qt 头，P 侧塞进 `namespace pkoracle`。但枚举的「对拍」本质是**编译期**
// 比对：两个值要么相同要么不同，没有运行期中间态。所以这里每条检查都写成
// static_assert（编译不过 = 位值不一致 = FAIL），另加运行期计数，让输出走与
// global 相同的 DIFF 契约（run_oracle.sh 读 DIFF 行）。
//
// ── 输出契约（run_oracle.sh 读这些行）───────────────────────────────
//     DIFF total=<N> mismatch=<M>      恰好一行，程序末尾打
//     DIFFTAG <enum> <name> <count>      位值分家的枚举项，一行一条（应为空）
// 退出码必须是 0，即使 mismatch>0（已声明的偏离不算失败）——本 oracle 的期望是
// mismatch=0（零已登记偏离，全部枚举照抄真 Qt），但契约写法保持与 global 一致。
//
// ── 为什么替代品要塞进 namespace pkoracle ─────────────────────────────
// PkNamespace.h 里的 `namespace Qt` 与真 Qt 的 `::Qt` 同名——两个头直接进同一个
// 全局作用域会在 namespace Qt 里重复定义枚举（硬错误）。解法与 global 相同：
// `namespace pkoracle { #include "PkNamespace.h" }`，让它的 Qt 落在
// pkoracle::Qt。PkNamespace.h 又会带进 PkGlobal.h（标量 + Qt::AspectRatioMode/
// Axis）与 PkFlags.h（纯模板）——PkGlobal.h 只 #include <cmath>/<limits>、
// PkFlags.h 只 #include <initializer_list>/<type_traits>，这四个必须由本文件
// **在 namespace 之外先 include**（include guard 让 namespace 里的二次 include
// 空转），否则 std 会被卷进 pkoracle::std。
//
// ── 两侧真的各链各的吗 ────────────────────────────────────────────────
// 本 oracle 不含任何 Q_CORE_EXPORT 的 out-of-line 函数（枚举全部编译期内联），
// 链不链 Qt 库都不影响运行结果。所以「真的链上了 Qt」由 run_oracle.sh 的 ldd
// 检查保证——**必须**看到 libQt5Core（-lQt5Core 显式给了，链不上就 FAIL）。
// 枚举位值本身没有「运行期取到真 Qt 值」一说：static_assert 的 Q 侧直接编译期
// 解析自真 Qt 头，这就是对拍。

// ── 真 Qt 侧 + 系统头（都必须在 namespace 之外）────────────────────────
#include <QtGlobal>
#include <QtCore/qnamespace.h>

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <type_traits>

// ── 替代品侧 ───────────────────────────────────────────────────────────
namespace pkoracle {
#include "PkNamespace.h"
}

// ── 计数（照 R-03/R-18 骨架）────────────────────────────────────────────
// mismatch 恒为 0：每条检查都带 static_assert（编译期硬保证），编译过了运行期
// 就不可能分家。DIFF 行的 mismatch 保留在契约里，让 run_oracle.sh 的解析器与
// global 通用；实际只会打出 0。
static long g_total = 0, g_mismatch = 0;

#define PKN_CHECK(enumName, enumerator)                                            \
    do {                                                                           \
        /* long long 两侧同类型，消掉 -Wenum-compare；也能装下 unsigned 枚举 */      \
        static_assert(static_cast<long long>(::Qt::enumerator)                      \
                          == static_cast<long long>(pkoracle::Qt::enumerator),     \
                      "Qt::" #enumerator " 位值与真 Qt 不一致");                    \
        ++g_total;                                                                 \
    } while (0)

int main()
{
    // ── KeyboardModifier / Modifier ─────────────────────────────────────
    PKN_CHECK(KeyboardModifier, NoModifier);
    PKN_CHECK(KeyboardModifier, ShiftModifier);
    PKN_CHECK(KeyboardModifier, ControlModifier);
    PKN_CHECK(KeyboardModifier, AltModifier);
    PKN_CHECK(KeyboardModifier, MetaModifier);
    PKN_CHECK(KeyboardModifier, KeypadModifier);
    PKN_CHECK(KeyboardModifier, GroupSwitchModifier);
    PKN_CHECK(KeyboardModifier, KeyboardModifierMask);
    PKN_CHECK(Modifier, META);
    PKN_CHECK(Modifier, SHIFT);
    PKN_CHECK(Modifier, CTRL);
    PKN_CHECK(Modifier, ALT);
    PKN_CHECK(Modifier, MODIFIER_MASK);
    PKN_CHECK(Modifier, UNICODE_ACCEL);

    // ── MouseButton ────────────────────────────────────────────────────
    PKN_CHECK(MouseButton, NoButton);
    PKN_CHECK(MouseButton, LeftButton);
    PKN_CHECK(MouseButton, RightButton);
    PKN_CHECK(MouseButton, MiddleButton);
    PKN_CHECK(MouseButton, BackButton);
    PKN_CHECK(MouseButton, XButton1);
    PKN_CHECK(MouseButton, ExtraButton1);
    PKN_CHECK(MouseButton, ForwardButton);
    PKN_CHECK(MouseButton, XButton2);
    PKN_CHECK(MouseButton, ExtraButton2);
    PKN_CHECK(MouseButton, TaskButton);
    PKN_CHECK(MouseButton, ExtraButton3);
    PKN_CHECK(MouseButton, ExtraButton4);
    PKN_CHECK(MouseButton, ExtraButton5);
    PKN_CHECK(MouseButton, ExtraButton6);
    PKN_CHECK(MouseButton, ExtraButton7);
    PKN_CHECK(MouseButton, ExtraButton8);
    PKN_CHECK(MouseButton, ExtraButton9);
    PKN_CHECK(MouseButton, ExtraButton10);
    PKN_CHECK(MouseButton, ExtraButton11);
    PKN_CHECK(MouseButton, ExtraButton12);
    PKN_CHECK(MouseButton, ExtraButton13);
    PKN_CHECK(MouseButton, ExtraButton14);
    PKN_CHECK(MouseButton, ExtraButton15);
    PKN_CHECK(MouseButton, ExtraButton16);
    PKN_CHECK(MouseButton, ExtraButton17);
    PKN_CHECK(MouseButton, ExtraButton18);
    PKN_CHECK(MouseButton, ExtraButton19);
    PKN_CHECK(MouseButton, ExtraButton20);
    PKN_CHECK(MouseButton, ExtraButton21);
    PKN_CHECK(MouseButton, ExtraButton22);
    PKN_CHECK(MouseButton, ExtraButton23);
    PKN_CHECK(MouseButton, ExtraButton24);
    PKN_CHECK(MouseButton, AllButtons);
    PKN_CHECK(MouseButton, MaxMouseButton);
    PKN_CHECK(MouseButton, MouseButtonMask);

    // ── Orientation / FocusPolicy / SortOrder / SplitBehaviorFlags ─────
    PKN_CHECK(Orientation, Horizontal);
    PKN_CHECK(Orientation, Vertical);
    PKN_CHECK(FocusPolicy, NoFocus);
    PKN_CHECK(FocusPolicy, TabFocus);
    PKN_CHECK(FocusPolicy, ClickFocus);
    PKN_CHECK(FocusPolicy, StrongFocus);
    PKN_CHECK(FocusPolicy, WheelFocus);
    PKN_CHECK(SortOrder, AscendingOrder);
    PKN_CHECK(SortOrder, DescendingOrder);
    PKN_CHECK(SplitBehaviorFlags, KeepEmptyParts);
    PKN_CHECK(SplitBehaviorFlags, SkipEmptyParts);

    // ── AlignmentFlag / TextFlag ───────────────────────────────────────
    PKN_CHECK(AlignmentFlag, AlignLeft);
    PKN_CHECK(AlignmentFlag, AlignRight);
    PKN_CHECK(AlignmentFlag, AlignHCenter);
    PKN_CHECK(AlignmentFlag, AlignTop);
    PKN_CHECK(AlignmentFlag, AlignBottom);
    PKN_CHECK(AlignmentFlag, AlignVCenter);
    PKN_CHECK(AlignmentFlag, AlignCenter);
    PKN_CHECK(TextFlag, TextSingleLine);
    PKN_CHECK(TextFlag, TextDontClip);
    PKN_CHECK(TextFlag, TextExpandTabs);
    PKN_CHECK(TextFlag, TextShowMnemonic);
    PKN_CHECK(TextFlag, TextWordWrap);
    PKN_CHECK(TextFlag, TextWrapAnywhere);
    PKN_CHECK(TextFlag, TextDontPrint);
    PKN_CHECK(TextFlag, TextHideMnemonic);
    PKN_CHECK(TextFlag, TextJustificationForced);
    PKN_CHECK(TextFlag, TextForceLeftToRight);
    PKN_CHECK(TextFlag, TextForceRightToLeft);
    PKN_CHECK(TextFlag, TextLongestVariant);
    PKN_CHECK(TextFlag, TextIncludeTrailingSpaces);

    // ── ImageConversionFlag ────────────────────────────────────────────
    PKN_CHECK(ImageConversionFlag, ColorMode_Mask);
    PKN_CHECK(ImageConversionFlag, AutoColor);
    PKN_CHECK(ImageConversionFlag, ColorOnly);
    PKN_CHECK(ImageConversionFlag, MonoOnly);
    PKN_CHECK(ImageConversionFlag, AlphaDither_Mask);
    PKN_CHECK(ImageConversionFlag, ThresholdAlphaDither);
    PKN_CHECK(ImageConversionFlag, OrderedAlphaDither);
    PKN_CHECK(ImageConversionFlag, DiffuseAlphaDither);
    PKN_CHECK(ImageConversionFlag, NoAlpha);
    PKN_CHECK(ImageConversionFlag, Dither_Mask);
    PKN_CHECK(ImageConversionFlag, DiffuseDither);
    PKN_CHECK(ImageConversionFlag, OrderedDither);
    PKN_CHECK(ImageConversionFlag, ThresholdDither);
    PKN_CHECK(ImageConversionFlag, DitherMode_Mask);
    PKN_CHECK(ImageConversionFlag, AutoDither);
    PKN_CHECK(ImageConversionFlag, PreferDither);
    PKN_CHECK(ImageConversionFlag, AvoidDither);
    PKN_CHECK(ImageConversionFlag, NoOpaqueDetection);
    PKN_CHECK(ImageConversionFlag, NoFormatConversion);

    // ── Key（裁剪子集：只检查本头实现的成员）───────────────────────────
    PKN_CHECK(Key, Key_Escape);
    PKN_CHECK(Key, Key_Backspace);
    PKN_CHECK(Key, Key_Return);
    PKN_CHECK(Key, Key_Enter);
    PKN_CHECK(Key, Key_Delete);
    PKN_CHECK(Key, Key_Left);
    PKN_CHECK(Key, Key_Up);
    PKN_CHECK(Key, Key_Right);
    PKN_CHECK(Key, Key_Down);
    PKN_CHECK(Key, Key_Shift);
    PKN_CHECK(Key, Key_Control);
    PKN_CHECK(Key, Key_Meta);
    PKN_CHECK(Key, Key_Alt);
    PKN_CHECK(Key, Key_Space);
    PKN_CHECK(Key, Key_A);
    PKN_CHECK(Key, Key_B);
    PKN_CHECK(Key, Key_F);
    PKN_CHECK(Key, Key_G);
    PKN_CHECK(Key, Key_I);
    PKN_CHECK(Key, Key_J);
    PKN_CHECK(Key, Key_L);
    PKN_CHECK(Key, Key_M);
    PKN_CHECK(Key, Key_P);
    PKN_CHECK(Key, Key_Q);
    PKN_CHECK(Key, Key_R);
    PKN_CHECK(Key, Key_T);
    PKN_CHECK(Key, Key_U);
    PKN_CHECK(Key, Key_BracketLeft);
    PKN_CHECK(Key, Key_BracketRight);

    // ── PenStyle / PenCapStyle / PenJoinStyle / BrushStyle ─────────────
    PKN_CHECK(PenStyle, NoPen);
    PKN_CHECK(PenStyle, SolidLine);
    PKN_CHECK(PenStyle, DashLine);
    PKN_CHECK(PenStyle, DotLine);
    PKN_CHECK(PenStyle, DashDotLine);
    PKN_CHECK(PenStyle, DashDotDotLine);
    PKN_CHECK(PenStyle, CustomDashLine);
    PKN_CHECK(PenCapStyle, FlatCap);
    PKN_CHECK(PenCapStyle, SquareCap);
    PKN_CHECK(PenCapStyle, RoundCap);
    PKN_CHECK(PenJoinStyle, MiterJoin);
    PKN_CHECK(PenJoinStyle, BevelJoin);
    PKN_CHECK(PenJoinStyle, RoundJoin);
    PKN_CHECK(PenJoinStyle, SvgMiterJoin);
    PKN_CHECK(BrushStyle, NoBrush);
    PKN_CHECK(BrushStyle, SolidPattern);
    PKN_CHECK(BrushStyle, Dense1Pattern);
    PKN_CHECK(BrushStyle, Dense2Pattern);
    PKN_CHECK(BrushStyle, Dense3Pattern);
    PKN_CHECK(BrushStyle, Dense4Pattern);
    PKN_CHECK(BrushStyle, Dense5Pattern);
    PKN_CHECK(BrushStyle, Dense6Pattern);
    PKN_CHECK(BrushStyle, Dense7Pattern);
    PKN_CHECK(BrushStyle, HorPattern);
    PKN_CHECK(BrushStyle, VerPattern);
    PKN_CHECK(BrushStyle, CrossPattern);
    PKN_CHECK(BrushStyle, BDiagPattern);
    PKN_CHECK(BrushStyle, FDiagPattern);
    PKN_CHECK(BrushStyle, DiagCrossPattern);
    PKN_CHECK(BrushStyle, LinearGradientPattern);
    PKN_CHECK(BrushStyle, RadialGradientPattern);
    PKN_CHECK(BrushStyle, ConicalGradientPattern);
    PKN_CHECK(BrushStyle, TexturePattern);

    // ── CursorShape ────────────────────────────────────────────────────
    PKN_CHECK(CursorShape, ArrowCursor);
    PKN_CHECK(CursorShape, UpArrowCursor);
    PKN_CHECK(CursorShape, CrossCursor);
    PKN_CHECK(CursorShape, WaitCursor);
    PKN_CHECK(CursorShape, IBeamCursor);
    PKN_CHECK(CursorShape, SizeVerCursor);
    PKN_CHECK(CursorShape, SizeHorCursor);
    PKN_CHECK(CursorShape, SizeBDiagCursor);
    PKN_CHECK(CursorShape, SizeFDiagCursor);
    PKN_CHECK(CursorShape, SizeAllCursor);
    PKN_CHECK(CursorShape, BlankCursor);
    PKN_CHECK(CursorShape, SplitVCursor);
    PKN_CHECK(CursorShape, SplitHCursor);
    PKN_CHECK(CursorShape, PointingHandCursor);
    PKN_CHECK(CursorShape, ForbiddenCursor);
    PKN_CHECK(CursorShape, WhatsThisCursor);
    PKN_CHECK(CursorShape, BusyCursor);
    PKN_CHECK(CursorShape, OpenHandCursor);
    PKN_CHECK(CursorShape, ClosedHandCursor);
    PKN_CHECK(CursorShape, DragCopyCursor);
    PKN_CHECK(CursorShape, DragMoveCursor);
    PKN_CHECK(CursorShape, DragLinkCursor);
    PKN_CHECK(CursorShape, LastCursor);

    // ── TextFormat / DateFormat / TimeSpec / ScrollBarPolicy ───────────
    PKN_CHECK(TextFormat, PlainText);
    PKN_CHECK(TextFormat, RichText);
    PKN_CHECK(TextFormat, AutoText);
    PKN_CHECK(DateFormat, TextDate);
    PKN_CHECK(DateFormat, ISODate);
    PKN_CHECK(DateFormat, RFC2822Date);
    PKN_CHECK(DateFormat, ISODateWithMs);
    PKN_CHECK(TimeSpec, LocalTime);
    PKN_CHECK(TimeSpec, UTC);
    PKN_CHECK(TimeSpec, OffsetFromUTC);
    PKN_CHECK(ScrollBarPolicy, ScrollBarAsNeeded);
    PKN_CHECK(ScrollBarPolicy, ScrollBarAlwaysOff);
    PKN_CHECK(ScrollBarPolicy, ScrollBarAlwaysOn);

    // ── CaseSensitivity / ConnectionType / FillRule / ClipOperation ────
    PKN_CHECK(CaseSensitivity, CaseInsensitive);
    PKN_CHECK(CaseSensitivity, CaseSensitive);
    PKN_CHECK(ConnectionType, AutoConnection);
    PKN_CHECK(ConnectionType, DirectConnection);
    PKN_CHECK(ConnectionType, QueuedConnection);
    PKN_CHECK(ConnectionType, BlockingQueuedConnection);
    PKN_CHECK(ConnectionType, UniqueConnection);
    PKN_CHECK(FillRule, OddEvenFill);
    PKN_CHECK(FillRule, WindingFill);
    PKN_CHECK(ClipOperation, NoClip);
    PKN_CHECK(ClipOperation, ReplaceClip);
    PKN_CHECK(ClipOperation, IntersectClip);

    // ── TransformationMode / LayoutDirection / CheckState ──────────────
    PKN_CHECK(TransformationMode, FastTransformation);
    PKN_CHECK(TransformationMode, SmoothTransformation);
    PKN_CHECK(LayoutDirection, LeftToRight);
    PKN_CHECK(LayoutDirection, RightToLeft);
    PKN_CHECK(LayoutDirection, LayoutDirectionAuto);
    PKN_CHECK(CheckState, Unchecked);
    PKN_CHECK(CheckState, PartiallyChecked);
    PKN_CHECK(CheckState, Checked);

    // ── ItemDataRole / ItemFlag / TimerType ─────────────────────────────
    PKN_CHECK(ItemDataRole, DisplayRole);
    PKN_CHECK(ItemDataRole, DecorationRole);
    PKN_CHECK(ItemDataRole, EditRole);
    PKN_CHECK(ItemDataRole, ToolTipRole);
    PKN_CHECK(ItemDataRole, StatusTipRole);
    PKN_CHECK(ItemDataRole, WhatsThisRole);
    PKN_CHECK(ItemDataRole, FontRole);
    PKN_CHECK(ItemDataRole, TextAlignmentRole);
    PKN_CHECK(ItemDataRole, BackgroundRole);
    PKN_CHECK(ItemDataRole, ForegroundRole);
    PKN_CHECK(ItemDataRole, CheckStateRole);
    PKN_CHECK(ItemDataRole, UserRole);
    PKN_CHECK(ItemFlag, NoItemFlags);
    PKN_CHECK(ItemFlag, ItemIsSelectable);
    PKN_CHECK(ItemFlag, ItemIsEditable);
    PKN_CHECK(ItemFlag, ItemIsDragEnabled);
    PKN_CHECK(ItemFlag, ItemIsDropEnabled);
    PKN_CHECK(ItemFlag, ItemIsUserCheckable);
    PKN_CHECK(ItemFlag, ItemIsEnabled);
    PKN_CHECK(ItemFlag, ItemIsAutoTristate);
    PKN_CHECK(ItemFlag, ItemNeverHasChildren);
    PKN_CHECK(ItemFlag, ItemIsUserTristate);
    PKN_CHECK(TimerType, PreciseTimer);
    PKN_CHECK(TimerType, CoarseTimer);
    PKN_CHECK(TimerType, VeryCoarseTimer);

    // ── GlobalColor ────────────────────────────────────────────────────
    PKN_CHECK(GlobalColor, color0);
    PKN_CHECK(GlobalColor, color1);
    PKN_CHECK(GlobalColor, black);
    PKN_CHECK(GlobalColor, white);
    PKN_CHECK(GlobalColor, darkGray);
    PKN_CHECK(GlobalColor, gray);
    PKN_CHECK(GlobalColor, lightGray);
    PKN_CHECK(GlobalColor, red);
    PKN_CHECK(GlobalColor, green);
    PKN_CHECK(GlobalColor, blue);
    PKN_CHECK(GlobalColor, cyan);
    PKN_CHECK(GlobalColor, magenta);
    PKN_CHECK(GlobalColor, yellow);
    PKN_CHECK(GlobalColor, darkRed);
    PKN_CHECK(GlobalColor, darkGreen);
    PKN_CHECK(GlobalColor, darkBlue);
    PKN_CHECK(GlobalColor, darkCyan);
    PKN_CHECK(GlobalColor, darkMagenta);
    PKN_CHECK(GlobalColor, darkYellow);
    PKN_CHECK(GlobalColor, transparent);

    // ── 来自 PkGlobal.h 的成员（R-18 交付，同一 namespace Qt 的并集）────
    PKN_CHECK(AspectRatioMode, IgnoreAspectRatio);
    PKN_CHECK(AspectRatioMode, KeepAspectRatio);
    PKN_CHECK(AspectRatioMode, KeepAspectRatioByExpanding);
    PKN_CHECK(Axis, XAxis);
    PKN_CHECK(Axis, YAxis);
    PKN_CHECK(Axis, ZAxis);

    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;
}
