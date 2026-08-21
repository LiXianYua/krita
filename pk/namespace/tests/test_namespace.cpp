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
    PK_COMPARE(int(Qt::NoModifier), 0x00000000);
    PK_COMPARE(int(Qt::ShiftModifier), 0x02000000);
    PK_COMPARE(int(Qt::ControlModifier), 0x04000000);
    PK_COMPARE(int(Qt::AltModifier), 0x08000000);
    PK_COMPARE(int(Qt::MetaModifier), 0x10000000);
    PK_COMPARE(int(Qt::KeypadModifier), 0x20000000);
    PK_COMPARE(int(Qt::GroupSwitchModifier), 0x40000000);
    PK_COMPARE(int(Qt::KeyboardModifierMask), 0xfe000000);
}

void PkNamespaceCase::keyboardModifiersFlags()
{
    // PK_DECLARE_FLAGS 给出的复数类型：QFlags 语义（对齐 pk/flags 测试）。
    Qt::KeyboardModifiers mods = Qt::ControlModifier | Qt::ShiftModifier;
    PK_COMPARE(int(mods), 0x06000000);
    PK_VERIFY(mods.testFlag(Qt::ControlModifier));
    PK_VERIFY(mods.testFlag(Qt::ShiftModifier));
    PK_VERIFY(!mods.testFlag(Qt::AltModifier));
    mods.setFlag(Qt::AltModifier);
    PK_COMPARE(int(mods), 0x0e000000);
    mods.setFlag(Qt::ShiftModifier, false);
    PK_COMPARE(int(mods), 0x0c000000);
    // operator&(int) 掩码
    PK_COMPARE(int(mods & 0xfe000000), 0x0c000000);
    PK_COMPARE(int(mods & 0x01000000), 0);
}

void PkNamespaceCase::modifierShortNames()
{
    PK_COMPARE(int(Qt::META), 0x10000000);
    PK_COMPARE(int(Qt::SHIFT), 0x02000000);
    PK_COMPARE(int(Qt::CTRL), 0x04000000);
    PK_COMPARE(int(Qt::ALT), 0x08000000);
    PK_COMPARE(int(Qt::MODIFIER_MASK), 0xfe000000);
    PK_COMPARE(int(Qt::UNICODE_ACCEL), 0x00000000);
}

void PkNamespaceCase::mouseButtonValues()
{
    PK_COMPARE(int(Qt::NoButton), 0x00000000);
    PK_COMPARE(int(Qt::LeftButton), 0x00000001);
    PK_COMPARE(int(Qt::RightButton), 0x00000002);
    PK_COMPARE(int(Qt::MiddleButton), 0x00000004);
    PK_COMPARE(int(Qt::BackButton), 0x00000008);
    PK_COMPARE(int(Qt::XButton1), 0x00000008);
    PK_COMPARE(int(Qt::ExtraButton1), 0x00000008);
    PK_COMPARE(int(Qt::ForwardButton), 0x00000010);
    PK_COMPARE(int(Qt::XButton2), 0x00000010);
    PK_COMPARE(int(Qt::ExtraButton2), 0x00000010);
    PK_COMPARE(int(Qt::TaskButton), 0x00000020);
    PK_COMPARE(int(Qt::ExtraButton3), 0x00000020);
    PK_COMPARE(int(Qt::ExtraButton4), 0x00000040);
    PK_COMPARE(int(Qt::ExtraButton24), 0x04000000);
    // ⚠ 两处相对 brief 示例的修正（真 Qt 实测）：MaxMouseButton = ExtraButton24
    // （不是 TaskButton）；kis_stroke_shortcut.cpp:36 的 log2((int)MaxMouseButton)
    // 依赖这一位。
    PK_COMPARE(int(Qt::MaxMouseButton), 0x04000000);
    PK_COMPARE(int(Qt::AllButtons), 0x07ffffff);
    PK_COMPARE(int(Qt::MouseButtonMask), 0xffffffff);
}

