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

add_subdirectory(third_party/ntia_itm)
add_subdirectory(core)

if(TROPOLINK_BUILD_UI)
    add_subdirectory(ui)
    add_subdirectory(app)
endif()

if(TROPOLINK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
