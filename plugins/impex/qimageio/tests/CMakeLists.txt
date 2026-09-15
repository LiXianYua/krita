pk_add_test(
    KisQImageIOTest
    REGISTER_FILTERS
        registerKisQImageIOImportFilter
        registerKisQImageIOExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritaqimageioimport kritaqimageioexport
        kritalcmsengine
    )
