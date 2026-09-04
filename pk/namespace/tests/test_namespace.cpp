#include "cases/namespace_case.h"

// PkTestBinder<PkNamespaceCase> 由 pk_test_moc.py 生成（CMake 的 pk_test_generate
// 触发构建），像 Qt moc 输出一样直接 #include 进本 TU——显式特化必须在
// qExec<PkNamespaceCase> 实例化前对本 TU 可见。先例：
// pk/global/tests/test_global.cpp。
#include "pk_binder_namespace_case.inc"

// ---------------------------------------------------------------------------
// 所有期望值都取自**真 Qt 5.15.7** qnamespace.h 的探针输出（本任务 probe：
// .superpowers/sdd/R-27/probe_qnamespace.out）。对齐口径：与 Qt 的任何位值差异
// 默认都是缺陷。oracle（oracle/difftest_namespace.cpp）把本头包进 pkoracle 与真
// Qt 逐值 static_assert，这里是纯替代品一侧的行为核对（不链接真 Qt）。
// ---------------------------------------------------------------------------

void PkNamespaceCase::keyboardModifierValues()
{
    PK_COMPARE(int(Pk::NoModifier), 0x00000000);
    PK_COMPARE(int(Pk::ShiftModifier), 0x02000000);
    PK_COMPARE(int(Pk::ControlModifier), 0x04000000);
    PK_COMPARE(int(Pk::AltModifier), 0x08000000);
    PK_COMPARE(int(Pk::MetaModifier), 0x10000000);
    PK_COMPARE(int(Pk::KeypadModifier), 0x20000000);
    PK_COMPARE(int(Pk::GroupSwitchModifier), 0x40000000);
    PK_COMPARE(int(Pk::KeyboardModifierMask), 0xfe000000);
}

void PkNamespaceCase::keyboardModifiersFlags()
{
    // PK_DECLARE_FLAGS 给出的复数类型：QFlags 语义（对齐 pk/flags 测试）。
    Pk::KeyboardModifiers mods = Pk::ControlModifier | Pk::ShiftModifier;
    PK_COMPARE(int(mods), 0x06000000);
    PK_VERIFY(mods.testFlag(Pk::ControlModifier));
    PK_VERIFY(mods.testFlag(Pk::ShiftModifier));
    PK_VERIFY(!mods.testFlag(Pk::AltModifier));
    mods.setFlag(Pk::AltModifier);
    PK_COMPARE(int(mods), 0x0e000000);
    mods.setFlag(Pk::ShiftModifier, false);
    PK_COMPARE(int(mods), 0x0c000000);
    // operator&(int) 掩码
    PK_COMPARE(int(mods & 0xfe000000), 0x0c000000);
    PK_COMPARE(int(mods & 0x01000000), 0);
}

void PkNamespaceCase::modifierShortNames()
{
    PK_COMPARE(int(Pk::META), 0x10000000);
    PK_COMPARE(int(Pk::SHIFT), 0x02000000);
    PK_COMPARE(int(Pk::CTRL), 0x04000000);
    PK_COMPARE(int(Pk::ALT), 0x08000000);
    PK_COMPARE(int(Pk::MODIFIER_MASK), 0xfe000000);
    PK_COMPARE(int(Pk::UNICODE_ACCEL), 0x00000000);
}

void PkNamespaceCase::mouseButtonValues()
{
    PK_COMPARE(int(Pk::NoButton), 0x00000000);
    PK_COMPARE(int(Pk::LeftButton), 0x00000001);
    PK_COMPARE(int(Pk::RightButton), 0x00000002);
    PK_COMPARE(int(Pk::MiddleButton), 0x00000004);
    PK_COMPARE(int(Pk::BackButton), 0x00000008);
    PK_COMPARE(int(Pk::XButton1), 0x00000008);
    PK_COMPARE(int(Pk::ExtraButton1), 0x00000008);
    PK_COMPARE(int(Pk::ForwardButton), 0x00000010);
    PK_COMPARE(int(Pk::XButton2), 0x00000010);
    PK_COMPARE(int(Pk::ExtraButton2), 0x00000010);
    PK_COMPARE(int(Pk::TaskButton), 0x00000020);
    PK_COMPARE(int(Pk::ExtraButton3), 0x00000020);
    PK_COMPARE(int(Pk::ExtraButton4), 0x00000040);
    PK_COMPARE(int(Pk::ExtraButton24), 0x04000000);
    // ⚠ 两处相对 brief 示例的修正（真 Qt 实测）：MaxMouseButton = ExtraButton24
    // （不是 TaskButton）；kis_stroke_shortcut.cpp:36 的 log2((int)MaxMouseButton)
    // 依赖这一位。
    PK_COMPARE(int(Pk::MaxMouseButton), 0x04000000);
    PK_COMPARE(int(Pk::AllButtons), 0x07ffffff);
    PK_COMPARE(int(Pk::MouseButtonMask), 0xffffffff);
}

