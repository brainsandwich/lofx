add_executable(terrain main.cpp)

target_include_directories(terrain PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(terrain lofx)
set_target_properties (terrain PROPERTIES FOLDER lofx/tests)