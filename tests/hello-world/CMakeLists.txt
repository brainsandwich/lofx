add_executable(hello-world
	main.cpp
	lodepng/lodepng.cpp)

target_include_directories(hello-world PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(hello-world lofx)
set_target_properties (hello-world PROPERTIES FOLDER lofx/tests)