void PkNamespaceCase::mouseButtonsFlags()
{
    Qt::MouseButtons btns = Qt::LeftButton | Qt::RightButton | Qt::MiddleButton;
    PK_COMPARE(int(btns), 0x00000007);
    PK_VERIFY(btns.testFlag(Qt::LeftButton));
    PK_VERIFY(btns.testFlag(Qt::RightButton));
    PK_VERIFY(!btns.testFlag(Qt::BackButton));
    PK_COMPARE(int(btns & Qt::RightButton), 0x2);
}

void PkNamespaceCase::orientationValues()
{
    PK_COMPARE(int(Qt::Horizontal), 0x1);
    PK_COMPARE(int(Qt::Vertical), 0x2);
    Qt::Orientations o = Qt::Horizontal | Qt::Vertical;
    PK_COMPARE(int(o), 0x3);
    PK_VERIFY(o.testFlag(Qt::Vertical));
}

void PkNamespaceCase::focusPolicyValues()
{
    PK_COMPARE(int(Qt::NoFocus), 0);
    PK_COMPARE(int(Qt::TabFocus), 0x1);
    PK_COMPARE(int(Qt::ClickFocus), 0x2);
    PK_COMPARE(int(Qt::StrongFocus), 0xb);   // TabFocus|ClickFocus|0x8 = 11
    PK_COMPARE(int(Qt::WheelFocus), 0xf);    // StrongFocus|0x4 = 15
}

void PkNamespaceCase::sortOrderValues()
{
    PK_COMPARE(int(Qt::AscendingOrder), 0);
    PK_COMPARE(int(Qt::DescendingOrder), 1);
}

void PkNamespaceCase::splitBehaviorValues()
{
    PK_COMPARE(int(Qt::KeepEmptyParts), 0);
    PK_COMPARE(int(Qt::SkipEmptyParts), 0x1);
    Qt::SplitBehavior s = Qt::SkipEmptyParts;
    PK_COMPARE(int(s), 0x1);
}

void PkNamespaceCase::alignmentValues()
{
    PK_COMPARE(int(Qt::AlignLeft), 0x0001);
    PK_COMPARE(int(Qt::AlignRight), 0x0002);
    PK_COMPARE(int(Qt::AlignHCenter), 0x0004);
    PK_COMPARE(int(Qt::AlignTop), 0x0020);
    PK_COMPARE(int(Qt::AlignBottom), 0x0040);
    PK_COMPARE(int(Qt::AlignVCenter), 0x0080);
    PK_COMPARE(int(Qt::AlignCenter), 0x0084);  // AlignVCenter|AlignHCenter
    Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter;
    PK_COMPARE(int(a), 0x0081);
    PK_VERIFY(a.testFlag(Qt::AlignLeft));
    PK_VERIFY(a.testFlag(Qt::AlignVCenter));
    PK_VERIFY(!a.testFlag(Qt::AlignRight));
}

void PkNamespaceCase::textFlagValues()
{
    PK_COMPARE(int(Qt::TextSingleLine), 0x0100);
    PK_COMPARE(int(Qt::TextDontClip), 0x0200);
    PK_COMPARE(int(Qt::TextExpandTabs), 0x0400);
    PK_COMPARE(int(Qt::TextShowMnemonic), 0x0800);
    PK_COMPARE(int(Qt::TextWordWrap), 0x1000);
    PK_COMPARE(int(Qt::TextWrapAnywhere), 0x2000);
    PK_COMPARE(int(Qt::TextDontPrint), 0x4000);
    PK_COMPARE(int(Qt::TextHideMnemonic), 0x8000);
    PK_COMPARE(int(Qt::TextJustificationForced), 0x10000);
    PK_COMPARE(int(Qt::TextForceLeftToRight), 0x20000);
    PK_COMPARE(int(Qt::TextForceRightToLeft), 0x40000);
    PK_COMPARE(int(Qt::TextLongestVariant), 0x80000);
    PK_COMPARE(int(Qt::TextIncludeTrailingSpaces), 0x08000000);
}

