#!/usr/bin/env bash
# 真实调用点零改动试接：把 <QFlags> 解析到 compat/QFlags 垫片（→ PkFlags），
# 语法检查真实 Krita 生产头 + 一个复刻调用点形状的 driver。源文件一个字都没改。
# 形态照抄 pk/port/graft/graft_check.sh（R-12）与 pk/string/tests/graft/graft_check.sh（R-01）。
#
# 为什么 -include 而非只 -I：KisRenderPassFlags.h / KisUpdaterContextSnapshotEx.h
# 零 #include（在真实构建里靠传递包含拿到 Q_DECLARE_FLAGS 宏）。-include 把垫片
# 提到翻译单元最前面，Q_DECLARE_FLAGS 在用到之前就已被 #define 成 PK_DECLARE_FLAGS。
# 这是编译参数，不是对调用点的改动。
set -u
export LC_ALL=C
cd "$(dirname "$0")/../../.." || exit 1
CXX=${CXX:-g++}
INC=(
    -include pk/flags/compat/QFlags
    -I pk/flags/compat -I pk/flags
    -I pk/flags/graft/stubs
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

# ── EXPECT_PASS：只靠 compat/QFlags 就该编过（纯标志声明头，无其它依赖） ──
for f in \
    libs/image/KisNodeAdditionFlags.h \
    libs/image/KisProjectionUpdateFlags.h \
    libs/image/KisRenderPassFlags.h \
    libs/image/KisUpdaterContextSnapshotEx.h \
; do check_pass "$f"; done

# ── driver：复刻调用点形状（真实枚举类型 + 真实调用形状），见 instantiate.cpp 头注释 ──
out=$("$CXX" -std=c++17 -include pk/flags/compat/QFlags -I pk/flags/compat -I pk/flags -I . pk/flags/graft/instantiate.cpp -o /tmp/r20_instantiate 2>&1)
if [ $? -eq 0 ]; then
    if /tmp/r20_instantiate; then printf '  graft driver 跑绿\n'; else printf '  graft driver 返回非 0（语义错）\n'; fail=1; fi
else printf '  graft driver 编不过:\n%s\n' "$out" | head -20; fail=1; fi

# ── EXPECT_FAIL：卡在还没交付的类型上，登记该由哪个后续任务补 ──
# 每条只登记该文件 FIRST 阻塞的依赖（编译停在第 1 个 missing include）

# 卡 QMetaType（R-06 PkVariant 未交付）
check_expect_fail libs/global/KisTransformComponents.h  'QMetaType'  'R-06 PkVariant'

# 卡 krita*_export.h（不在本任务 locks 内，归各 S 批次处理）
check_expect_fail libs/image/KisAnimAutoKey.h           'kritaimage_export'  'S-06 剥 image'
check_expect_fail libs/flake/KoPathPoint.h    'kritaflake_export'  'S-08 剥 flake'
check_expect_fail libs/flake/KoZoomMode.h    'kritaflake_export'  'S-08 剥 flake'
check_expect_fail libs/flake/KoShapeSavingContext.h  'kritaflake_export'  'S-08 剥 flake'
check_expect_fail libs/flake/KoSnapGuide.h   'kritaflake_export'  'S-08 剥 flake'
check_expect_fail libs/flake/text/KoSvgTextShape.h  'kritaflake_export'  'S-08 剥 flake'
check_expect_fail libs/pigment/KoColorConversionTransformation.h  'kritapigment_export'  'S-03-a 剥 pigment'
check_expect_fail libs/global/KoUnit.h             'kritaglobal_export'  'S-02-a 剥 global'
check_expect_fail libs/image/commands/kis_image_layer_add_command.h  'kritaimage_export'  'S-06 剥 image'

# 卡 QScopedPointer（R-04 pk/pointer 未交付至主树 include 路径）
check_expect_fail libs/resources/KoResourcePaths.h  'QScopedPointer'  'R-04 pk/pointer'

# 卡 QString / QList / QDateTime（R-01/R-02/R-16 未交付至主树 include 路径）
check_expect_fail libs/impex/KisImportExportUtils.h 'QString'  'R-01 pk/string/R-02 pk/container'
check_expect_fail libs/impex/KisDocument.h  'QDateTime'  'R-16 pk/time'
check_expect_fail libs/image/kis_image_animation_interface.h 'QObject'  'R-05 pk/signal'
check_expect_fail libs/image/kis_base_rects_walker.h  'QStack'  'R-02 pk/container'
check_expect_fail libs/pigment/KoColorConversionSystem_p.h  'QDebug'  'R-08 pk/log'

# 卡 kundo2 命令（不在本任务 locks 内，归 S-06 剥 image 时处理）
check_expect_fail libs/image/kis_layer_utils.h  'kundo2command'  'S-06 剥 image'
check_expect_fail libs/image/kis_processing_applicator.h  'kundo2commandextradata'  'S-06 剥 image'
check_expect_fail libs/image/kis_transaction.h  'kundo2command'  'S-06 剥 image'

# 卡 QFont（R-21/R-22 几何 未交付）
check_expect_fail libs/flake/text/KoSvgText.h  'QFont'  'R-21/R-22 几何'

# 卡复杂工程头依赖链（kis_types.h 等，S-06 剥 image 时处理）
check_expect_fail libs/image/KisFakeRunnableStrokeJobsExecutor.h  'KisRunnableStrokeJobsInterface'  'S-06 剥 image'
check_expect_fail libs/image/kis_merge_walker.h  'kis_types'  'S-06 剥 image'
check_expect_fail libs/image/kis_refresh_subtree_walker.h  'kis_types'  'S-06 剥 image'

# 卡 KisLodPreferences.h 的 kis_assert.h 依赖
check_expect_fail libs/image/KisLodPreferences.h  'kis_assert'  'S-06 剥 image'

# 零改动自证
dirty=$(git status --porcelain -- libs/ plugins/)
if [ -n "$dirty" ]; then printf '  源树被改动了 —— 试接必须零改动\n' >&2; printf '%s\n' "$dirty" >&2; fail=1; fi
exit "$fail"