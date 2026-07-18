# NTIA Irregular Terrain Model (Longley-Rice) v1.x — public domain (US Government work).
# Vendored unmodified from https://github.com/NTIA/itm. See LICENSE.md and docs/third_party.md.

file(GLOB NTIA_ITM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(ntia_itm STATIC ${NTIA_ITM_SOURCES})
target_include_directories(ntia_itm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Third-party code: compiled quietly, never with -Werror.
if(MSVC)
    target_compile_options(ntia_itm PRIVATE /W0)
else()
    target_compile_options(ntia_itm PRIVATE -w)
endif()