void PkNamespaceCase::imageConversionFlagValues()
{
    PK_COMPARE(int(Qt::ColorMode_Mask), 0x00000003);
    PK_COMPARE(int(Qt::AutoColor), 0x00000000);
    PK_COMPARE(int(Qt::ColorOnly), 0x00000003);
    PK_COMPARE(int(Qt::MonoOnly), 0x00000002);
    PK_COMPARE(int(Qt::AlphaDither_Mask), 0x0000000c);
    PK_COMPARE(int(Qt::ThresholdAlphaDither), 0x00000000);
    PK_COMPARE(int(Qt::OrderedAlphaDither), 0x00000004);
    PK_COMPARE(int(Qt::DiffuseAlphaDither), 0x00000008);
    PK_COMPARE(int(Qt::NoAlpha), 0x0000000c);
    PK_COMPARE(int(Qt::Dither_Mask), 0x00000030);
    PK_COMPARE(int(Qt::DiffuseDither), 0x00000000);
    PK_COMPARE(int(Qt::OrderedDither), 0x00000010);
    PK_COMPARE(int(Qt::ThresholdDither), 0x00000020);
    PK_COMPARE(int(Qt::DitherMode_Mask), 0x000000c0);
    PK_COMPARE(int(Qt::AutoDither), 0x00000000);
    PK_COMPARE(int(Qt::PreferDither), 0x00000040);
    PK_COMPARE(int(Qt::AvoidDither), 0x00000080);
    PK_COMPARE(int(Qt::NoOpaqueDetection), 0x00000100);
    PK_COMPARE(int(Qt::NoFormatConversion), 0x00000200);
    Qt::ImageConversionFlags f = Qt::ColorOnly | Qt::PreferDither;
    PK_COMPARE(int(f), 0x00000043);
    PK_VERIFY(f.testFlag(Qt::ColorOnly));
    PK_VERIFY(f.testFlag(Qt::PreferDither));
}

void PkNamespaceCase::keyValues()
{
    // 特殊键 = 0x01000000 基址 + 键码（照抄 qnamespace.h:604-744）。
    PK_COMPARE(int(Qt::Key_Escape), 0x01000000);
    PK_COMPARE(int(Qt::Key_Backspace), 0x01000003);
    PK_COMPARE(int(Qt::Key_Return), 0x01000004);
    PK_COMPARE(int(Qt::Key_Enter), 0x01000005);
    PK_COMPARE(int(Qt::Key_Delete), 0x01000007);
    PK_COMPARE(int(Qt::Key_Left), 0x01000012);
    PK_COMPARE(int(Qt::Key_Up), 0x01000013);
    PK_COMPARE(int(Qt::Key_Right), 0x01000014);
    PK_COMPARE(int(Qt::Key_Down), 0x01000015);
    PK_COMPARE(int(Qt::Key_Shift), 0x01000020);
    PK_COMPARE(int(Qt::Key_Control), 0x01000021);
    PK_COMPARE(int(Qt::Key_Meta), 0x01000022);
    PK_COMPARE(int(Qt::Key_Alt), 0x01000023);
    // 可打印键 = ASCII/Unicode 码点。
    PK_COMPARE(int(Qt::Key_Space), 0x20);
    PK_COMPARE(int(Qt::Key_A), 0x41);
    PK_COMPARE(int(Qt::Key_B), 0x42);
    PK_COMPARE(int(Qt::Key_F), 0x46);
    PK_COMPARE(int(Qt::Key_G), 0x47);
    PK_COMPARE(int(Qt::Key_I), 0x49);
    PK_COMPARE(int(Qt::Key_J), 0x4a);
    PK_COMPARE(int(Qt::Key_L), 0x4c);
    PK_COMPARE(int(Qt::Key_M), 0x4d);
    PK_COMPARE(int(Qt::Key_P), 0x50);
    PK_COMPARE(int(Qt::Key_Q), 0x51);
    PK_COMPARE(int(Qt::Key_R), 0x52);
    PK_COMPARE(int(Qt::Key_T), 0x54);
    PK_COMPARE(int(Qt::Key_U), 0x55);
    PK_COMPARE(int(Qt::Key_BracketLeft), 0x5b);
    PK_COMPARE(int(Qt::Key_BracketRight), 0x5d);
}

