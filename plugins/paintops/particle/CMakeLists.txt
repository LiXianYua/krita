set(kritaparticlepaintop_SOURCES
    particle_paintop_plugin.cpp
    kis_particle_paintop.cpp
    kis_particle_paintop_settings.cpp
    particle_brush.cpp
    KisParticleOpOptionData.cpp
    )

kis_add_library(kritaparticlepaintop MODULE ${kritaparticlepaintop_SOURCES})

target_link_libraries(kritaparticlepaintop kritalibpaintop)

install(TARGETS kritaparticlepaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES  krita-particle.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)


