#!/usr/bin/env bash
# pk/image 试接：两个 driver（driver 降级路径，非真实文件编译试接）。
#
# 真实文件为什么编不过、driver 复刻的是哪段调用点、四条降级判据怎么满足——
# 全文在 driver_compare_qimages.cpp / driver_fast_copy_area.cpp 的头部注释，与
# task-5-report.md「试接说明」一节。这里只负责：真编译、真运行、跑绿打印证据。
#
# 两个 driver 都**零 Qt**：链接命令里没有任何 Qt 库、-I 里没有任何真 Qt 头，
# 全部 QImage/QRect/QPointF/QVector 名字经 compat 垫片解析到 Pk*，QRgb/qRed/
# qGreen/qBlue/qAlpha 经 qrgb_shim.h（待认领缺口⑤的脚手架）。跑完再对两个二进制
# 各做一次 `nm -u | grep -i qt`，有输出即 FAIL（自证零 Qt，与 graft_check.sh 的
# 判据同一口径）。
set -u
cd "$(dirname "$0")/../../../.." || exit 1  # → fork 仓库根（graft → tests → image → pk → 根）

CXX=${CXX:-g++}
OUTDIR=pk/image/build/graft
mkdir -p "$OUTDIR"

# -I 里绝不能出现真 Qt 头目录（$QT/include）——那会让 QImage 在某个 include 顺序
# 下解析到真 Qt，driver 就变成「拿真 Qt 跑真 Qt」，不是替代品证据。
INC=(
    -I pk/image/compat        # compat/QImage：QImage→PkImage（并传递 include QRect）
    -I pk/geometry/compat     # compat/QRect/QPoint/QSize/QtGlobal（QImage 传递 include）
    -I pk/container/compat    # compat/QVector：driver B 的 m_rectsToCopy
    -I pk/image/tests/graft   # qrgb_shim.h（QRgb/qRed/...，待认领缺口⑤）
    -I pk/image               # PkImage.h / PkImageData.h
    -I pk/geometry            # PkRect/PkPoint/PkSize/PkTransform/PkGlobal
    -I pk/container           # PkArrayData/PkVector（经 compat/QVector）
)

# 依赖源：PkImage + 全部 geometry 的 out-of-line 实现（driver 直接链源码，不依赖
# cmake 先构建出 .a——这与 graft_check.sh「编译参数 + 垫片」的自包含精神一致）。
SOURCES=(pk/image/PkImage.cpp
         pk/geometry/PkGlobal.cpp
         pk/geometry/PkPoint.cpp
         pk/geometry/PkSize.cpp
         pk/geometry/PkRect.cpp
         pk/geometry/PkTransform.cpp)

fail=0

build_and_run() {
    local src="$1" out="$2"
    printf '── 编译并运行 %s ──\n' "$src"
    if ! "$CXX" -std=c++17 -O1 -Wall "${INC[@]}" "$src" "${SOURCES[@]}" -o "$out"; then
        printf '  编译失败\n' >&2
        fail=1
        return
    fi
    if ! "$out"; then
        printf '  运行失败（断言没全绿）\n' >&2
        fail=1
        return
    fi
    # 零 Qt 自证：这个二进制的未定义符号里不得出现任何 Qt 痕迹。
    if nm -u "$out" | grep -i qt; then
        printf '  零 Qt 自证失败：上面有 Qt 未定义符号\n' >&2
        fail=1
        return
    fi
    printf '  零 Qt 自证通过（nm -u | grep -i qt 无输出）\n'
}

build_and_run pk/image/tests/graft/driver_compare_qimages.cpp "$OUTDIR/graft_driver_compare_qimages"
build_and_run pk/image/tests/graft/driver_fast_copy_area.cpp "$OUTDIR/graft_driver_fast_copy_area"

exit "$fail"
