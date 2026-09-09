function(karbon_embed_calligraphy_resource input_png template_cpp output_cpp)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_png}")
    file(READ "${input_png}" KARBON_CALLIGRAPHY_PNG_HEX HEX)
    file(SIZE "${input_png}" KARBON_CALLIGRAPHY_PNG_SIZE)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," KARBON_CALLIGRAPHY_PNG_BYTES
        "${KARBON_CALLIGRAPHY_PNG_HEX}")
    configure_file("${template_cpp}" "${output_cpp}" @ONLY)
endfunction()
