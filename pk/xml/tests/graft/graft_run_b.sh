#!/usr/bin/env bash
# R-07 Task 4 判据②：候选 B —— libs/image/kis_distance_information.cpp 零改动
# 编译试接，链接真实、未修改的 libs/image/tests/kis_distance_information_test.cpp
# （走 R-11 的 PK_TEST_MAIN/SIMPLE_TEST_MAIN 机制），跑绿 brief 要求的
# `testInitInfoXMLClone`（四组构造参数往返 + operator==）。
#
# 六段式结构照抄 graft_run_a.sh（①原地编译生产 .cpp ②链接测试③链接④跑
# ⑤nm -u判据③⑥git diff --quiet零改动自证），本脚本额外的一步是"用 pk/test
# harness 的命令行过滤只跑 testInitInfo 这个 slot"——见下方④的说明：
# `kis_distance_information_test.cpp` 有两个 Q_SLOTS（testInitInfo/
# testInterpolation），`testInitInfoXMLClone` 是 testInitInfo() 内部调用的
# 私有方法，不是独立 slot；`testInterpolation` 依赖 KisPaintInformation/
# KisAlgebra2D 的真实插值数学，本试接把这两个类型/命名空间做成"编译期占位"
# （stubs/brushengine/kis_paint_information.h、graft_stubs.cpp 里的
# KisAlgebra2D::directionBetweenPoints），不追求数值正确——只跑 testInitInfo
# 这一个 slot 是本候选判据②明确要求的范围，不是回避 testInterpolation 编不过。
#
# 与 graft_run_a.sh 同样的关键差异：对被测/测试源做"原地编译 + 原地引用"，
# 源树零字节改动全靠第⑥段的 git diff --quiet 自证。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

GRAFT=pk/xml/tests/graft
BUILD=$GRAFT/build
STUBS=$GRAFT/stubs
CXX=${CXX:-g++}
BOOST_INC=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install/include
rc=0

# ---------------------------------------------------------------------------
# 0. 依赖库：与 graft_run_a.sh 完全一致的三个兄弟 build 目录探测（各自不复用
#    彼此的 build，归各自 tests/run_tests.sh 管，这里各自另建，不存在就现建）。
# ---------------------------------------------------------------------------
if [ ! -f pk/xml/build/libpkxml.a ]; then
    cmake -S pk/xml -B pk/xml/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/xml/build -j"$(nproc)" >/dev/null
fi
if [ ! -f pk/geometry/build/libpkgeometry.a ]; then
    cmake -S pk/geometry -B pk/geometry/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/geometry/build -j"$(nproc)" >/dev/null
fi
if [ ! -f pk/log/build/libpklog.a ]; then
    cmake -S pk/log -B pk/log/build -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build pk/log/build -j"$(nproc)" >/dev/null
fi

SPDLOG_LIB=""
for cand in pk/log/build/libspdlogd.a pk/log/build/libspdlog.a; do
    if [ -f "$cand" ]; then SPDLOG_LIB="$cand"; break; fi
done
if [ -z "$SPDLOG_LIB" ]; then
    printf 'graft_run_b.sh: 找不到 spdlog 静态库（试过 libspdlogd.a 与 libspdlog.a，\n' >&2
    printf '  pk/log/build 下都没有）。见 graft_run_a.sh 同一段注释。\n' >&2
    exit 1
fi

PUGIXML_INC=$(find pk/xml/build/_deps -maxdepth 2 -type d -name src -path "*pugixml*" | head -1)
if [ -z "$PUGIXML_INC" ]; then
    printf 'graft_run_b.sh: 找不到 pugixml 头文件目录（见 graft_run_a.sh 同一段注释）。\n' >&2
    exit 1
fi

rm -rf "$BUILD"; mkdir -p "$BUILD"

# -I 顺序有讲究，同 graft_run_a.sh：$STUBS 必须最靠前。候选 B 比候选 A 多两个
# 需要抢先命中的模块路径垫片：$STUBS/QtCore（真 Qt 的 <QtCore/qmath.h> 与
# $BOOST_INC 共用同一个 include 前缀，不抢先会解析到真 qglobal.h）与
# $STUBS/brushengine（KisPaintInformation 编译期占位，见该文件头注释）。
INC="-I $STUBS -I pk/xml -I pk/xml/compat -I $PUGIXML_INC \
     -I pk/geometry -I pk/geometry/compat \
     -I pk/pointer -I pk/pointer/compat \
     -I pk/container -I pk/container/compat \
     -I pk/port -I pk/port/compat \
     -I pk/log -I pk/log/compat \
     -I pk/string \
     -I pk/test -I pk/test/compat \
     -I libs/global -I libs/image -I libs/image/brushengine \
     -I $BOOST_INC \
     -I $BUILD -I ."
