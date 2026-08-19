// pk/namespace/PkNamespace.h —— Qt 命名空间枚举族的替代品（R-27 Task 2）
//
// **对齐口径**：与真 Qt 5.15.7 的 qnamespace.h 逐位一致。枚举值与顺序直接照抄
// $QT/include/QtCore/qnamespace.h（QT_VERSION_STR "5.15.7"），来源行号标在各项
// 上方；位值探针证据在 .superpowers/sdd/R-27/probe_qnamespace.out。任何一位与
// Qt 不一致默认都是缺陷。
//
// **与 pk/global 的分权**（R-18 已交付）：PkGlobal.h 已定义 `namespace Qt` 的
// `AspectRatioMode` / `Axis` / `SizeMode` / `FillRule` / `GlobalColor` /
// `TransformationMode` 六个枚举（qnamespace.h:1235-1239 / 1386-1390 /
// 1185-1187 / 1352-1355 / 75-96 / 1381-1384）。
// **本头不重定义这六个**——重定义会与 R-18 的 namespace Qt 同名枚举硬错。同一
// TU 同时 include pk/global + pk/namespace 时，两个 enum 集合在同一个 namespace
// Qt 里**并集可见**（C++ 允许同名 namespace 多次打开，只要枚举名不重复）——这
// 正好构成完整 Qt 枚举集。测试的 coexistWithGlobal 探针钉住这一点。
//
// **工程形态**（R-18 先例）：真 `namespace Qt { ... }`，不是 `#define Qt`。
// #define 会炸 `#include <Qt>` 伞形头（libs/flake 有 4 处）与 QTextStream::fixed。
//
// **Key 枚举的裁剪策略**（判据①）：真 Qt 的 Key 有 500+ 值，保留范围实测只用了
// ~30 个。只实现保留范围用量 > 0 的 Key 值 + brief 测试要求的 Key_A；其余按
// qnamespace.h 的规律登记（可打印键 = ASCII/Unicode 码点，F1-F35/Home/End/等 =
// 0x01000000 基址 + 键码），不实现。见 README「偏离登记」。
//
// **QFlags 复数类型**：qnamespace.h 里每个 flags 型枚举配 Q_DECLARE_FLAGS 的复数
// 别名（KeyboardModifiers / MouseButtons / Alignment / ItemFlags / Orientations /
// SplitBehavior / ImageConversionFlags）。本头用 pk/flags 的 PK_DECLARE_FLAGS /
// PK_DECLARE_OPERATORS_FOR_FLAGS 复刻同一形态（PkFlags<Enum> 模板，行为对齐
// Qt 5.15 <QtCore/qflags.h>）。
#pragma once

#include "../global/PkGlobal.h"   // 标量 + Qt::AspectRatioMode/Axis（同一 namespace Qt 的成员）
#include "../flags/PkFlags.h"     // PkFlags<Enum> + PK_DECLARE_FLAGS / PK_DECLARE_OPERATORS_FOR_FLAGS