void PkNamespaceCase::mouseButtonsFlags()
{
    Pk::MouseButtons btns = Pk::LeftButton | Pk::RightButton | Pk::MiddleButton;
    PK_COMPARE(int(btns), 0x00000007);
    PK_VERIFY(btns.testFlag(Pk::LeftButton));
    PK_VERIFY(btns.testFlag(Pk::RightButton));
    PK_VERIFY(!btns.testFlag(Pk::BackButton));
    PK_COMPARE(int(btns & Pk::RightButton), 0x2);
}

void PkNamespaceCase::orientationValues()
{
    PK_COMPARE(int(Pk::Horizontal), 0x1);
    PK_COMPARE(int(Pk::Vertical), 0x2);
    Pk::Orientations o = Pk::Horizontal | Pk::Vertical;
    PK_COMPARE(int(o), 0x3);
    PK_VERIFY(o.testFlag(Pk::Vertical));
}

void PkNamespaceCase::focusPolicyValues()
{
    PK_COMPARE(int(Pk::NoFocus), 0);
    PK_COMPARE(int(Pk::TabFocus), 0x1);
    PK_COMPARE(int(Pk::ClickFocus), 0x2);
    PK_COMPARE(int(Pk::StrongFocus), 0xb);   // TabFocus|ClickFocus|0x8 = 11
    PK_COMPARE(int(Pk::WheelFocus), 0xf);    // StrongFocus|0x4 = 15
}

void PkNamespaceCase::sortOrderValues()
{
    PK_COMPARE(int(Pk::AscendingOrder), 0);
    PK_COMPARE(int(Pk::DescendingOrder), 1);
}

void PkNamespaceCase::splitBehaviorValues()
{
    PK_COMPARE(int(Pk::KeepEmptyParts), 0);
    PK_COMPARE(int(Pk::SkipEmptyParts), 0x1);
    Pk::SplitBehavior s = Pk::SkipEmptyParts;
    PK_COMPARE(int(s), 0x1);
}

void PkNamespaceCase::alignmentValues()
{
    PK_COMPARE(int(Pk::AlignLeft), 0x0001);
    PK_COMPARE(int(Pk::AlignRight), 0x0002);
    PK_COMPARE(int(Pk::AlignHCenter), 0x0004);
    PK_COMPARE(int(Pk::AlignTop), 0x0020);
    PK_COMPARE(int(Pk::AlignBottom), 0x0040);
    PK_COMPARE(int(Pk::AlignVCenter), 0x0080);
    PK_COMPARE(int(Pk::AlignCenter), 0x0084);  // AlignVCenter|AlignHCenter
    Pk::Alignment a = Pk::AlignLeft | Pk::AlignVCenter;
    PK_COMPARE(int(a), 0x0081);
    PK_VERIFY(a.testFlag(Pk::AlignLeft));
    PK_VERIFY(a.testFlag(Pk::AlignVCenter));
    PK_VERIFY(!a.testFlag(Pk::AlignRight));
}

