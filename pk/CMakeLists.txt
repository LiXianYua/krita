# pk 库接入主构建树。
#
# 每个 pk/*/CMakeLists.txt 是独立薄壳工程（有 project() + cmake_minimum_required），
# 内部有无条件 add_subdirectory(pk/test) 调用，在同一份 configure 里被多个目录
# add_subdirectory 会导致冲突。所以本文件**不委托**给各 pk/*/CMakeLists.txt，
# 而是直接定义各 pk 库的 target，复刻各 CMakeLists.txt 的库目标定义（只取库目标，
# 不取测试目标、不重复 add_subdirectory pk/test）。
#
# 依赖拓扑序（顶层 pk/* 目录，按此顺序构建）：
#   1. container（无 pk 依赖）
#   2. string（include 依赖 container）
#   3. geometry（include 依赖 container）
#   4. flags（无 pk 依赖，INTERFACE）
#   5. concurrent（无 pk 依赖，Threads::Threads）
#   6. log（无 pk 依赖，spdlog::spdlog）
#   7. time（无 pk 依赖）
#   8. config（依赖 string）
#   9. pointer（无 pk 依赖）
#  10. xml（依赖 string，内联 PkStream.cpp，pugixml）
#  11. color + variant（color 依赖 string；variant 依赖 string、geometry、time、port、color；
#      port target 在后续段定义，CMake 按 target link edge 建立真实构建顺序）
#  12. port（依赖 minizip-ng FetchContent，内联 PkZipArchive.cpp + pk/string .cpp）
#  13. signal（依赖 concurrent，Python3）
#  14. sql（依赖 variant、geometry、string，SQLite3）
#  15. test（依赖 string，Python3——仅库目标，不生成测试 binder）

find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# =========================================================================
# 1. pk/container
# =========================================================================
add_library(pkcontainer STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkArrayData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkVector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkHash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkStack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/container/PkQueue.cpp
)
target_include_directories(pkcontainer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/container)

# =========================================================================
# 2. pk/string
# =========================================================================
add_library(pkstring STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkStringCodec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkString_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkString_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkString_format.cpp
)
target_include_directories(pkstring PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/string
    ${CMAKE_CURRENT_SOURCE_DIR}/container
)

# =========================================================================
# 3. pk/geometry
#    注意：PkPolygon.h 继承 PkVector<PkPoint>（pk/container），
#    PkVector<T> 是模板头文件，需要 include 路径。
# =========================================================================
add_library(pkgeometry STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/global/PkGlobal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkPoint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkSize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkRect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkTransform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkLine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkMargins.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkPolygon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkVectorND.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkMatrix4x4.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkRegion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/PkPainterPath.cpp
)
target_include_directories(pkgeometry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/container
    ${CMAKE_CURRENT_SOURCE_DIR}/global
)
target_compile_options(pkgeometry PUBLIC -fwrapv)

# =========================================================================
# 4. pk/flags（INTERFACE 库，纯头文件）
# =========================================================================
add_library(pkflags INTERFACE)
target_include_directories(pkflags INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/flags)

# =========================================================================
# 5. pk/concurrent
# =========================================================================
add_library(pkconcurrent STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/concurrent/PkThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/concurrent/PkThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/concurrent/PkSemaphore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/concurrent/PkThreadCallQueue.cpp
)
target_include_directories(pkconcurrent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/concurrent)
target_link_libraries(pkconcurrent PUBLIC Threads::Threads)

