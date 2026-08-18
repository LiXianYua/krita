#!/usr/bin/env bash
# R-07 Task 3 判据②：候选 A —— libs/global/kis_dom_utils.cpp 零改动编译试接，
# 链接真实、未修改的 libs/image/tests/kis_dom_utils_test.cpp（走 R-11 的
# PK_TEST_MAIN/SIMPLE_TEST_MAIN 机制），跑绿。
#
# 六段式结构照抄 pk/config/tests/graft/graft_run.sh（①原地编译生产 .cpp
# ②链接测试③链接④跑⑤nm -u判据③⑥git diff --quiet零改动自证），本任务在
# ①②之间多一步"生成 PK 测试 binder"（pk/test 的 pk_test_moc.py，替代 moc）。
#
# 与 pk/test/graft/graft_run.sh 的一个关键差异：那份脚本对被测/测试源做
# "复制到构建目录 + D-23 机械改名"（因为它要证明 rename.sed 机械可行）。
# 本任务不复制、不改名——直接对 libs/global/kis_dom_utils.cpp **原地**编译，
# 测试 .cpp 也是**原地**（未复制）被引用，源树零字节改动全靠本脚本第⑥段的
# git diff --quiet 自证，不靠"改名之后编不过就报错"这种间接证据。
set -eu
cd "$(dirname "$0")/../../../.." || exit 1     # → fork 仓库根

GRAFT=pk/xml/tests/graft
BUILD=$GRAFT/build
STUBS=$GRAFT/stubs
CXX=${CXX:-g++}
BOOST_INC=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install/include
rc=0

# ---------------------------------------------------------------------------
# 0. 依赖库：pk/xml 自己的 build 目录已经把 pkstring/pugixml/pktest 一起建了
#    （pk/xml/CMakeLists.txt 用 add_subdirectory 嵌入），只缺 pk/geometry
#    （PkTransform::setMatrix 等，kis_dom_utils.cpp 的 QTransform 往返用到）
#    与 pk/log（PkLoggingCategory/PkDebug/PkMessageLogger，kis_debug.cpp 用到）
#    两个兄弟目录各自的 build。三处都不复用彼此的 build 目录（各自归自己的
#    tests/run_tests.sh 管），这里各自另建，不存在就现建。
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

# pk/log 的 spdlog 静态库：Debug 构建下 FetchContent 兜底产出 libspdlogd.a，
# Release 产出 libspdlog.a（无 d 后缀）；find_package 命中系统安装位置时两个
# 都不存在（同 pk/config/tests/graft/graft_run.sh 的探测手法）。
SPDLOG_LIB=""
for cand in pk/log/build/libspdlogd.a pk/log/build/libspdlog.a; do
    if [ -f "$cand" ]; then SPDLOG_LIB="$cand"; break; fi
done
if [ -z "$SPDLOG_LIB" ]; then
    printf 'graft_run_a.sh: 找不到 spdlog 静态库（试过 libspdlogd.a 与 libspdlog.a，\n' >&2
    printf '  pk/log/build 下都没有）。这条探测只覆盖 CMakeLists.txt 的\n' >&2
    printf '  FetchContent 兜底路径；如果这次是 find_package 命中了系统安装的\n' >&2
    printf '  spdlog，请另外确认链接方式。\n' >&2
    exit 1
fi

PUGIXML_INC=$(find pk/xml/build/_deps -maxdepth 2 -type d -name src -path "*pugixml*" | head -1)
if [ -z "$PUGIXML_INC" ]; then
    printf 'graft_run_a.sh: 找不到 pugixml 头文件目录（pk/xml/build/_deps 下没有\n' >&2
    printf '  匹配 *pugixml*/src 的目录）。可能是 find_package 命中了系统安装的\n' >&2
    printf '  pugixml（那种情况下不需要这个 -I，此处需要改判断逻辑）。\n' >&2
    exit 1
fi

rm -rf "$BUILD"; mkdir -p "$BUILD"

