cmake_minimum_required(VERSION 3.24)

project(TropoLink
    VERSION 1.0.0
    DESCRIPTION "Troposcatter link design instrument"
    LANGUAGES CXX)

option(TROPOLINK_AIRGAP "Air-gap flavor: downloader and AI commentary compiled out entirely" OFF)
option(TROPOLINK_BUILD_UI "Build the Qt Quick user interface" ON)
option(TROPOLINK_BUILD_TESTS "Build the GoogleTest suite" ON)
option(TROPOLINK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TROPOLINK_SANITIZE "Enable AddressSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include(cmake/CompilerWarnings.cmake)

# All dependency lookups happen at the top level so every subdirectory scope sees
# the imported targets (required by Qt's target finalizers).
find_package(GDAL CONFIG REQUIRED)
find_package(GeographicLib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
if(TROPOLINK_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
endif()
if(TROPOLINK_BUILD_UI)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Quick QuickControls2 Svg LinguistTools)
    if(NOT TROPOLINK_AIRGAP)
        find_package(Qt6 REQUIRED COMPONENTS Network)
    endif()
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    find_package(ZLIB REQUIRED)
    qt_standard_project_setup(REQUIRES 6.7 I18N_TRANSLATED_LANGUAGES pl)
endif()

add_subdirectory(third_party/ntia_itm)
add_subdirectory(core)
add_subdirectory(tools)

if(TROPOLINK_BUILD_UI)
    add_subdirectory(ui)
    add_subdirectory(app)
endif()

if(TROPOLINK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