# =========================================================================
# 6. pk/log
# =========================================================================
find_package(spdlog QUIET)
if (NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        79524ddd08a4ec981b7fea76afd08ee05f83755d
        GIT_SHALLOW    FALSE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL       OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

add_library(pklog STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/log/PkLogSink.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/log/PkLogBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/log/PkDebug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/log/PkLoggingCategory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/log/PkMessageLogger.cpp
)
target_include_directories(pklog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/log)
target_link_libraries(pklog PRIVATE spdlog::spdlog)

# =========================================================================
# 7. pk/time
# =========================================================================
add_library(pktime STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/time/PkElapsedTimer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/time/PkDateTime.cpp
)
target_include_directories(pktime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/time)

# =========================================================================
# 8. pk/config
# =========================================================================
add_library(pkconfig STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config/PkConfigStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config/PkConfigGroup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config/PkSharedConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config/PkMimeDatabase.cpp
)
target_include_directories(pkconfig PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/string
    ${CMAKE_CURRENT_SOURCE_DIR}/container
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry
)
target_link_libraries(pkconfig PUBLIC pkstring)

# =========================================================================
# 9. pk/pointer
# =========================================================================
add_library(pkpointer STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/pointer/PkPointer.cpp
)
target_include_directories(pkpointer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pointer)

# =========================================================================
# 10. pk/xml（依赖 pugixml + 内联 PkStream.cpp）
# =========================================================================
find_package(pugixml QUIET)
if (NOT pugixml_FOUND)
    include(FetchContent)
    FetchContent_Declare(pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        c8033ce9d039e7f9d134877c363397b3cfe20816
        GIT_SHALLOW    FALSE)
    set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(pugixml)
endif()

set(PKPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/port)

add_library(pkxml STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/xml/PkXmlNode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xml/PkXmlDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xml/PkXmlElement.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xml/PkXmlOffsetUtil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xml/PkXmlStreamReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xml/PkXmlStreamWriter.cpp
    ${PKPORT_DIR}/PkStream.cpp
)
target_include_directories(pkxml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/xml
    ${PKPORT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/string
)
target_link_libraries(pkxml PUBLIC pugixml)

# =========================================================================
# 11. pk/color + pk/variant（variant 依赖 pkport + pkcolor）
# =========================================================================
add_library(pkcolor STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/color/PkColor.cpp
)
target_include_directories(pkcolor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/color
    ${CMAKE_CURRENT_SOURCE_DIR}/global
    ${CMAKE_CURRENT_SOURCE_DIR}/namespace
    ${CMAKE_CURRENT_SOURCE_DIR}/string
    ${CMAKE_CURRENT_SOURCE_DIR}/container
)
target_link_libraries(pkcolor PUBLIC pkstring)

add_library(pkvariant STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/variant/PkAuxTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/variant/PkDataStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/variant/PkDataStreamColor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/variant/PkVariant.cpp
)
target_include_directories(pkvariant PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/variant
    ${CMAKE_CURRENT_SOURCE_DIR}/container
    ${CMAKE_CURRENT_SOURCE_DIR}/time
    ${CMAKE_CURRENT_SOURCE_DIR}/port
    ${CMAKE_CURRENT_SOURCE_DIR}/color
)
target_compile_options(pkvariant PUBLIC -fwrapv)
target_link_libraries(pkvariant PUBLIC pkstring pkgeometry pktime pkport pkcolor)

# =========================================================================
# 12. pk/port/zip（minizip-ng FetchContent）—— PkZipArchive.cpp 内联到 pkport
#     （不建独立 pkzip 目标，避免 STATIC 库链接排序问题）。
# =========================================================================
include(FetchContent)
find_package(minizip-ng QUIET)
if (NOT minizip-ng_FOUND)
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
    FetchContent_Declare(minizip-ng
        GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng
        GIT_TAG 4.0.7
    )
    FetchContent_MakeAvailable(minizip-ng)
    # minizip-ng 的符号被父项目的 -fvisibility=hidden 隐藏（mz_zip_* 都是 t 而非 T）。
    # 恢复默认可见性，让 pkport 和消费者能链接到这些符号。
    if (TARGET minizip-ng)
        set_target_properties(minizip-ng PROPERTIES
            C_VISIBILITY_PRESET default
            CXX_VISIBILITY_PRESET default)
    endif()
endif()

# =========================================================================
# 13. pk/port（依赖 pkstring + minizip-ng）
#     PkZipArchive.cpp 直接编入 pkport，避免 STATIC 库链接排序问题。
# =========================================================================
add_library(pkport SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/port/PkStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/port/PkEventSink.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/port/PkResourceStorage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/port/PkFontProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/port/zip/PkZipArchive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkStringCodec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkString_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkString_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/string/PkString_format.cpp
)
target_include_directories(pkport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/port
    ${CMAKE_CURRENT_SOURCE_DIR}/port/zip
    ${CMAKE_CURRENT_SOURCE_DIR}/string
    ${CMAKE_CURRENT_SOURCE_DIR}/container
)
target_link_libraries(pkport PUBLIC minizip-ng)
# 全局 CMAKE_CXX_FLAGS 含 -fvisibility=hidden，覆盖 target 属性。
# 显式加 -fvisibility=default 覆盖全局隐藏设置。
target_compile_options(pkport PRIVATE -fvisibility=default)

# =========================================================================
# 14. pk/signal（依赖 pkconcurrent + Python3 pk_signal_moc.py）
# =========================================================================
add_library(pksignal STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/signal/PkObject.cpp
)
target_include_directories(pksignal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/signal)
target_link_libraries(pksignal PUBLIC pkconcurrent)

# =========================================================================
# 15. pk/sql（依赖 SQLite3 + pkvariant + pkgeometry + pkstring）
# =========================================================================
find_package(SQLite3 QUIET)
if (NOT SQLite3_FOUND)
    include(FetchContent)
    FetchContent_Declare(sqlite3_amalgamation
        URL https://sqlite.org/2025/sqlite-amalgamation-3500400.zip
        URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032)
    FetchContent_MakeAvailable(sqlite3_amalgamation)
    add_library(sqlite3_vendored STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3_vendored PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
    add_library(SQLite::SQLite3 ALIAS sqlite3_vendored)
endif()

# pk/sql 依赖已定义的 pkvariant、pkgeometry、pkstring target。
set(PKCONTAINER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/container)

add_library(pksql STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sql/PkSqlError.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sql/PkSqlDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sql/PkSqlQuery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sql/PkSqlCursor.cpp
)
target_include_directories(pksql PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sql
    ${PKCONTAINER_DIR}
)
target_link_libraries(pksql PUBLIC SQLite::SQLite3 pkvariant pkgeometry pkstring)

# =========================================================================
# 16. pk/test（仅库目标，不生成测试 binder）
# =========================================================================
add_library(pktest STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/test/PkTestCase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/PkTestRunner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/PkTestCompare.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/PkTestData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/PkTestDataString.cpp
)
target_include_directories(pktest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/test)
target_link_libraries(pktest PUBLIC pkstring)
