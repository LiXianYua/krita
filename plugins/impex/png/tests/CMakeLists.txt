pk_add_test(
    kis_png_test
    SOURCES ../kis_dlg_png_import.cpp
    REGISTER_FILTERS
        registerKisPNGImportFilter
        registerKisPNGExportFilter
    LINK_LIBRARIES
        kritaimpex pktest
        kritapngimport kritapngexport
    BROKEN
    )
