#!/usr/bin/env bash
# 真实调用点零改动试接：把 <QIODevice> 解析到 compat/QIODevice 垫片（→ PkStream），
# 语法检查真实 Krita 生产头文件。所有文件本身一个字都没改——兼容性全靠编译参数
# 和垫片，形态照抄 pk/string/tests/graft/graft_check.sh。
#
# 为什么除了 -I 还要 -include：
#   kis_meta_data_io_backend.h 第 13 行是 `class QIODevice;` 前置声明。只给
#   -I 的话，这一行被解析时垫片还没进来 → 那是一个**真的**叫 QIODevice 的
#   类。-include 把垫片提到翻译单元最前面，`class QIODevice;` 就被宏改写成
#   `class PkStream;`，前置声明照样成立。这是编译参数，不是对调用点的改动。
#   成因与 pk/string/tests/graft/graft_check.sh 对 QString 的处理完全相同。
#
# 判据②的完整含义不是「挑几个能过的头文件出来证明垫片存在」，而是整张候选表
# 都要跑：
#   - EXPECT_PASS：这个头文件在只有 compat/QIODevice（+ R-01 的 compat/QString）
#     的情况下就该编过。编不过 → 这一批 API 形状没做对，`exit 1`。
#   - EXPECT_FAIL <正则>：这个头文件预期还编不过，因为它卡在某个**还没交付**
#     的 Qt 类型上（登记了该由哪个后续任务补）。这一条本身也是一个断言，两种
#     方式都算失败：
#       ① 编过了 —— 说明缺口已经消失（八成是后续任务已落地，或候选表判断
#          有误），登记过期了，必须先去更新这张表，不能让脚本悄悄变绿；
#       ② 编不过，但错误信息里找不到登记的关键词 —— 说明卡住的原因换了，
#          归属判断可能也要跟着换，同样不能放过。
# 这样「缺口在哪、该谁补」就从一段会漂移的文字变成一条可执行的断言：R-02 落地
# 后 KoStore.h 会自己开始编过，这个脚本会在第一时间报出来，而不是靠人记得
# 回来更新登记。
set -u
cd "$(dirname "$0")/../../.." || exit 1

CXX=${CXX:-g++}
INC=(
    -include pk/port/compat/QIODevice
    -include pk/string/compat/QString
    -I pk/port/compat -I pk/port
    -I pk/string/compat -I pk/string
    -I pk/port/graft/stubs
)
fail=0

try_compile() {
    # $1 = 文件路径；把 stdout 设为编译输出，返回值是编译退出码。
    local f="$1"
    "$CXX" -std=c++17 -fsyntax-only "${INC[@]}" -I "$(dirname "$f")" "$f"
}

check_pass() {
    local f="$1"
    local out
    out=$(try_compile "$f" 2>&1)
    if [ $? -eq 0 ]; then
        printf '  graft OK: %s\n' "$f"
    else
        printf '  graft FAILED（预期该编过）: %s\n' "$f"
        printf '%s\n' "$out" | head -20 | sed 's/^/    /'
        fail=1
    fi
}

check_expect_fail() {
    # $1 = 文件路径, $2 = 错因正则, $3 = 归属说明（写进日志，供人核对）
    local f="$1" pattern="$2" reason="$3"
    local out
    out=$(try_compile "$f" 2>&1)
    if [ $? -eq 0 ]; then
        printf '  graft 缺口已消失，请更新登记: %s（原登记：%s）\n' "$f" "$reason"
        fail=1
        return
    fi
    if printf '%s\n' "$out" | grep -qE "$pattern"; then
        printf '  graft 按登记失败: %s（卡在 %s → %s）\n' "$f" "$pattern" "$reason"
    else
        printf '  graft 错因不匹配登记: %s（预期匹配 /%s/，归属 %s）\n' "$f" "$pattern" "$reason"
        printf '%s\n' "$out" | head -20 | sed 's/^/    /'
        fail=1
    fi
}

# ── EXPECT_PASS：只靠 compat/QIODevice（+ 已交付的 compat/QString）就该编过 ──
for f in \
    libs/metadata/kis_meta_data_io_backend.h \
    libs/widgetutils/KoProgressProxy.h \
    libs/widgetutils/KoFakeProgressProxy.h \
; do
    check_pass "$f"
done

# ── EXPECT_FAIL：卡在还没交付的 Qt 类型上，登记该由哪个后续任务补 ──
check_expect_fail libs/store/KoStore.h                                       'QByteArray'     'R-02'
check_expect_fail libs/store/KoStoreDevice.h                                 'QByteArray'     'R-02'
check_expect_fail libs/store/KoDirectoryStore.h                              'QByteArray'     'R-02'
check_expect_fail libs/image/tiles3/swap/kis_abstract_compression.h          'QtGlobal'       'R-02/R-03'
check_expect_fail libs/brush/kis_png_brush.h                                 'QImage'         'R-15'
check_expect_fail libs/brush/kis_svg_brush.h                                 'QImage'         'R-15'
check_expect_fail libs/image/kis_composite_progress_proxy.h                  'QList'          'R-02'
check_expect_fail libs/command/kis_undo_store.h                              'QObject'        'R-05'
check_expect_fail libs/resources/KisStoragePlugin.h                          'QScopedPointer' 'R-04'
check_expect_fail libs/image/kis_node_graph_listener.h                       'QScopedPointer' 'R-04'
check_expect_fail libs/pigment/resources/KoCachedGradient.h                  'expected class-name' '工程头闭包，非 Qt 缺口'
check_expect_fail libs/flake/resources/KoFontFamily.h                        'KoResource\.h'  '工程头闭包，非 Qt 缺口'

# 源树零改动自证——照抄 pk/test/graft/graft_run.sh 结尾那段。
if ! git diff --quiet -- libs/; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/ >&2
    fail=1
fi

exit "$fail"