namespace Qt {

// ── qnamespace.h:98-108 ──────────────────────────────────────────────────────
// 探针：No=0x0 Shift=0x2000000 Ctrl=0x4000000 Alt=0x8000000 Meta=0x10000000
// Keypad=0x20000000 GroupSwitch=0x40000000 Mask=0xfe000000
enum KeyboardModifier {
    NoModifier           = 0x00000000,
    ShiftModifier        = 0x02000000,
    ControlModifier      = 0x04000000,
    AltModifier          = 0x08000000,
    MetaModifier         = 0x10000000,
    KeypadModifier       = 0x20000000,
    GroupSwitchModifier  = 0x40000000,
    // qnamespace.h 注释：Do not extend the mask to include 0x01000000
    KeyboardModifierMask = 0xfe000000
};
PK_DECLARE_FLAGS(KeyboardModifiers, KeyboardModifier)
PK_DECLARE_OPERATORS_FOR_FLAGS(KeyboardModifiers)

// ── qnamespace.h:117-124 ─────────────────────────────────────────────────────
// 快捷键短名（CTRL/SHIFT/ALT 在保留范围各有用量）。KeypadModifier 刻意不进短名
// 枚举（qnamespace.h 注释：全大写标识符易与用户宏冲突）。
enum Modifier {
    META          = Qt::MetaModifier,
    SHIFT         = Qt::ShiftModifier,
    CTRL          = Qt::ControlModifier,
    ALT           = Qt::AltModifier,
    MODIFIER_MASK = KeyboardModifierMask,
    UNICODE_ACCEL = 0x00000000
};

// ── qnamespace.h:126-167 ─────────────────────────────────────────────────────
// 探针：No=0 Left=1 Right=2 Middle=4 Back=8 Forward=16 Task=32。
// ⚠ 相对 brief 示例代码的一处修正：真 Qt 5.15.7 的 MaxMouseButton = ExtraButton24
// （= 0x04000000），不是 TaskButton。libs/input/kis_stroke_shortcut.cpp:36 的
// `std::log2((int)Qt::MaxMouseButton)` 依赖这一位的值；libs/input/
// kis_shortcut_configuration.cpp 的 BOOST_PP_REPEAT_FROM_TO(4,25,EXTRA_BUTTON)
// 宏实例化 ExtraButton4..ExtraButton24。故 ExtraButton4-24 全量照抄。
enum MouseButton {
    NoButton         = 0x00000000,
    LeftButton       = 0x00000001,
    RightButton      = 0x00000002,
    MiddleButton     = 0x00000004,
    BackButton       = 0x00000008,
    XButton1         = BackButton,
    ExtraButton1     = XButton1,
    ForwardButton    = 0x00000010,
    XButton2         = ForwardButton,
    ExtraButton2     = ForwardButton,
    TaskButton       = 0x00000020,
    ExtraButton3     = TaskButton,
    ExtraButton4     = 0x00000040,
    ExtraButton5     = 0x00000080,
    ExtraButton6     = 0x00000100,
    ExtraButton7     = 0x00000200,
    ExtraButton8     = 0x00000400,
    ExtraButton9     = 0x00000800,
    ExtraButton10    = 0x00001000,
    ExtraButton11    = 0x00002000,
    ExtraButton12    = 0x00004000,
    ExtraButton13    = 0x00008000,
    ExtraButton14    = 0x00010000,
    ExtraButton15    = 0x00020000,
    ExtraButton16    = 0x00040000,
    ExtraButton17    = 0x00080000,
    ExtraButton18    = 0x00100000,
    ExtraButton19    = 0x00200000,
    ExtraButton20    = 0x00400000,
    ExtraButton21    = 0x00800000,
    ExtraButton22    = 0x01000000,
    ExtraButton23    = 0x02000000,
    ExtraButton24    = 0x04000000,
    AllButtons       = 0x07ffffff,
    MaxMouseButton   = ExtraButton24,
    // 4 high-order bits remain available for future use (0x08000000 through 0x40000000).
    MouseButtonMask  = 0xffffffff
};
PK_DECLARE_FLAGS(MouseButtons, MouseButton)
PK_DECLARE_OPERATORS_FOR_FLAGS(MouseButtons)

// ── qnamespace.h:171-174 ─────────────────────────────────────────────────────
// 探针：Hor=1 Ver=2
enum Orientation {
    Horizontal = 0x1,
    Vertical = 0x2
};
PK_DECLARE_FLAGS(Orientations, Orientation)
PK_DECLARE_OPERATORS_FOR_FLAGS(Orientations)

// ── qnamespace.h:179-185 ─────────────────────────────────────────────────────
// 探针：NoFocus=0 TabFocus=1 ClickFocus=2 StrongFocus=11 WheelFocus=15
enum FocusPolicy {
    NoFocus = 0,
    TabFocus = 0x1,
    ClickFocus = 0x2,
    StrongFocus = TabFocus | ClickFocus | 0x8,
    WheelFocus = StrongFocus | 0x4
};

// ── qnamespace.h:194-197 ─────────────────────────────────────────────────────
enum SortOrder {
    AscendingOrder,
    DescendingOrder
};

// ── qnamespace.h:199-204 ─────────────────────────────────────────────────────
// SkipEmptyParts 保留范围 40 处。
enum SplitBehaviorFlags {
    KeepEmptyParts = 0,
    SkipEmptyParts = 0x1,
};
PK_DECLARE_FLAGS(SplitBehavior, SplitBehaviorFlags)
PK_DECLARE_OPERATORS_FOR_FLAGS(SplitBehavior)

// ── qnamespace.h:216-237 ─────────────────────────────────────────────────────
// 探针：Left=1 Right=2 HCenter=4 Top=32 Bottom=64 VCenter=128 Center=132。
// 只实现保留范围有用量的成员（AlignLeading/Trailing/Justify/Absolute/Baseline/
// 两个 Mask 各 0 处，登记缺口）。AlignCenter = VCenter|HCenter = 0x84。
enum AlignmentFlag {
    AlignLeft = 0x0001,
    AlignRight = 0x0002,
    AlignHCenter = 0x0004,
    AlignTop = 0x0020,
    AlignBottom = 0x0040,
    AlignVCenter = 0x0080,
    AlignCenter = AlignVCenter | AlignHCenter
};
PK_DECLARE_FLAGS(Alignment, AlignmentFlag)
PK_DECLARE_OPERATORS_FOR_FLAGS(Alignment)

// ── qnamespace.h:242-262 ─────────────────────────────────────────────────────
// TextWordWrap 保留范围 10 处（QPainter::drawText 的 flags，常与 Alignment 组合）。
enum TextFlag {
    TextSingleLine = 0x0100,
    TextDontClip = 0x0200,
    TextExpandTabs = 0x0400,
    TextShowMnemonic = 0x0800,
    TextWordWrap = 0x1000,
    TextWrapAnywhere = 0x2000,
    TextDontPrint = 0x4000,
    TextHideMnemonic = 0x8000,
    TextJustificationForced = 0x10000,
    TextForceLeftToRight = 0x20000,
    TextForceRightToLeft = 0x40000,
    TextLongestVariant = 0x80000,
    TextIncludeTrailingSpaces = 0x08000000
};

// ── qnamespace.h:569-595 ─────────────────────────────────────────────────────
// ImageConversionFlag：保留范围 AutoColor/ColorOnly/PreferDither 各 1 处
// （QImage::convertToFormat 的 flags）。值照抄 qnamespace.h（AutoColor=0、
// ColorOnly=0x3、PreferDither=0x40）。
enum ImageConversionFlag {
    ColorMode_Mask          = 0x00000003,
    AutoColor               = 0x00000000,
    ColorOnly               = 0x00000003,
    MonoOnly                = 0x00000002,