void PkNamespaceCase::penValues()
{
    PK_COMPARE(int(Qt::NoPen), 0);
    PK_COMPARE(int(Qt::SolidLine), 1);
    PK_COMPARE(int(Qt::DashLine), 2);
    PK_COMPARE(int(Qt::DotLine), 3);
    PK_COMPARE(int(Qt::DashDotLine), 4);
    PK_COMPARE(int(Qt::DashDotDotLine), 5);
    PK_COMPARE(int(Qt::CustomDashLine), 6);
    // PenCapStyle
    PK_COMPARE(int(Qt::FlatCap), 0x00);
    PK_COMPARE(int(Qt::SquareCap), 0x10);
    PK_COMPARE(int(Qt::RoundCap), 0x20);
    // PenJoinStyle
    PK_COMPARE(int(Qt::MiterJoin), 0x00);
    PK_COMPARE(int(Qt::BevelJoin), 0x40);
    PK_COMPARE(int(Qt::RoundJoin), 0x80);
    PK_COMPARE(int(Qt::SvgMiterJoin), 0x100);
}

void PkNamespaceCase::brushStyleValues()
{
    PK_COMPARE(int(Qt::NoBrush), 0);
    PK_COMPARE(int(Qt::SolidPattern), 1);
    PK_COMPARE(int(Qt::Dense1Pattern), 2);
    PK_COMPARE(int(Qt::Dense7Pattern), 8);
    PK_COMPARE(int(Qt::HorPattern), 9);
    PK_COMPARE(int(Qt::DiagCrossPattern), 14);
    PK_COMPARE(int(Qt::LinearGradientPattern), 15);
    PK_COMPARE(int(Qt::RadialGradientPattern), 16);
    PK_COMPARE(int(Qt::ConicalGradientPattern), 17);
    // ⚠ 修正：TexturePattern = 24（LinearGradientPattern=15 后留出 18-23 空档）。
    PK_COMPARE(int(Qt::TexturePattern), 24);
}

void PkNamespaceCase::cursorShapeValues()
{
    PK_COMPARE(int(Qt::ArrowCursor), 0);
    PK_COMPARE(int(Qt::UpArrowCursor), 1);
    PK_COMPARE(int(Qt::CrossCursor), 2);
    PK_COMPARE(int(Qt::WaitCursor), 3);
    PK_COMPARE(int(Qt::IBeamCursor), 4);
    PK_COMPARE(int(Qt::SizeVerCursor), 5);
    PK_COMPARE(int(Qt::SizeHorCursor), 6);
    PK_COMPARE(int(Qt::SizeBDiagCursor), 7);
    PK_COMPARE(int(Qt::SizeFDiagCursor), 8);
    PK_COMPARE(int(Qt::SizeAllCursor), 9);
    PK_COMPARE(int(Qt::BlankCursor), 10);
    PK_COMPARE(int(Qt::SplitVCursor), 11);
    PK_COMPARE(int(Qt::SplitHCursor), 12);
    PK_COMPARE(int(Qt::PointingHandCursor), 13);
    PK_COMPARE(int(Qt::ForbiddenCursor), 14);
    PK_COMPARE(int(Qt::WhatsThisCursor), 15);
    PK_COMPARE(int(Qt::BusyCursor), 16);
    PK_COMPARE(int(Qt::OpenHandCursor), 17);
    PK_COMPARE(int(Qt::ClosedHandCursor), 18);
    PK_COMPARE(int(Qt::DragCopyCursor), 19);
    PK_COMPARE(int(Qt::DragMoveCursor), 20);
    PK_COMPARE(int(Qt::DragLinkCursor), 21);
    PK_COMPARE(int(Qt::LastCursor), 21);
}

