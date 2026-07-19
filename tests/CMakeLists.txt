add_executable(tropolink_tests
    test_units.cpp
    test_geodesy.cpp
    test_coords.cpp
    test_atmosphere.cpp
    test_terrain.cpp
    test_tropo.cpp
    test_budget.cpp
    test_project.cpp
)

target_link_libraries(tropolink_tests PRIVATE tropolink_core GTest::gtest GTest::gtest_main GDAL::GDAL)
tropolink_set_warnings(tropolink_tests)

# Test fixtures: synthetic DEMs are generated at test runtime into a temp dir; the ITU
# refractivity maps are read from the source tree.
target_compile_definitions(tropolink_tests PRIVATE
    TROPOLINK_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/resources/data"
)

# Single ctest entry running the whole suite. Per-test discovery
# (gtest_discover_tests) writes a temp JSON through gtest's ANSI fopen, which breaks
# under non-ASCII build paths on Windows.
add_test(NAME tropolink_tests COMMAND tropolink_tests)
