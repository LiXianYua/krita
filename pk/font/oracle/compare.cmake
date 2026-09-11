# pk/font/oracle 逐像素对拍：Qt 侧（ORACLE）vs native 侧（NATIVE）。
#
# 平台条件化（人 2026-09-12 裁决，见 plan §3.3）：
#   文字类对拍的参照系本身依赖平台——Qt 在 Linux 走 FreeType、在 macOS 走
#   CoreText。本机 Qt 没有 FreeType 文字引擎，两侧永远不可能逐像素相等，
#   所以这条判据在本机是「不可运行」，不是「被改红了」。
#   于是在跑比较之前先做一次 **实测的能力探测**：数 ORACLE 真正链接的那份
#   QtGui 里有多少个 QFontEngineFT 符号。
#
#     >0           → Qt 走 FreeType，参照系成立：照旧真跑两侧比较
#                    （行为与条件化之前逐字节一致）。
#     ==0 且分母 >0 → 正面证据说明 Qt 无 FreeType 引擎：打印 SKIP、退出 0、
#                    **不写任何产出**（在任何 execute_process 之前 return）。
#     探测不到     → 找不到 QtGui 或 nm 没读到东西：探测无效，**不跳过**，
#                    照旧真跑比较。宁可在本机红着，也不把「探测失败」当成
#                    「无引擎」——那会让带 FreeType 的宿主假装通过。
#
# 打印的全是实测数字（QtGui 路径、nm 行数、符号计数）。这里没有一处硬编码
# 「这是 macOS 所以跳过」——跳过与否只由 QFontEngineFT 的实测计数决定。
#
# --- QtGui 怎么定位：两条路，为什么选这条 --------------------------------
#   路 A：对 ORACLE 二进制跑 `otool -L`（macOS）/ `ldd`（Linux），反查它
#         链接的 QtGui。**选它当主路**：探测要问的正是「ORACLE 用的那份
#         QtGui 有没有 FreeType 引擎」，对着 ORACLE 本身反查最贴近这个语义；
#         ORACLE 此刻必然存在（是 CMake 从 target 传进来的路径），不必猜
#         prefix，也不依赖 CMAKE_PREFIX_PATH 在测试运行时还在不在环境里。
#         代价：macOS 上 Qt 以 framework 形式安装，otool 给的是
#         `@rpath/QtGui.framework/Versions/5/QtGui`，得再拼回候选根。
#   路 B：在 CMAKE_PREFIX_PATH 下 glob 出 QtGui 库文件。**当兜底**：
#         otool/ldd 不是每个宿主都有，glob 只用 CMake 自带的 file(GLOB)。
#         代价：prefix 下可能有多份 Qt，容易选错。
#   两条路产出的候选路径合成一张表，取第一个 EXIST 的。路 A 若直接给绝对
#   路径（Linux 常见）就原样用，是 `@rpath/...` 才去前缀后拼候选根。

# 候选根：CMAKE_PREFIX_PATH 的每一项，以及每项下的 /lib（framework 与
# libQt5Gui.* 都住在 <prefix>/lib）。
set(PK_QT_ROOTS "")
set(_pk_prefixes "$ENV{CMAKE_PREFIX_PATH}")
if(NOT _pk_prefixes)
    set(_pk_prefixes "/usr/local;/usr")
endif()
string(REPLACE ":" ";" _pk_prefixes "${_pk_prefixes}")
foreach(_pk_prefix IN LISTS _pk_prefixes)
    list(APPEND PK_QT_ROOTS "${_pk_prefix}/lib" "${_pk_prefix}")
endforeach()

set(PK_QT_CANDIDATES "")

# 路 A：反查 ORACLE 链接的 QtGui。
find_program(PK_OTOOL otool)
find_program(PK_LDD ldd)
set(_pk_linked "")
if(PK_OTOOL AND EXISTS "${ORACLE}")
    execute_process(COMMAND "${PK_OTOOL}" -L "${ORACLE}"
        OUTPUT_VARIABLE _pk_linked RESULT_VARIABLE _pk_rc ERROR_QUIET)
elseif(PK_LDD AND EXISTS "${ORACLE}")
    execute_process(COMMAND "${PK_LDD}" "${ORACLE}"
        OUTPUT_VARIABLE _pk_linked RESULT_VARIABLE _pk_rc ERROR_QUIET)
endif()
if(_pk_linked)
    string(REPLACE "\n" ";" _pk_linked_lines "${_pk_linked}")
    foreach(_pk_line IN LISTS _pk_linked_lines)
        if(_pk_line MATCHES "QtGui")
            string(REGEX MATCH "[^ \t]+QtGui[^ \t]*" _pk_hit "${_pk_line}")
            if(_pk_hit)
                string(REPLACE "@rpath/" "" _pk_rel "${_pk_hit}")
                if(IS_ABSOLUTE "${_pk_rel}")
                    list(APPEND PK_QT_CANDIDATES "${_pk_rel}")
                else()
                    foreach(_pk_root IN LISTS PK_QT_ROOTS)
                        list(APPEND PK_QT_CANDIDATES "${_pk_root}/${_pk_rel}")
                    endforeach()
                endif()
            endif()
        endif()
    endforeach()
