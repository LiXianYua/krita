set(kritapaintkernel_LIB_SRCS
    KisPaintKernelStageMarker.cpp
)

kis_add_library(paintkernel SHARED ${kritapaintkernel_LIB_SRCS})
generate_export_header(paintkernel BASE_NAME paintkernel)

target_link_libraries(paintkernel
    PUBLIC
    kritaversion
    kritaglobal
    kritaplugin
    kritastore
    kritaflake
    kritapigment
    kritacommand
    kritalibbrush
    kritapsdutils
    kritacolor
    kritaimage
    kritacanvas
    kritainput
    kritaanimation
    kritaimpex
    kritaresources
    kritametadata
    kritapsd
    kritamultiarch
    kritasurfacecolormanagementapi
)

# --no-as-needed 是 ELF 链接器的说法。Mach-O 默认就把命令行上列出的每个 dylib
# 都记进 LC_LOAD_DYLIB（没有 ELF 的 --as-needed 语义），所以这个开关在 Apple
# 工具链上既无效（clang 报 `unknown options`）也不需要——按平台置空即可。
# 先例：libs/image/tests/CMakeLists.txt（S-10）。
if(APPLE)
    set(KRITA_PAINTKERNEL_NO_AS_NEEDED "")
else()
    set(KRITA_PAINTKERNEL_NO_AS_NEEDED "-Wl,--no-as-needed")
endif()

target_link_options(paintkernel PRIVATE ${KRITA_PAINTKERNEL_NO_AS_NEEDED})

set_target_properties(paintkernel PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS paintkernel ${INSTALL_TARGETS_DEFAULT_ARGS})