void PkNamespaceCase::textFlagValues()
{
    PK_COMPARE(int(Pk::TextSingleLine), 0x0100);
    PK_COMPARE(int(Pk::TextDontClip), 0x0200);
    PK_COMPARE(int(Pk::TextExpandTabs), 0x0400);
    PK_COMPARE(int(Pk::TextShowMnemonic), 0x0800);
    PK_COMPARE(int(Pk::TextWordWrap), 0x1000);
    PK_COMPARE(int(Pk::TextWrapAnywhere), 0x2000);
    PK_COMPARE(int(Pk::TextDontPrint), 0x4000);
    PK_COMPARE(int(Pk::TextHideMnemonic), 0x8000);
    PK_COMPARE(int(Pk::TextJustificationForced), 0x10000);
    PK_COMPARE(int(Pk::TextForceLeftToRight), 0x20000);
    PK_COMPARE(int(Pk::TextForceRightToLeft), 0x40000);
    PK_COMPARE(int(Pk::TextLongestVariant), 0x80000);
    PK_COMPARE(int(Pk::TextIncludeTrailingSpaces), 0x08000000);
}

void PkNamespaceCase::imageConversionFlagValues()
{
    PK_COMPARE(int(Pk::ColorMode_Mask), 0x00000003);
    PK_COMPARE(int(Pk::AutoColor), 0x00000000);
    PK_COMPARE(int(Pk::ColorOnly), 0x00000003);
    PK_COMPARE(int(Pk::MonoOnly), 0x00000002);
    PK_COMPARE(int(Pk::AlphaDither_Mask), 0x0000000c);
    PK_COMPARE(int(Pk::ThresholdAlphaDither), 0x00000000);
    PK_COMPARE(int(Pk::OrderedAlphaDither), 0x00000004);
    PK_COMPARE(int(Pk::DiffuseAlphaDither), 0x00000008);
    PK_COMPARE(int(Pk::NoAlpha), 0x0000000c);
    PK_COMPARE(int(Pk::Dither_Mask), 0x00000030);
    PK_COMPARE(int(Pk::DiffuseDither), 0x00000000);
    PK_COMPARE(int(Pk::OrderedDither), 0x00000010);
    PK_COMPARE(int(Pk::ThresholdDither), 0x00000020);
    PK_COMPARE(int(Pk::DitherMode_Mask), 0x000000c0);
    PK_COMPARE(int(Pk::AutoDither), 0x00000000);
    PK_COMPARE(int(Pk::PreferDither), 0x00000040);
    PK_COMPARE(int(Pk::AvoidDither), 0x00000080);
    PK_COMPARE(int(Pk::NoOpaqueDetection), 0x00000100);
    PK_COMPARE(int(Pk::NoFormatConversion), 0x00000200);
    Pk::ImageConversionFlags f = Pk::ColorOnly | Pk::PreferDither;
    PK_COMPARE(int(f), 0x00000043);
    PK_VERIFY(f.testFlag(Pk::ColorOnly));
    PK_VERIFY(f.testFlag(Pk::PreferDither));
}

void PkNamespaceCase::keyValues()
{
    // 特殊键 = 0x01000000 基址 + 键码（照抄 qnamespace.h:604-744）。
    PK_COMPARE(int(Pk::Key_Escape), 0x01000000);
    PK_COMPARE(int(Pk::Key_Backspace), 0x01000003);
    PK_COMPARE(int(Pk::Key_Return), 0x01000004);
    PK_COMPARE(int(Pk::Key_Enter), 0x01000005);
    PK_COMPARE(int(Pk::Key_Delete), 0x01000007);
    PK_COMPARE(int(Pk::Key_Left), 0x01000012);
    PK_COMPARE(int(Pk::Key_Up), 0x01000013);
    PK_COMPARE(int(Pk::Key_Right), 0x01000014);
    PK_COMPARE(int(Pk::Key_Down), 0x01000015);
    PK_COMPARE(int(Pk::Key_Shift), 0x01000020);
    PK_COMPARE(int(Pk::Key_Control), 0x01000021);
    PK_COMPARE(int(Pk::Key_Meta), 0x01000022);
    PK_COMPARE(int(Pk::Key_Alt), 0x01000023);
    // 可打印键 = ASCII/Unicode 码点。
    PK_COMPARE(int(Pk::Key_Space), 0x20);
    PK_COMPARE(int(Pk::Key_A), 0x41);
    PK_COMPARE(int(Pk::Key_B), 0x42);
    PK_COMPARE(int(Pk::Key_F), 0x46);
    PK_COMPARE(int(Pk::Key_G), 0x47);
    PK_COMPARE(int(Pk::Key_I), 0x49);
    PK_COMPARE(int(Pk::Key_J), 0x4a);
    PK_COMPARE(int(Pk::Key_L), 0x4c);
    PK_COMPARE(int(Pk::Key_M), 0x4d);
    PK_COMPARE(int(Pk::Key_P), 0x50);
    PK_COMPARE(int(Pk::Key_Q), 0x51);
    PK_COMPARE(int(Pk::Key_R), 0x52);
    PK_COMPARE(int(Pk::Key_T), 0x54);
    PK_COMPARE(int(Pk::Key_U), 0x55);
    PK_COMPARE(int(Pk::Key_BracketLeft), 0x5b);
    PK_COMPARE(int(Pk::Key_BracketRight), 0x5d);
}