void PkNamespaceCase::textFormatValues()
{
    PK_COMPARE(int(Qt::PlainText), 0);
    PK_COMPARE(int(Qt::RichText), 1);
    PK_COMPARE(int(Qt::AutoText), 2);
}

void PkNamespaceCase::timeSpecValues()
{
    PK_COMPARE(int(Qt::LocalTime), 0);
    PK_COMPARE(int(Qt::UTC), 1);
    PK_COMPARE(int(Qt::OffsetFromUTC), 2);
}

void PkNamespaceCase::scrollBarPolicyValues()
{
    PK_COMPARE(int(Qt::ScrollBarAsNeeded), 0);
    PK_COMPARE(int(Qt::ScrollBarAlwaysOff), 1);
    PK_COMPARE(int(Qt::ScrollBarAlwaysOn), 2);
}

void PkNamespaceCase::caseSensitivityValues()
{
    PK_COMPARE(int(Qt::CaseInsensitive), 0);
    PK_COMPARE(int(Qt::CaseSensitive), 1);
}

void PkNamespaceCase::fillRuleValues()
{
    PK_COMPARE(int(Qt::OddEvenFill), 0);
    PK_COMPARE(int(Qt::WindingFill), 1);
}

void PkNamespaceCase::clipOperationValues()
{
    PK_COMPARE(int(Qt::NoClip), 0);
    PK_COMPARE(int(Qt::ReplaceClip), 1);
    PK_COMPARE(int(Qt::IntersectClip), 2);
}

void PkNamespaceCase::transformationModeValues()
{
    PK_COMPARE(int(Qt::FastTransformation), 0);
    PK_COMPARE(int(Qt::SmoothTransformation), 1);
}

void PkNamespaceCase::layoutDirectionValues()
{
    PK_COMPARE(int(Qt::LeftToRight), 0);
    PK_COMPARE(int(Qt::RightToLeft), 1);
    PK_COMPARE(int(Qt::LayoutDirectionAuto), 2);
}

void PkNamespaceCase::checkStateValues()
{
    // Krita 用 `Qt::CheckState::Unchecked` 这种限定语法（plain enum 名限定，
    // C++17 允许）——照抄 Qt 形态。
    PK_COMPARE(int(Qt::CheckState::Unchecked), 0);
    PK_COMPARE(int(Qt::CheckState::PartiallyChecked), 1);
    PK_COMPARE(int(Qt::CheckState::Checked), 2);
    PK_COMPARE(int(Qt::Unchecked), 0);
}

void PkNamespaceCase::itemDataRoleValues()
{
    PK_COMPARE(int(Qt::DisplayRole), 0);
    PK_COMPARE(int(Qt::DecorationRole), 1);
    PK_COMPARE(int(Qt::EditRole), 2);
    PK_COMPARE(int(Qt::ToolTipRole), 3);
    PK_COMPARE(int(Qt::StatusTipRole), 4);
    PK_COMPARE(int(Qt::WhatsThisRole), 5);
    PK_COMPARE(int(Qt::FontRole), 6);
    PK_COMPARE(int(Qt::TextAlignmentRole), 7);
    PK_COMPARE(int(Qt::BackgroundRole), 8);
    PK_COMPARE(int(Qt::ForegroundRole), 9);
    PK_COMPARE(int(Qt::CheckStateRole), 10);
    PK_COMPARE(int(Qt::UserRole), 0x0100);
}

