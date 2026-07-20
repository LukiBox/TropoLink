# Operator utilities.
add_executable(make_dted make_dted.cpp)
target_link_libraries(make_dted PRIVATE GDAL::GDAL)

# Development aid: offscreen paper-topo tile preview (shares the map sources
# translation unit; not deployed).
if(TROPOLINK_BUILD_UI)
    qt_add_executable(render_topo_preview
        render_topo_preview.cpp
        ${CMAKE_SOURCE_DIR}/ui/map/MapSources.cpp
        ${CMAKE_SOURCE_DIR}/ui/map/MapSources.h
    )
    set_target_properties(render_topo_preview PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    target_include_directories(render_topo_preview PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(render_topo_preview PRIVATE
        tropolink_core Qt6::Gui unofficial::sqlite3::sqlite3)
    if(NOT TROPOLINK_AIRGAP)
        target_link_libraries(render_topo_preview PRIVATE Qt6::Network)
    endif()
endif()