# -I 顺序有讲究：$STUBS 必须最靠前——QString/QLocale/QColor/klocalizedstring.h/
# KoConfig.h/config-debug.h/config-memory-leak-tracker.h/QtGlobal/QAtomicInt/
# QMetaType/QVariant/QVector3D/QLineF/QScreen/QWidget/QGuiApplication/
# QStringList/QTextStream/kritaglobal_export.h/kritaimage_export.h 在
# libs/global 或某个 pk/* compat 里也可能有同名/同路径候选，我们要的是 stubs
# 目录下这份占位（详细理由见各文件头注释）。
INC="-I $STUBS -I pk/xml -I pk/xml/compat -I $PUGIXML_INC \
     -I pk/geometry -I pk/geometry/compat \
     -I pk/pointer -I pk/pointer/compat \
     -I pk/container -I pk/container/compat \
     -I pk/port -I pk/port/compat \
     -I pk/log -I pk/log/compat \
     -I pk/string \
     -I pk/test -I pk/test/compat \
     -I libs/global -I libs/image \
     -I $BOOST_INC \
     -I $BUILD -I ."
DEFS=(-DQT_NO_DEBUG)

# FORCE：三个必须在解析到对应裸标识符**之前**就位的宏/类型，光靠 -I 搜索顺序
# 覆盖不了——它们第一次以裸标识符（不经 `#include <QString>` 之类的显式
# include）出现的位置，早于任何真实调用点会主动 include 对应垫片的位置：
#   · $STUBS/QString：QString 宏与 PkDomUtilsGraftString::number()/remove()
#     的类型，kis_debug.h 的 `QString kisBacktrace();` 声明用到，而
#     kis_debug.h 全程没有一次 `#include <QString>`（同真 Qt 的传递性，
#     没人替它显式 include）。
#   · pk/geometry/compat/QTransform：kis_dom_utils.h 的
#     `saveValue(..., const QTransform&)` 声明用到，但 <QTransform> 直到
#     kis_dom_utils.**cpp** 顶部才被 include（在 .h 之后），声明当时还没有
#     宏可用。
#   · $STUBS/QVariant：kis_dom_utils.h 泛型 `loadValue<T>`
#     （`is_arithmetic<T>` 分支）裸用 QVariant，同样没有任何一处
#     `#include <QVariant>`。
FORCE=(-include "$STUBS/QString" -include pk/geometry/compat/QTransform -include "$STUBS/QVariant")

# ① 被测实现：libs/global/kis_dom_utils.cpp 原地编译，不复制不改名。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" $INC \
    -c libs/global/kis_dom_utils.cpp -o "$BUILD/kis_dom_utils.o" \
    2>"$BUILD/compile_impl.log"; then
    printf '  试接编译失败: kis_dom_utils.cpp\n'
    sed 's/^/    /' "$BUILD/compile_impl.log" | head -80
    exit 1
fi

# ①.5 kis_debug.cpp：kis_dom_utils.cpp 经 kis_debug.h 用到的 21 个
#     Q_LOGGING_CATEGORY 分类对象（`_41000()` 等）与 kisBacktrace()/
#     __methodName() 两个自由函数，真身都在这个 .cpp 里——不编它就没有这些
#     符号，链接期报 undefined reference。config-debug.h 占位把
#     HAVE_BACKTRACE 压成 0，backtrace 分支（需要 QByteArray/QLatin1String
#     等 PkString 用量表外的类型）整段被预处理器跳过，不需要真的实现。
#     同样原地编译，不属于本任务的"候选源文件"，但也是真实、未修改的 Krita
#     生产源码——不是垫片、不是重写。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" $INC \
    -c libs/global/kis_debug.cpp -o "$BUILD/kis_debug.o" \
    2>"$BUILD/compile_debug.log"; then
    printf '  试接编译失败: kis_debug.cpp\n'
    sed 's/^/    /' "$BUILD/compile_debug.log" | head -80
    exit 1
fi