void PkNamespaceCase::itemFlagsValues()
{
    PK_COMPARE(int(Qt::NoItemFlags), 0);
    PK_COMPARE(int(Qt::ItemIsSelectable), 1);
    PK_COMPARE(int(Qt::ItemIsEditable), 2);
    PK_COMPARE(int(Qt::ItemIsDragEnabled), 4);
    PK_COMPARE(int(Qt::ItemIsDropEnabled), 8);
    PK_COMPARE(int(Qt::ItemIsUserCheckable), 16);
    PK_COMPARE(int(Qt::ItemIsEnabled), 32);
    PK_COMPARE(int(Qt::ItemIsAutoTristate), 64);
    PK_COMPARE(int(Qt::ItemNeverHasChildren), 128);
    PK_COMPARE(int(Qt::ItemIsUserTristate), 256);
    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    PK_COMPARE(int(f), 33);
    PK_VERIFY(f.testFlag(Qt::ItemIsSelectable));
    PK_VERIFY(f.testFlag(Qt::ItemIsEnabled));
}

void PkNamespaceCase::timerTypeValues()
{
    PK_COMPARE(int(Qt::PreciseTimer), 0);
    PK_COMPARE(int(Qt::CoarseTimer), 1);
    PK_COMPARE(int(Qt::VeryCoarseTimer), 2);
}

void PkNamespaceCase::globalColorValues()
{
    // QColor 构造的 Qt::GlobalColor 实参依赖这些序号的数值。
    PK_COMPARE(int(Qt::color0), 0);
    PK_COMPARE(int(Qt::color1), 1);
    PK_COMPARE(int(Qt::black), 2);
    PK_COMPARE(int(Qt::white), 3);
    PK_COMPARE(int(Qt::darkGray), 4);
    PK_COMPARE(int(Qt::gray), 5);
    PK_COMPARE(int(Qt::lightGray), 6);
    PK_COMPARE(int(Qt::red), 7);
    PK_COMPARE(int(Qt::green), 8);
    PK_COMPARE(int(Qt::blue), 9);
    PK_COMPARE(int(Qt::cyan), 10);
    PK_COMPARE(int(Qt::magenta), 11);
    PK_COMPARE(int(Qt::yellow), 12);
    PK_COMPARE(int(Qt::darkRed), 13);
    PK_COMPARE(int(Qt::darkGreen), 14);
    PK_COMPARE(int(Qt::darkBlue), 15);
    PK_COMPARE(int(Qt::darkCyan), 16);
    PK_COMPARE(int(Qt::darkMagenta), 17);
    PK_COMPARE(int(Qt::darkYellow), 18);
    PK_COMPARE(int(Qt::transparent), 19);
}

void PkNamespaceCase::coexistWithGlobalEnums()
{
    // PkNamespace.h include 了 PkGlobal.h，两个 `namespace Qt` 块在同一个 TU 里
    // 并集可见（C++ 同名 namespace 多次打开，枚举名不重复）。这条探针钉住
    // 「AspectRatioMode（R-18 交付）与本头的 KeyboardModifier 同住一个 namespace
    // Qt 且各自取值正确」——重定义 AspectRatioMode 会在这里硬错。
    PK_COMPARE(int(Qt::AspectRatioMode::IgnoreAspectRatio), 0);
    PK_COMPARE(int(Qt::AspectRatioMode::KeepAspectRatio), 1);
    PK_COMPARE(int(Qt::AspectRatioMode::KeepAspectRatioByExpanding), 2);
    PK_COMPARE(int(Qt::KeyboardModifier::ShiftModifier), 0x02000000);
    PK_COMPARE(int(Qt::ControlModifier), 0x04000000);
}

void PkNamespaceCase::coexistAxisEnum()
{
    // PkGlobal.h 的 Axis 枚举（同 namespace Qt）。
    PK_COMPARE(int(Qt::Axis::XAxis), 0);
    PK_COMPARE(int(Qt::Axis::YAxis), 1);
    PK_COMPARE(int(Qt::Axis::ZAxis), 2);
}

int run_namespace_tests()
{
    PkNamespaceCase tc;
    const char *argv[] = {"test_pknamespace"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