void PkNamespaceCase::penValues()
{
    PK_COMPARE(int(Pk::NoPen), 0);
    PK_COMPARE(int(Pk::SolidLine), 1);
    PK_COMPARE(int(Pk::DashLine), 2);
    PK_COMPARE(int(Pk::DotLine), 3);
    PK_COMPARE(int(Pk::DashDotLine), 4);
    PK_COMPARE(int(Pk::DashDotDotLine), 5);
    PK_COMPARE(int(Pk::CustomDashLine), 6);
    // PenCapStyle
    PK_COMPARE(int(Pk::FlatCap), 0x00);
    PK_COMPARE(int(Pk::SquareCap), 0x10);
    PK_COMPARE(int(Pk::RoundCap), 0x20);
    // PenJoinStyle
    PK_COMPARE(int(Pk::MiterJoin), 0x00);
    PK_COMPARE(int(Pk::BevelJoin), 0x40);
    PK_COMPARE(int(Pk::RoundJoin), 0x80);
    PK_COMPARE(int(Pk::SvgMiterJoin), 0x100);
}

void PkNamespaceCase::brushStyleValues()
{
    PK_COMPARE(int(Pk::NoBrush), 0);
    PK_COMPARE(int(Pk::SolidPattern), 1);
    PK_COMPARE(int(Pk::Dense1Pattern), 2);
    PK_COMPARE(int(Pk::Dense7Pattern), 8);
    PK_COMPARE(int(Pk::HorPattern), 9);
    PK_COMPARE(int(Pk::DiagCrossPattern), 14);
    PK_COMPARE(int(Pk::LinearGradientPattern), 15);
    PK_COMPARE(int(Pk::RadialGradientPattern), 16);
    PK_COMPARE(int(Pk::ConicalGradientPattern), 17);
    // ⚠ 修正：TexturePattern = 24（LinearGradientPattern=15 后留出 18-23 空档）。
    PK_COMPARE(int(Pk::TexturePattern), 24);
}

void PkNamespaceCase::cursorShapeValues()
{
    PK_COMPARE(int(Pk::ArrowCursor), 0);
    PK_COMPARE(int(Pk::UpArrowCursor), 1);
    PK_COMPARE(int(Pk::CrossCursor), 2);
    PK_COMPARE(int(Pk::WaitCursor), 3);
    PK_COMPARE(int(Pk::IBeamCursor), 4);
    PK_COMPARE(int(Pk::SizeVerCursor), 5);
    PK_COMPARE(int(Pk::SizeHorCursor), 6);
    PK_COMPARE(int(Pk::SizeBDiagCursor), 7);
    PK_COMPARE(int(Pk::SizeFDiagCursor), 8);
    PK_COMPARE(int(Pk::SizeAllCursor), 9);
    PK_COMPARE(int(Pk::BlankCursor), 10);
    PK_COMPARE(int(Pk::SplitVCursor), 11);
    PK_COMPARE(int(Pk::SplitHCursor), 12);
    PK_COMPARE(int(Pk::PointingHandCursor), 13);
    PK_COMPARE(int(Pk::ForbiddenCursor), 14);
    PK_COMPARE(int(Pk::WhatsThisCursor), 15);
    PK_COMPARE(int(Pk::BusyCursor), 16);
    PK_COMPARE(int(Pk::OpenHandCursor), 17);
    PK_COMPARE(int(Pk::ClosedHandCursor), 18);
    PK_COMPARE(int(Pk::DragCopyCursor), 19);
    PK_COMPARE(int(Pk::DragMoveCursor), 20);
    PK_COMPARE(int(Pk::DragLinkCursor), 21);
    PK_COMPARE(int(Pk::LastCursor), 21);
}

