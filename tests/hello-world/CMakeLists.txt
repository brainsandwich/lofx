add_executable(hello-world main.cpp)
target_link_libraries(hello-world lofx)
set_target_properties (lofxtest PROPERTIES FOLDER LOFX/TESTS)