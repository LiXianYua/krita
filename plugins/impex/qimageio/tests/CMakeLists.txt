pk_add_test(
    KisQImageIOTest
    REGISTER_FILTERS
        registerKisQImageIOImportFilter
        registerKisQImageIOExportFilter
    LINK_LIBRARIES
        kritaimpex
        kritaqimageioimport kritaqimageioexport
    )
