# tropolink_core: pure C++20, no Qt UI types, unit-typed quantities throughout.

add_library(tropolink_core STATIC
    common/sha256.cpp
    geo/geodesy.cpp
    geo/coords.cpp
    geo/atmosphere.cpp
    terrain/dem_dataset.cpp
    terrain/terrain_store.cpp
    terrain/profile.cpp
    terrain/horizon.cpp
    tropo/scatter_geometry.cpp
    tropo/statistics.cpp
    tropo/loss_model.cpp
    tropo/p617.cpp
    tropo/tn101.cpp
    tropo/itm_model.cpp
    tropo/model_suite.cpp
    budget/modulation.cpp
    budget/link_budget.cpp
    budget/availability.cpp
    budget/solver.cpp
    budget/auto_design.cpp
    project/project.cpp
    project/kml_export.cpp
    project/csv_export.cpp
    report/report_content.cpp
)

target_include_directories(tropolink_core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(tropolink_core
    PUBLIC ntia_itm
    PRIVATE GDAL::GDAL GeographicLib::GeographicLib nlohmann_json::nlohmann_json libzip::zip
)
target_compile_definitions(tropolink_core PUBLIC
    TROPOLINK_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${TROPOLINK_AIRGAP}>:TROPOLINK_AIRGAP=1>
)

if(NOT TROPOLINK_AIRGAP)
    target_sources(tropolink_core PRIVATE ai/ollama_client.cpp)
    if(WIN32)
        target_link_libraries(tropolink_core PRIVATE ws2_32)
    endif()
endif()

tropolink_set_warnings(tropolink_core)
