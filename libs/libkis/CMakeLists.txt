set(kritalibkis_LIB_SRCS
    Channel.cpp
    Document.cpp
    Filter.cpp
    InfoObject.cpp
    Krita.cpp
    ManagedColor.cpp
    Node.cpp
    Notifier.cpp
    Preset.cpp
    Swatch.cpp
    Resource.cpp
    Selection.cpp
    KisSelectionClipStore.cpp
    GroupLayer.cpp
    CloneLayer.cpp
    FileLayer.cpp
    FilterLayer.cpp
    FillLayer.cpp
    VectorLayer.cpp
    FilterMask.cpp
    SelectionMask.cpp
    TransparencyMask.cpp
    TransformMask.cpp
    ColorizeMask.cpp

    Shape.cpp
    GroupShape.cpp

    LibKisUtils.cpp

    GuidesConfig.cpp
    GridConfig.cpp
)

kis_add_library(kritalibkis SHARED ${kritalibkis_LIB_SRCS})
generate_export_header(kritalibkis)

target_link_libraries(kritalibkis
    PRIVATE
        KF${KF_MAJOR}::ConfigCore
        kritaanimation
        kritaimpex
        kritaversion
    PUBLIC
        kritacanvas
        kritaimage
)

set_target_properties(kritalibkis PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritalibkis  ${INSTALL_TARGETS_DEFAULT_ARGS})


add_subdirectory(tests)
