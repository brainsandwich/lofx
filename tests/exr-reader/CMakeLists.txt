add_executable(exr-reader main.cpp lodepng/lodepng.cpp)

target_include_directories(exr-reader PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(exr-reader lofx)
set_target_properties (exr-reader PROPERTIES FOLDER lofx/tests)