DEFS=(-DQT_NO_DEBUG)

# FORCE：四个必须在解析到对应裸标识符**之前**就位的宏/类型：
#   · $STUBS/QString：同 graft_run_a.sh。
#   · pk/geometry/compat/QTransform：kis_distance_information.cpp 直接
#     `#include <QTransform>`（`getNextPointPositionAnisotropic` 用到），
#     但更早的 `#include <QPointF>`（kis_distance_information.h:11）已经需要
#     `pk/geometry/compat/QtGlobal` 里的 qreal/qAbs 家族——同候选A的理由。
#   · $STUBS/QVariant：`kis_dom_utils.h`（经 kis_distance_information.cpp:17
#     `#include "kis_dom_utils.h"` 拉入，`fromXML` 用到
#     `KisDomUtils::toDouble`/`toInt`）裸用 QVariant，同候选 A 的理由。
#   · pk/container/compat/QList：`kis_algebra_2d.h`（真品，经
#     kis_distance_information.cpp:16 拉入）裸用 `QList<T>` 声明一批自由
#     函数/`VectorPath` 类，自己没有 `#include <QList>`（复刻真 Qt 里 QList
#     经 <QtGlobal> 传递可见这条传递性，同 pk/container/compat/QList
#     头注释）。
#   · pk/log/compat/QDebug：`kis_algebra_2d.h` 同样裸用 `QDebug`
#     （`operator<<(QDebug, const VectorPath&)` 等自由函数声明）。
FORCE=(-include "$STUBS/QString" -include pk/geometry/compat/QTransform \
       -include "$STUBS/QVariant" -include pk/container/compat/QList \
       -include pk/log/compat/QDebug)

# ① 被测实现：libs/image/kis_distance_information.cpp 原地编译，不复制不改名。
#    brief 声称候选 B "无需任何 stub"——实测发现这只在"只看 toXML/fromXML
#    两个函数签名"的口径下成立；整个 .cpp 作为一个翻译单元编译时，还牵出
#    QVector2D/QVector/QPolygonF/QPainterPath（kis_algebra_2d.h 的
#    #include）与 KisPaintInformation（brushengine/kis_paint_information.h，
#    候选 A 没有踩到）两类真正的编译期占位，比候选 A 的 QColor/i18n 更深一层
#    ——README.md「已知偏离清单」已记录这处与 plan §2 候选 B 描述的偏离。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" $INC \
    -c libs/image/kis_distance_information.cpp -o "$BUILD/kis_distance_information.o" \
    2>"$BUILD/compile_impl.log"; then
    printf '  试接编译失败: kis_distance_information.cpp\n'
    sed 's/^/    /' "$BUILD/compile_impl.log" | head -80
    exit 1
fi

# ①.5 kis_debug.cpp：kis_distance_information.cpp 经 kis_debug.h 用到的
#     Q_LOGGING_CATEGORY 分类对象与自由函数，真身都在这个 .cpp 里——同
#     graft_run_a.sh 的理由，原地编译，不属于候选源文件，但也是真实、未修改
#     的 Krita 生产源码。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" $INC \
    -c libs/global/kis_debug.cpp -o "$BUILD/kis_debug.o" \
    2>"$BUILD/compile_debug.log"; then
    printf '  试接编译失败: kis_debug.cpp\n'
    sed 's/^/    /' "$BUILD/compile_debug.log" | head -80
    exit 1
fi

# ② 生成 PK 测试 binder，直接对**真实路径**的头跑，不复制。
if ! python3 pk/test/pk_test_moc.py libs/image/tests/kis_distance_information_test.h \
    -o "$BUILD/binder.inc" 2>"$BUILD/moc.log"; then
    printf '  pk_test_moc 生成 binder 失败\n'
    sed 's/^/    /' "$BUILD/moc.log" | head -40
    exit 1
fi

