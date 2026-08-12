# Q-5（docs/Qt替代品选型.md）：zip 从 QuaZip 换成 minizip-ng。
#
# 依赖获取口径已裁决（task-6-brief.md「依赖获取」）：先 find_package 系统安装，
# 未命中则 FetchContent、tag 钉死。本机实测（task-6-report.md「find_package
# 未命中 / FetchContent 生效的原始输出」）：minizip-ng 与 minizip 的 dev 包都
# 没装，find_package 不命中，实际走 FetchContent；网络通
# （codeload.github.com 对 refs/tags/4.0.7 返回 HTTP 200）；zlib 1.3 dev 已
# 就位（ZLIB_FOUND=TRUE）。
include(FetchContent)

find_package(minizip-ng QUIET)
if (NOT minizip-ng_FOUND)
    # 关掉本任务能力面用不到的编解码/加密/第三方依赖开关，缩小 FetchContent
    # 拉起的构建面、避免不必要的外部依赖探测失败：
    #   MZ_COMPAT 关——只用 mz_zip.h/mz_zip_rw.h 的现代 API，不需要 1.x 兼容层。
    #   MZ_ZLIB 开——唯一需要的压缩算法（KoQuaZip 的两档压缩级别都基于
    #     Z_DEFLATED，见 PkZipArchive.cpp 的 openEntryForWrite() 注释）。
    #   MZ_BZIP2/MZ_LZMA/MZ_ZSTD/MZ_LIBCOMP 关——能力面表里没有这些编解码器。
    #   MZ_PKCRYPT/MZ_WZAES/MZ_OPENSSL 关——「加密不实现」（setPassword 实测
    #     0 处调用），顺带避免依赖 OpenSSL；同时让 minizip-ng 编译期定义
    #     MZ_ZIP_NO_CRYPTO，条目关闭时不会跑用不到的 SHA 哈希计算。
    #   MZ_ICONV 关——文件名编码固定 UTF-8，不需要额外的编码转换层。
    #   MZ_FETCH_LIBS 关——不允许它在找不到依赖时又反过来递归拉子依赖；本任务
    #     已经把用不到的编解码器全关掉，不会触发这条路径。
    #   MZ_BUILD_*_TESTS/MZ_CODE_COVERAGE 关——只要库，不要 minizip-ng 自己的
    #     测试/覆盖率产物拖进构建图。
    set(MZ_COMPAT OFF CACHE BOOL "" FORCE)
    set(MZ_ZLIB ON CACHE BOOL "" FORCE)
    set(MZ_BZIP2 OFF CACHE BOOL "" FORCE)
    set(MZ_LZMA OFF CACHE BOOL "" FORCE)
    set(MZ_ZSTD OFF CACHE BOOL "" FORCE)
    set(MZ_LIBCOMP OFF CACHE BOOL "" FORCE)
    set(MZ_PKCRYPT OFF CACHE BOOL "" FORCE)
    set(MZ_WZAES OFF CACHE BOOL "" FORCE)
    set(MZ_OPENSSL OFF CACHE BOOL "" FORCE)
    set(MZ_ICONV OFF CACHE BOOL "" FORCE)
    set(MZ_FETCH_LIBS OFF CACHE BOOL "" FORCE)
    set(MZ_FORCE_FETCH_LIBS OFF CACHE BOOL "" FORCE)
    set(MZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(MZ_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(MZ_BUILD_FUZZ_TESTS OFF CACHE BOOL "" FORCE)
    set(MZ_CODE_COVERAGE OFF CACHE BOOL "" FORCE)

    # 钉死原因见任务报告「钉的 tag 与理由」——不用分支名，避免上游推新提交
    # 让本任务的构建结果不可重现。
    FetchContent_Declare(minizip-ng
        GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng
        GIT_TAG 4.0.7
    )
    FetchContent_MakeAvailable(minizip-ng)
endif()

add_library(pkzip STATIC PkZipArchive.cpp)
target_include_directories(pkzip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# PkZipArchive.cpp 用 #include "../PkStream.h"（相对路径，同 pk/port 现有
# 头文件之间的引用习惯），不需要额外的 include 目录；PkString.h 在 pk/string
# 那条搜索路径下，由上级 pk/port/CMakeLists.txt 通过 PUBLIC
# target_include_directories(pkport ... ${PK_STRING_DIR}) 提供，这里显式
# 补一份 PRIVATE，避免 pkzip 反过来依赖 pkport 的 target 属性传播顺序。
target_include_directories(pkzip PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../string)

# 零 Qt：minizip-ng 是纯 C 库，不引入任何 Qt 符号，链它不影响
# pk/port/tests/run_tests.sh 的 `nm -u libpkport.a | grep -i qt` 判据
# （libpkzip.a/libminizip.a 是独立的静态库，不在 libpkport.a 里）。
target_link_libraries(pkzip PUBLIC MINIZIP::minizip)