void PkNamespaceCase::textFormatValues()
{
    PK_COMPARE(int(Pk::PlainText), 0);
    PK_COMPARE(int(Pk::RichText), 1);
    PK_COMPARE(int(Pk::AutoText), 2);
}

void PkNamespaceCase::timeSpecValues()
{
    PK_COMPARE(int(Pk::LocalTime), 0);
    PK_COMPARE(int(Pk::UTC), 1);
    PK_COMPARE(int(Pk::OffsetFromUTC), 2);
}

void PkNamespaceCase::scrollBarPolicyValues()
{
    PK_COMPARE(int(Pk::ScrollBarAsNeeded), 0);
    PK_COMPARE(int(Pk::ScrollBarAlwaysOff), 1);
    PK_COMPARE(int(Pk::ScrollBarAlwaysOn), 2);
}

void PkNamespaceCase::caseSensitivityValues()
{
    PK_COMPARE(int(Pk::CaseInsensitive), 0);
    PK_COMPARE(int(Pk::CaseSensitive), 1);
}

void PkNamespaceCase::fillRuleValues()
{
    PK_COMPARE(int(Pk::OddEvenFill), 0);
    PK_COMPARE(int(Pk::WindingFill), 1);
}

void PkNamespaceCase::clipOperationValues()
{
    PK_COMPARE(int(Pk::NoClip), 0);
    PK_COMPARE(int(Pk::ReplaceClip), 1);
    PK_COMPARE(int(Pk::IntersectClip), 2);
}

void PkNamespaceCase::transformationModeValues()
{
    PK_COMPARE(int(Pk::FastTransformation), 0);
    PK_COMPARE(int(Pk::SmoothTransformation), 1);
}

void PkNamespaceCase::layoutDirectionValues()
{
    PK_COMPARE(int(Pk::LeftToRight), 0);
    PK_COMPARE(int(Pk::RightToLeft), 1);
    PK_COMPARE(int(Pk::LayoutDirectionAuto), 2);
}

void PkNamespaceCase::checkStateValues()
{
    // Krita 用 `Pk::CheckState::Unchecked` 这种限定语法（plain enum 名限定，
    // C++17 允许）——照抄 Qt 形态。
    PK_COMPARE(int(Pk::CheckState::Unchecked), 0);
    PK_COMPARE(int(Pk::CheckState::PartiallyChecked), 1);
    PK_COMPARE(int(Pk::CheckState::Checked), 2);
    PK_COMPARE(int(Pk::Unchecked), 0);
}

void PkNamespaceCase::itemDataRoleValues()
{
    PK_COMPARE(int(Pk::DisplayRole), 0);
    PK_COMPARE(int(Pk::DecorationRole), 1);
    PK_COMPARE(int(Pk::EditRole), 2);
    PK_COMPARE(int(Pk::ToolTipRole), 3);
    PK_COMPARE(int(Pk::StatusTipRole), 4);
    PK_COMPARE(int(Pk::WhatsThisRole), 5);
    PK_COMPARE(int(Pk::FontRole), 6);
    PK_COMPARE(int(Pk::TextAlignmentRole), 7);
    PK_COMPARE(int(Pk::BackgroundRole), 8);
    PK_COMPARE(int(Pk::ForegroundRole), 9);
    PK_COMPARE(int(Pk::CheckStateRole), 10);
    PK_COMPARE(int(Pk::UserRole), 0x0100);
}

