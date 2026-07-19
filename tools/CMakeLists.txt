# Operator utilities.
add_executable(make_dted make_dted.cpp)
target_link_libraries(make_dted PRIVATE GDAL::GDAL)