endif()

# 路 B：在候选根下 glob QtGui（framework 与 .so/.dylib 两种形态）。
foreach(_pk_root IN LISTS PK_QT_ROOTS)
    file(GLOB _pk_g_fw "${_pk_root}/QtGui.framework/Versions/*/QtGui")
    file(GLOB _pk_g_lib "${_pk_root}/libQt5Gui.so*"
                        "${_pk_root}/libQt5Gui.*.dylib"
                        "${_pk_root}/libQt5Gui.dylib")
    list(APPEND PK_QT_CANDIDATES ${_pk_g_fw} ${_pk_g_lib})
endforeach()

set(PK_QT_GUI "")
foreach(_pk_cand IN LISTS PK_QT_CANDIDATES)
    if(EXISTS "${_pk_cand}")
        set(PK_QT_GUI "${_pk_cand}")
        break()
    endif()
endforeach()

# 能力探测：数 QFontEngineFT 符号。分母（nm 行数）一并记下——它是判别力对照
# 的一半：QFontEngineMulti 计数非零说明 nm 确实看得见字体引擎类，0 不是
# 「nm 读了个空文件」的假阴性。
find_program(PK_NM nm)
set(PK_FT_COUNT -1)
set(PK_MULTI_COUNT -1)
set(PK_NM_LINES 0)
if(PK_QT_GUI AND PK_NM)
    execute_process(COMMAND "${PK_NM}" -a "${PK_QT_GUI}"
        OUTPUT_VARIABLE _pk_nm_out RESULT_VARIABLE _pk_nm_rc ERROR_QUIET)
    if(_pk_nm_rc EQUAL 0 AND _pk_nm_out)
        string(REPLACE "\n" ";" _pk_nm_lines "${_pk_nm_out}")
        list(REMOVE_ITEM _pk_nm_lines "")
        list(LENGTH _pk_nm_lines PK_NM_LINES)
        string(REGEX MATCHALL "[^\n]*QFontEngineFT[^\n]*" _pk_ft_hits "${_pk_nm_out}")
        list(LENGTH _pk_ft_hits PK_FT_COUNT)
        string(REGEX MATCHALL "[^\n]*QFontEngineMulti[^\n]*" _pk_multi_hits "${_pk_nm_out}")
        list(LENGTH _pk_multi_hits PK_MULTI_COUNT)
    endif()
endif()

message(STATUS "font oracle probe: QtGui=${PK_QT_GUI} nm_lines=${PK_NM_LINES} QFontEngineFT=${PK_FT_COUNT} QFontEngineMulti=${PK_MULTI_COUNT}")

# SKIP 只建立在**正面证据**上：定位到 QtGui、nm 成功、读到非空符号表、且
# QFontEngineFT 计数为 0。三个条件缺一，探测就是无效的，照旧往下真跑比较。
if(PK_FT_COUNT EQUAL 0 AND PK_NM_LINES GREATER 0)
    message("SKIP: Qt has no FreeType text engine (${PK_FT_COUNT})")
    return()
endif()

execute_process(COMMAND "${ORACLE}" --pixels "${OUTPUT_DIR}/qt-pixels.bin" RESULT_VARIABLE oracle_status
    OUTPUT_FILE "${OUTPUT_DIR}/qt-pixels.tsv" ERROR_FILE "${OUTPUT_DIR}/qt-stderr.log")
execute_process(COMMAND "${NATIVE}" --pixels "${OUTPUT_DIR}/native-pixels.bin" RESULT_VARIABLE native_status
    OUTPUT_FILE "${OUTPUT_DIR}/native-pixels.tsv" ERROR_FILE "${OUTPUT_DIR}/native-stderr.log")
if(NOT oracle_status EQUAL 0 OR NOT native_status EQUAL 0)
    message(FATAL_ERROR "Font oracle failed: Qt=${oracle_status}; native=${native_status}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${OUTPUT_DIR}/qt-pixels.bin" "${OUTPUT_DIR}/native-pixels.bin"
    RESULT_VARIABLE raw_status)
if(NOT raw_status EQUAL 0)
    message(FATAL_ERROR "Font raw pixels differ: compare qt-pixels.bin with native-pixels.bin")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${OUTPUT_DIR}/qt-pixels.tsv" "${OUTPUT_DIR}/native-pixels.tsv"
    RESULT_VARIABLE comparison_status)
if(NOT comparison_status EQUAL 0)
    message(FATAL_ERROR "Font pixels differ: compare qt-pixels.tsv with native-pixels.tsv")
endif()
