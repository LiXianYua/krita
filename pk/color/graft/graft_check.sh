#!/usr/bin/env bash
# 真实调用点零改动试接：把 <QColor> 解析到 compat/QColor 垫片（→ PkColor），
# 语法检查真实 Krita 生产头 + 一个复刻调用点形状的 driver。源文件一个字都没改。
# 形态照抄 pk/flags/graft/graft_check.sh（R-20）与 pk/port/graft（R-12）。
#
# 为什么 -include 而非只 -I：部分生产头靠传递包含拿到 QColor（自身不 #include
# <QColor>），-include 把垫片提到翻译单元最前面，QColor 在第一次用到前就已被
# #define 成 PkColor。这是编译参数，不是对调用点的改动。
set -u
export LC_ALL=C
cd "$(dirname "$0")/../../.." || exit 1
CXX=${CXX:-g++}
# 已交付垫片的全 -I：QColor(本任务) + QString(R-01) + QList/QVector(R-02) +
# QtGlobal(R-18) + QObject/QTest(pk/test)。缺哪块垫片就让哪块头 FAIL（登记）。
INC=(
    -include pk/color/compat/QColor
    -I pk/color/compat -I pk/color
    -I pk/global/compat -I pk/test/compat
    -I pk/string/compat -I pk/container/compat
    -I pk/string -I pk/global -I pk/namespace -I pk/container
    -I pk/time/compat -I pk/pointer/compat
    -I libs/global -I libs/pigment -I libs/image -I libs/canvas -I libs/flake -I libs/widgets
)
fail=0
try_compile() { "$CXX" -std=c++17 -fsyntax-only "${INC[@]}" -I "$(dirname "$1")" "$1"; }
check_pass() {
    local f="$1"; local out
    out=$(try_compile "$f" 2>&1)
    if [ $? -eq 0 ]; then printf '  graft OK: %s\n' "$f";
    else printf '  graft FAILED（预期该编过）: %s\n' "$f"; printf '%s\n' "$out" | head -20 | sed 's/^/    /'; fail=1; fi
}
check_expect_fail() {
    local f="$1" pattern="$2" reason="$3"; local out
    out=$(try_compile "$f" 2>&1)
    if [ $? -eq 0 ]; then printf '  graft 缺口已消失: %s（原登记：%s）\n' "$f" "$reason"; fail=1; return; fi
    if printf '%s\n' "$out" | grep -qE "$pattern"; then
        printf '  graft 按登记失败: %s（卡在 %s → %s）\n' "$f" "$pattern" "$reason"
    else printf '  graft 错因不匹配登记: %s（预期 /%s/，归属 %s）\n' "$f" "$pattern" "$reason"; printf '%s\n' "$out" | head -20 | sed 's/^/    /'; fail=1; fi
}

# ── EXPECT_PASS：只靠 QColor + 已交付垫片（QString/QList/QtGlobal）就该编过 ──
# KoChannelInfo.h：真 Krita 生产头，QColor 作默认实参/成员/返回值，是 color 垫片
# 的最小真实消费方（另需 <limits> 与 QString/QList/QtGlobal 垫片，均已交付）。
for f in libs/pigment/KoChannelInfo.h; do check_pass "$f"; done

# ── driver：复刻调用点形状（真实构造/成员调用/取值核对），见 instantiate_color.cpp ──
out=$("$CXX" -std=c++17 -I pk/color/compat -I pk/color -I pk/global -I pk/string -I pk/namespace -I pk/container \
      pk/color/graft/instantiate_color.cpp \
      pk/color/PkColor.cpp \
      pk/string/PkString_core.cpp pk/string/PkString_query.cpp pk/string/PkString_format.cpp pk/string/PkStringCodec.cpp \
      pk/global/PkGlobal.cpp \
      -o /tmp/r27_color_instantiate 2>&1)
if [ $? -eq 0 ]; then
    if /tmp/r27_color_instantiate; then printf '  graft driver 跑绿\n'; else printf '  graft driver 返回非 0（语义错）\n'; fail=1; fi
else printf '  graft driver 编不过:\n%s\n' "$out" | head -30 | sed 's/^/    /'; fail=1; fi

# ── EXPECT_FAIL：卡在还没交付的类型上，登记该由哪个后续任务补 ──
# 每条只登记该文件 FIRST 阻塞的依赖（编译停在第 1 个 missing include）。
# 卡 export 宏（不在本任务 locks 内，归各 S 批次处理）
check_expect_fail libs/pigment/KoLabDarkenColorTransformation.h 'kritapigment_export' 'S-03-a 剥 pigment'
check_expect_fail libs/pigment/KoAlphaMaskApplicatorBase.h      'kritapigment_export' 'S-03-a 剥 pigment'
check_expect_fail libs/image/kis_update_outline_job.h            'kritaimage_export'   'S-06 剥 image'
check_expect_fail libs/image/KisBezierGradientMesh.h             'kritaimage_export'   'S-06 剥 image'
check_expect_fail libs/flake/KoShapeStroke.h                     'kritaflake_export'   'S-08 剥 flake'
check_expect_fail libs/flake/KoGradientHelper.h                  'kritaflake_export'   'S-08 剥 flake'
check_expect_fail libs/global/kis_painting_tweaks.h              'kritaglobal_export'  'S-02-a 剥 global'

# 卡 QMetaType（R-06 PkVariant 未交付）
check_expect_fail libs/canvas/kis_grid_config.h                  'QMetaType'  'R-06 PkVariant'
check_expect_fail libs/pigment/KoColorDisplayRendererInterface.h 'QMetaType'  'R-06 PkVariant'

# 卡 QDebug（R-08 pk/log 未交付）
check_expect_fail libs/pigment/colorspaces/KoRgbU8ColorSpace.h   'QDebug'  'R-08 pk/log'

# 卡 QMetaType（R-06 PkVariant；QScopedPointer 已由 R-04 pk/pointer 垫片解析，
# 编译链停在后继 KoColor.h 的 <QMetaType>）
check_expect_fail libs/canvas/kis_display_color_converter.h      'QMetaType'  'R-06 PkVariant'

# 卡 QPointF（R-21/R-22 几何未交付）
check_expect_fail libs/global/kis_dom_utils.h                    'QPointF'  'R-21/R-22 几何'

# 零改动自证
dirty=$(git status --porcelain -- libs/ plugins/)
if [ -n "$dirty" ]; then printf '  源树被改动了 —— 试接必须零改动\n' >&2; printf '%s\n' "$dirty" >&2; fail=1; fi
exit "$fail"