    AlphaDither_Mask        = 0x0000000c,
    ThresholdAlphaDither    = 0x00000000,
    OrderedAlphaDither      = 0x00000004,
    DiffuseAlphaDither      = 0x00000008,
    NoAlpha                 = 0x0000000c, // Not supported

    Dither_Mask             = 0x00000030,
    DiffuseDither           = 0x00000000,
    OrderedDither           = 0x00000010,
    ThresholdDither         = 0x00000020,

    DitherMode_Mask         = 0x000000c0,
    AutoDither              = 0x00000000,
    PreferDither            = 0x00000040,
    AvoidDither             = 0x00000080,

    NoOpaqueDetection       = 0x00000100,
    NoFormatConversion      = 0x00000200
};
PK_DECLARE_FLAGS(ImageConversionFlags, ImageConversionFlag)
PK_DECLARE_OPERATORS_FOR_FLAGS(ImageConversionFlags)

// ── qnamespace.h:604-1127 的裁剪子集（Key）─────────────────────────────────
// 真 Qt 全量 500+ 值；本头只实现保留范围用量 > 0 的（判据①），其余登记。
// 特殊键 = 0x01000000 基址 + 键码；可打印键 = ASCII/Unicode 码点（Key_Space=0x20，
// '['=0x5b ']'=0x5d）。键码表逐字照抄 qnamespace.h:604-744。
enum Key {
    Key_Escape = 0x01000000,                // misc keys
    Key_Backspace = 0x01000003,
    Key_Return = 0x01000004,
    Key_Enter = 0x01000005,
    Key_Delete = 0x01000007,
    Key_Left = 0x01000012,                  // cursor movement
    Key_Up = 0x01000013,
    Key_Right = 0x01000014,
    Key_Down = 0x01000015,
    Key_Shift = 0x01000020,                 // modifiers
    Key_Control = 0x01000021,
    Key_Meta = 0x01000022,
    Key_Alt = 0x01000023,
    Key_Space = 0x20,                       // 7 bit printable ASCII
    Key_A = 0x41,
    Key_B = 0x42,
    Key_F = 0x46,
    Key_G = 0x47,
    Key_I = 0x49,
    Key_J = 0x4a,
    Key_L = 0x4c,
    Key_M = 0x4d,
    Key_P = 0x50,
    Key_Q = 0x51,
    Key_R = 0x52,
    Key_T = 0x54,
    Key_U = 0x55,
    Key_BracketLeft = 0x5b,
    Key_BracketRight = 0x5d
};

// ── qnamespace.h:1135-1146 ───────────────────────────────────────────────────
// 探针：NoPen=0 SolidLine=1 DashLine=2 DotLine=3 DashDotLine=4 DashDotDotLine=5
// CustomDashLine=6
enum PenStyle { // pen style
    NoPen,
    SolidLine,
    DashLine,
    DotLine,
    DashDotLine,
    DashDotDotLine,
    CustomDashLine
};

// ── qnamespace.h:1148-1153 ───────────────────────────────────────────────────
// 探针：Flat=0 Square=16 Round=32
enum PenCapStyle { // line endcap style
    FlatCap = 0x00,
    SquareCap = 0x10,
    RoundCap = 0x20
};

// ── qnamespace.h:1155-1161 ───────────────────────────────────────────────────
// 探针：Miter=0 Bevel=64 Round=128 SvgMiter=256
enum PenJoinStyle { // line join style
    MiterJoin = 0x00,
    BevelJoin = 0x40,
    RoundJoin = 0x80,
    SvgMiterJoin = 0x100
};

// ── qnamespace.h:1163-1183 ───────────────────────────────────────────────────
// 探针：NoBrush=0 SolidPattern=1 LinearGradientPattern=15。
// ⚠ 相对 brief 示例代码的一处修正：真 Qt 5.15.7 的 TexturePattern = **24**（不是
// 18）——LinearGradientPattern=15 之后留出 18-23 的空档。oracle 的 static_assert
// 钉住这一位。
enum BrushStyle { // brush style
    NoBrush,
    SolidPattern,
    Dense1Pattern,
    Dense2Pattern,
    Dense3Pattern,
    Dense4Pattern,
    Dense5Pattern,
    Dense6Pattern,
    Dense7Pattern,
    HorPattern,
    VerPattern,
    CrossPattern,
    BDiagPattern,
    FDiagPattern,
    DiagCrossPattern,
    LinearGradientPattern,
    RadialGradientPattern,
    ConicalGradientPattern,
    TexturePattern = 24
};

// ── qnamespace.h:1200-1226 ───────────────────────────────────────────────────
// 探针：Arrow=0 Wait=3 Cross=2 SizeVer=5 SizeHor=6 SizeAll=9 PointingHand=13
// Forbidden=14 Busy=16 Blank=10。BitmapCursor/CustomCursor 保留范围 0 处，登记缺口。
enum CursorShape {
    ArrowCursor,
    UpArrowCursor,
    CrossCursor,
    WaitCursor,
    IBeamCursor,
    SizeVerCursor,
    SizeHorCursor,
    SizeBDiagCursor,
    SizeFDiagCursor,
    SizeAllCursor,
    BlankCursor,
    SplitVCursor,
    SplitHCursor,
    PointingHandCursor,
    ForbiddenCursor,
    WhatsThisCursor,
    BusyCursor,
    OpenHandCursor,
    ClosedHandCursor,
    DragCopyCursor,
    DragMoveCursor,
    DragLinkCursor,
    LastCursor = DragLinkCursor
};

// ── qnamespace.h:1228-1233 ───────────────────────────────────────────────────
// 探针：Plain=0 Rich=1 Auto=2。MarkdownText（=3）保留范围 0 处，登记缺口。
enum TextFormat {
    PlainText,
    RichText,
    AutoText
};

// ── qnamespace.h:1276-1290 ───────────────────────────────────────────────────
// 探针：TextDate=0 ISODate=1 ISODateWithMs=9 RFC=8。deprecated 的
// SystemLocaleDate/LocalDate 等（2-7 号位）保留范围 0 处，不抄。
enum DateFormat {
    TextDate,      // default Qt
    ISODate,       // ISO 8601
    RFC2822Date = 8,
    ISODateWithMs = 9
};

// ── qnamespace.h:1292-1297 ───────────────────────────────────────────────────
// 探针：Local=0 UTC=1 Offset=2。TimeZone（=3）保留范围 0 处，登记缺口。
enum TimeSpec {
    LocalTime,
    UTC,
    OffsetFromUTC
};

// ── qnamespace.h:1309-1313 ───────────────────────────────────────────────────
enum ScrollBarPolicy {
    ScrollBarAsNeeded,
    ScrollBarAlwaysOff,
    ScrollBarAlwaysOn
};

// ── qnamespace.h:1315-1318 ───────────────────────────────────────────────────
// 探针：Insensitive=0 Sensitive=1
enum CaseSensitivity {
    CaseInsensitive,
    CaseSensitive
};

// ── qnamespace.h:1337-1343 ───────────────────────────────────────────────────
// 探针：Auto=0 Direct=1 Queued=2 BlockingQueued=3 Unique=128
enum ConnectionType {
    AutoConnection,
    DirectConnection,
    QueuedConnection,
    BlockingQueuedConnection,
    UniqueConnection =  0x80
};

// ── qnamespace.h:1362-1366 ───────────────────────────────────────────────────
// IntersectClip 保留范围 5 处、NoClip 1 处（QPainter::setClip* 的 ClipOperation）。
enum ClipOperation {
    NoClip,
    ReplaceClip,
    IntersectClip
};

// ── qnamespace.h:1507-1511 ───────────────────────────────────────────────────
// 探针：LTR=0 RTL=1 Auto=2
enum LayoutDirection {
    LeftToRight,
    RightToLeft,
    LayoutDirectionAuto
};

// ── qnamespace.h:1539-1543 ───────────────────────────────────────────────────
// 探针：Unchecked=0 Partially=1 Checked=2。Krita 用 `Qt::CheckState::Unchecked`
// 这种限定语法（libs/global/KisMessageBoxWrapper.cpp:27）——本枚举是不带作用域
// 的 plain enum，C++17 允许 enum 名限定访问，照抄 Qt 形态即可。
enum CheckState {
    Unchecked,
    PartiallyChecked,
    Checked
};

// ── qnamespace.h:1545-1576 ───────────────────────────────────────────────────
// ItemDataRole：UserRole 保留范围 213 处、DisplayRole 30、EditRole 6、ToolTipRole 6、
// WhatsThisRole 4、StatusTipRole 4、FontRole 3、DecorationRole 3、CheckStateRole 10。
// 只实现以上 + TextAlignmentRole/BackgroundRole/ForegroundRole（同序列，常量本身
// 0 处但保持序列连续）；其余（Accessible*/SizeHintRole 等）0 处，登记缺口。
enum ItemDataRole {
    DisplayRole = 0,
    DecorationRole = 1,
    EditRole = 2,
    ToolTipRole = 3,
    StatusTipRole = 4,
    WhatsThisRole = 5,
    // Metadata
    FontRole = 6,
    TextAlignmentRole = 7,
    BackgroundRole = 8,
    ForegroundRole = 9,
    CheckStateRole = 10,
    // Reserved
    UserRole = 0x0100
};

// ── qnamespace.h:1578-1592 ───────────────────────────────────────────────────
// ItemFlag：ItemIsSelectable/ItemIsEditable/ItemNeverHasChildren/NoItemFlags/
// ItemIsUserCheckable/ItemIsEnabled 保留范围各 1-3 处；ItemFlags 复数类型 8 处。
enum ItemFlag {
    NoItemFlags = 0,
    ItemIsSelectable = 1,
    ItemIsEditable = 2,
    ItemIsDragEnabled = 4,
    ItemIsDropEnabled = 8,
    ItemIsUserCheckable = 16,
    ItemIsEnabled = 32,
    ItemIsAutoTristate = 64,
    ItemNeverHasChildren = 128,
    ItemIsUserTristate = 256
};
PK_DECLARE_FLAGS(ItemFlags, ItemFlag)
PK_DECLARE_OPERATORS_FOR_FLAGS(ItemFlags)

// ── qnamespace.h:1750-1754 ───────────────────────────────────────────────────
// CoarseTimer 保留范围 2 处（QTimer::singleShot 的重载实参）；PreciseTimer 仅在
// libs/global/tests 出现（测试口径，仍照抄）。
enum TimerType {
    PreciseTimer,
    CoarseTimer,
    VeryCoarseTimer
};

} // namespace Qt
