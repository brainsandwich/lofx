add_executable(hello-world main.cpp)
target_link_libraries(hello-world lofx)
set_target_properties (hello-world PROPERTIES FOLDER LOFX/TESTS)