# ②.5 driver.cpp：构建期胶水（见 graft_run_b_driver.cpp 文件头注释），不是
#     从源树复制来的文件。与候选 A 的 driver 不同：不需要"外挂函数体"这一段
#     ——kis_distance_information_test.cpp 没有 KisTimeSpan 那样"声明在别处、
#     定义拖着无关依赖树"的缺口。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" $INC \
    -c "$GRAFT/graft_run_b_driver.cpp" -o "$BUILD/driver.o" \
    2>"$BUILD/compile_driver.log"; then
    printf '  试接编译失败: driver.cpp\n'
    sed 's/^/    /' "$BUILD/compile_driver.log" | head -80
    exit 1
fi

# ②.6 graft_stubs.cpp：kis_assert.h（真品）声明的四个断言函数的实现侧
#     （同 graft_run_a.sh）+ PkDomUtilsGraftString::number()/remove() 的实现
#     （含候选 B 新增的三参 number(double,char,int) 重载）。这个文件是候选
#     A/B 共用的实现侧，**不带 FORCE**（同 graft_run_a.sh 的编译行）——
#     `KisAlgebra2D::directionBetweenPoints` 的编译期占位改放在
#     graft_run_b_driver.cpp 里（候选 B 独有），不放在这份共用文件，理由见
#     该函数上方注释（放这里会导致候选 A 的构建回归，已实测踩过并改正）。
if ! "$CXX" -std=c++17 "${DEFS[@]}" $INC \
    -c "$STUBS/graft_stubs.cpp" -o "$BUILD/graft_stubs.o" \
    2>"$BUILD/compile_stubs.log"; then
    printf '  试接编译失败: graft_stubs.cpp\n'
    sed 's/^/    /' "$BUILD/compile_stubs.log" | head -80
    exit 1
fi

# ③ 链接
if ! "$CXX" -std=c++17 \
    "$BUILD/driver.o" "$BUILD/kis_distance_information.o" "$BUILD/kis_debug.o" "$BUILD/graft_stubs.o" \
    pk/xml/build/libpkxml.a pk/geometry/build/libpkgeometry.a pk/log/build/libpklog.a \
    pk/xml/build/libpktest.a pk/xml/build/libpkstring.a pk/xml/build/libpugixml.a \
    "$SPDLOG_LIB" \
    -o "$BUILD/test_kis_distance_information" 2>"$BUILD/link.log"; then
    printf '  试接链接失败\n'
    sed 's/^/    /' "$BUILD/link.log" | head -80
    exit 1
fi

# ④ 跑 —— 只跑 testInitInfo 这一个 slot（pk/test harness 支持 QTest 风格的
#     命令行过滤，见 pk/test/PkTestRunner.cpp 的 execPlan/selected）。
#     testInitInfoXMLClone 是 testInitInfo() 内部调用的私有方法，覆盖 brief
#     要求的判据；testInterpolation 是另一个独立 slot，依赖
#     KisPaintInformation/KisAlgebra2D 的真实数值行为（本试接把它们做成
#     编译期占位，不追求数值正确），不在本候选判据范围内，故意不跑。
if "./$BUILD/test_kis_distance_information" testInitInfo >"$BUILD/run.log" 2>&1; then
    printf '  试接跑绿: KisDistanceInformationTest::testInitInfo（含 testInitInfoXMLClone）\n'
    grep -E '^(PASS|FAIL|Totals)' "$BUILD/run.log" | sed 's/^/    /'
else
    printf '  试接跑挂: KisDistanceInformationTest::testInitInfo\n'
    sed 's/^/    /' "$BUILD/run.log" | head -40
    rc=1
fi

# ⑤ 判据③：产物不得有 Qt 未定义符号。
undef=$(nm -u "$BUILD/test_kis_distance_information" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf '  试接产物含 Qt 符号:\n%s\n' "$undef"
    rc=1
else
    printf '    nm -u test_kis_distance_information | grep -i qt: 无输出\n'
fi

# ⑥ 源树零改动自证。
if ! git diff --quiet -- libs/global libs/image; then
    printf '  源树被改动了 —— 试接必须零改动\n' >&2
    git diff --stat -- libs/global libs/image >&2
    rc=1
else
    printf '    git diff --quiet -- libs/global libs/image: 干净\n'
fi

exit "$rc"
