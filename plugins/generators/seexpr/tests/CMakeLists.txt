kis_add_tests(
    kis_seexpr_generator_test.cpp

    LINK_LIBRARIES kritaimage kritatestsdk pktest pkuuid
)

# --whole-archive / --no-whole-archive 是 GNU ld 的说法，Mach-O 的 ld64 不认。
# 语义上它是「强制把归档里全部成员拉进来，包括没被引用的」，Mach-O 的对应物是
# -force_load,<归档绝对路径>——**不能置空**：置空会让「静态全局构造自注册」的对象
# 被链接器丢弃、构造函数不运行，静默改变行为。
#
# 关键约束：**归档仍必须以 target 名出现在 target_link_libraries 里**，否则
# CMake 不再传播该 target 的 INTERFACE 链接依赖（实测：写成裸 $<TARGET_FILE:> 后
# 本 target 丢了 libKSeExpr/liblcms2 两个依赖符号）。所以形制是：target 名留在
# 原位，Apple 侧额外用 target_link_options 施加 -force_load，Linux 侧用
# --whole-archive/--no-whole-archive 夹住这些 target 名。
if(APPLE)
    set(KRITA_SEEXPR_WHOLE_ARCHIVE_BEGIN "")
    set(KRITA_SEEXPR_WHOLE_ARCHIVE_END "")
    set(KRITA_SEEXPR_WHOLE_ARCHIVE_OPTIONS
        "-Wl,-force_load,$<TARGET_FILE:kritaseexprgenerator>"
        "-Wl,-force_load,$<TARGET_FILE:kritalcmsengine>")
else()
    set(KRITA_SEEXPR_WHOLE_ARCHIVE_BEGIN "-Wl,--whole-archive")
    set(KRITA_SEEXPR_WHOLE_ARCHIVE_END "-Wl,--no-whole-archive")
    set(KRITA_SEEXPR_WHOLE_ARCHIVE_OPTIONS "")
endif()

target_link_libraries(kis_seexpr_generator_test
    ${KRITA_SEEXPR_WHOLE_ARCHIVE_BEGIN}
    kritaseexprgenerator
    kritalcmsengine
    ${KRITA_SEEXPR_WHOLE_ARCHIVE_END})
target_link_options(kis_seexpr_generator_test PRIVATE ${KRITA_SEEXPR_WHOLE_ARCHIVE_OPTIONS})