# ② 生成 PK 测试 binder（替代 moc 的测试发现），直接对**真实路径**的头跑，
#    不复制。
if ! python3 pk/test/pk_test_moc.py libs/image/tests/kis_dom_utils_test.h \
    -o "$BUILD/binder.inc" 2>"$BUILD/moc.log"; then
    printf '  pk_test_moc 生成 binder 失败\n'
    sed 's/^/    /' "$BUILD/moc.log" | head -40
    exit 1
fi

# ②.5 driver.cpp 是构建期胶水（见 graft_run_a_driver.cpp 文件头注释），不是
#     从源树复制来的文件：
#       · #include 真实、未修改的 kis_dom_utils_test.cpp（零字节改动——
#         PkTestBinder<T> 的显式特化必须与 SIMPLE_TEST_MAIN 展开出的 main()
#         在同一个翻译单元里看见，测试 .cpp 自己不能改一个字节去加这行
#         include，只能反过来）；
#       · 外挂 KisTimeSpan 专属 saveValue/loadValue 两个重载的定义——真身在
#         libs/image/kis_time_span.cpp，但那个 .cpp 同一翻译单元里还有四个
#         静态方法拖着 kis_node.h/kis_layer_utils.h/kis_keyframe_channel.h
#         一整棵依赖树，与候选 A 的试接范围无关；这两个函数体逐字照抄
#         kis_time_span.cpp:95-133（真实实现，不是重新设计）。
if ! "$CXX" -std=c++17 "${DEFS[@]}" "${FORCE[@]}" $INC \
    -c "$GRAFT/graft_run_a_driver.cpp" -o "$BUILD/driver.o" \
    2>"$BUILD/compile_driver.log"; then
    printf '  试接编译失败: driver.cpp\n'
    sed 's/^/    /' "$BUILD/compile_driver.log" | head -80
    exit 1
fi

# ②.6 graft_stubs.cpp：kis_assert.h（真品）声明的四个断言函数的实现侧
#     （真身在 libs/global/kis_assert.cpp，依赖 QMessageBox/
#     KisAssertException 一整套 UI/异常设施，试接期链不进来）+
#     PkDomUtilsGraftString::number()/remove() 两个方法的实现。
if ! "$CXX" -std=c++17 "${DEFS[@]}" $INC \
    -c "$STUBS/graft_stubs.cpp" -o "$BUILD/graft_stubs.o" \
    2>"$BUILD/compile_stubs.log"; then
    printf '  试接编译失败: graft_stubs.cpp\n'
    sed 's/^/    /' "$BUILD/compile_stubs.log" | head -80
    exit 1
fi

# ③ 链接
if ! "$CXX" -std=c++17 \
    "$BUILD/driver.o" "$BUILD/kis_dom_utils.o" "$BUILD/kis_debug.o" "$BUILD/graft_stubs.o" \
    pk/xml/build/libpkxml.a pk/geometry/build/libpkgeometry.a pk/log/build/libpklog.a \
    pk/xml/build/libpktest.a pk/xml/build/libpkstring.a pk/xml/build/libpugixml.a \
    "$SPDLOG_LIB" \
    -o "$BUILD/test_kis_dom_utils" 2>"$BUILD/link.log"; then
    printf '  试接链接失败\n'
    sed 's/^/    /' "$BUILD/link.log" | head -80
    exit 1
fi

# ④ 跑
if "./$BUILD/test_kis_dom_utils" >"$BUILD/run.log" 2>&1; then
    printf '  试接跑绿: KisDomUtilsTest\n'
    grep -E '^(PASS|FAIL|Totals)' "$BUILD/run.log" | sed 's/^/    /'
else
    printf '  试接跑挂: KisDomUtilsTest\n'
    sed 's/^/    /' "$BUILD/run.log" | head -40
    rc=1
fi

# ⑤ 判据③：产物不得有 Qt 未定义符号。
undef=$(nm -u "$BUILD/test_kis_dom_utils" 2>/dev/null | grep -i qt || true)
if [ -n "$undef" ]; then
    printf '  试接产物含 Qt 符号:\n%s\n' "$undef"
    rc=1
else
    printf '    nm -u test_kis_dom_utils | grep -i qt: 无输出\n'
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
