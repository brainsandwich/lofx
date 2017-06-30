cmake_minimum_required(VERSION 3.8)
project(LOFX VERSION 0.1.0 LANGUAGES CXX)
cmake_policy(SET CMP0048 NEW)

set(LOFX_EXTERNALS_DIR ${PROJECT_SOURCE_DIR}/external)

# GL3W
execute_process(
	COMMAND python ${LOFX_EXTERNALS_DIR}/gl3w/gl3w_gen.py
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# LOFX SOURCES
file(GLOB_RECURSE LOFX_SOURCES "${PROJECT_SOURCE_DIR}/src/**.c*")
file(GLOB_RECURSE LOFX_INCLUDES "${PROJECT_SOURCE_DIR}/include/**.h*")
set(LOFX_INCLUDEDIR ${PROJECT_SOURCE_DIR}/include)

# LOFX EXECUTABLE
add_library(lofx ${LOFX_SOURCES} ${LOFX_INCLUDES})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties (lofx PROPERTIES FOLDER LOFX/CORE)
target_include_directories(lofx PUBLIC ${LOFX_INCLUDEDIR})

# FETCH DEPENDENCIES
execute_process(COMMAND python depo.py
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# LUT
add_subdirectory(${LOFX_EXTERNALS_DIR}/lut)
target_link_libraries(lofx lut)

# OPENGL
find_package(OpenGL REQUIRED)
target_link_libraries(lofx ${OPENGL_gl_LIBRARY})

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${LOFX_EXTERNALS_DIR}/glfw)
target_link_libraries(lofx glfw)

# GLM
add_subdirectory(${LOFX_EXTERNALS_DIR}/glm)
target_link_libraries(lofx glm)

# ASSIMP
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
add_subdirectory(${LOFX_EXTERNALS_DIR}/assimp)
target_include_directories(lofx PUBLIC ${LOFX_EXTERNALS_DIR}/assimp/include)
target_link_libraries(lofx assimp)

# TESTS
option(LOFX_BUILD_TESTS "Activate to build LOFX tests" OFF)

if (${LOFX_BUILD_TESTS})
	add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
endif()