void PkNamespaceCase::itemFlagsValues()
{
    PK_COMPARE(int(Pk::NoItemFlags), 0);
    PK_COMPARE(int(Pk::ItemIsSelectable), 1);
    PK_COMPARE(int(Pk::ItemIsEditable), 2);
    PK_COMPARE(int(Pk::ItemIsDragEnabled), 4);
    PK_COMPARE(int(Pk::ItemIsDropEnabled), 8);
    PK_COMPARE(int(Pk::ItemIsUserCheckable), 16);
    PK_COMPARE(int(Pk::ItemIsEnabled), 32);
    PK_COMPARE(int(Pk::ItemIsAutoTristate), 64);
    PK_COMPARE(int(Pk::ItemNeverHasChildren), 128);
    PK_COMPARE(int(Pk::ItemIsUserTristate), 256);
    Pk::ItemFlags f = Pk::ItemIsSelectable | Pk::ItemIsEnabled;
    PK_COMPARE(int(f), 33);
    PK_VERIFY(f.testFlag(Pk::ItemIsSelectable));
    PK_VERIFY(f.testFlag(Pk::ItemIsEnabled));
}

void PkNamespaceCase::timerTypeValues()
{
    PK_COMPARE(int(Pk::PreciseTimer), 0);
    PK_COMPARE(int(Pk::CoarseTimer), 1);
    PK_COMPARE(int(Pk::VeryCoarseTimer), 2);
}

void PkNamespaceCase::globalColorValues()
{
    // QColor 构造的 Pk::GlobalColor 实参依赖这些序号的数值。
    PK_COMPARE(int(Pk::color0), 0);
    PK_COMPARE(int(Pk::color1), 1);
    PK_COMPARE(int(Pk::black), 2);
    PK_COMPARE(int(Pk::white), 3);
    PK_COMPARE(int(Pk::darkGray), 4);
    PK_COMPARE(int(Pk::gray), 5);
    PK_COMPARE(int(Pk::lightGray), 6);
    PK_COMPARE(int(Pk::red), 7);
    PK_COMPARE(int(Pk::green), 8);
    PK_COMPARE(int(Pk::blue), 9);
    PK_COMPARE(int(Pk::cyan), 10);
    PK_COMPARE(int(Pk::magenta), 11);
    PK_COMPARE(int(Pk::yellow), 12);
    PK_COMPARE(int(Pk::darkRed), 13);
    PK_COMPARE(int(Pk::darkGreen), 14);
    PK_COMPARE(int(Pk::darkBlue), 15);
    PK_COMPARE(int(Pk::darkCyan), 16);
    PK_COMPARE(int(Pk::darkMagenta), 17);
    PK_COMPARE(int(Pk::darkYellow), 18);
    PK_COMPARE(int(Pk::transparent), 19);
}

void PkNamespaceCase::coexistWithGlobalEnums()
{
    // PkNamespace.h include 了 PkGlobal.h，两个 `namespace Pk` 块在同一个 TU 里
    // 并集可见（C++ 同名 namespace 多次打开，枚举名不重复）。这条探针钉住
    // 「AspectRatioMode（R-18 交付）与本头的 KeyboardModifier 同住一个 namespace
    // Qt 且各自取值正确」——重定义 AspectRatioMode 会在这里硬错。
    PK_COMPARE(int(Pk::AspectRatioMode::IgnoreAspectRatio), 0);
    PK_COMPARE(int(Pk::AspectRatioMode::KeepAspectRatio), 1);
    PK_COMPARE(int(Pk::AspectRatioMode::KeepAspectRatioByExpanding), 2);
    PK_COMPARE(int(Pk::KeyboardModifier::ShiftModifier), 0x02000000);
    PK_COMPARE(int(Pk::ControlModifier), 0x04000000);
}

void PkNamespaceCase::coexistAxisEnum()
{
    // PkGlobal.h 的 Axis 枚举（同 namespace Pk）。
    PK_COMPARE(int(Pk::Axis::XAxis), 0);
    PK_COMPARE(int(Pk::Axis::YAxis), 1);
    PK_COMPARE(int(Pk::Axis::ZAxis), 2);
}

int run_namespace_tests()
{
    PkNamespaceCase tc;
    const char *argv[] = {"test_pknamespace"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
