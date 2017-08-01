cmake_minimum_required(VERSION 3.8)
project(LOFX VERSION 0.1.0 LANGUAGES C CXX)
cmake_policy(SET CMP0048 NEW)

set(LOFX_EXTERNALS_DIR ${PROJECT_SOURCE_DIR}/external)
set(LOFX_EXTERNAL_SOURCES_DIR ${LOFX_EXTERNALS_DIR}/sources)
set(LOFX_EXTERNAL_INCLUDE_DIR ${LOFX_EXTERNALS_DIR}/output/include)
set(LOFX_EXTERNAL_LIBRARY_DIR ${LOFX_EXTERNALS_DIR}/output/lib)
set(LOFX_EXTERNAL_BINARY_DIR ${LOFX_EXTERNALS_DIR}/output/bin)

# GL3W
execute_process(
	COMMAND python ${LOFX_EXTERNAL_SOURCES_DIR}/gl3w/gl3w_gen.py
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# LOFX SOURCES
file(GLOB_RECURSE LOFX_SOURCES "${PROJECT_SOURCE_DIR}/src/**.c*")
file(GLOB_RECURSE LOFX_INCLUDES "${PROJECT_SOURCE_DIR}/include/**.h*")
set(LOFX_INCLUDEDIR ${PROJECT_SOURCE_DIR}/include)

# LOFX EXECUTABLE
add_library(lofx ${LOFX_SOURCES} ${LOFX_INCLUDES})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties (lofx PROPERTIES FOLDER lofx)
target_include_directories(lofx PUBLIC ${LOFX_INCLUDEDIR})

# FETCH DEPENDENCIES
execute_process(COMMAND python depo.py
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# #############################################################
# Thats not working for now ... 
# 
# # EXTERNAL INCLUDES
# target_include_directories(lofx PUBLIC ${LOFX_EXTERNAL_INCLUDE_DIR})

# # EXTERNAL LIBRARIES
# find_library(GLFW_LIB glfw NAMES glfw3 HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_library(YOCTO_LIB yocto HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_library(LUT_LIB lut HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_library(FMT_LIB fmt HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_library(GLFW_LIB_DEBUG glfw_d NAMES glfw3_d HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_library(YOCTO_LIB_DEBUG yocto_d HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_library(LUT_LIB_DEBUG lut_d HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_library(FMT_LIB_DEBUG fmt_d HINTS ${LOFX_EXTERNAL_LIBRARY_DIR})
# find_package(OpenGL REQUIRED)

# target_link_libraries(lofx PUBLIC optimized ${FMT_LIB} ${LUT_LIB} ${OPENGL_gl_LIBRARY} ${GLFW_LIB} ${YOCTO_LIB})
# target_link_libraries(lofx PUBLIC debug ${FMT_LIB_DEBUG} ${LUT_LIB_DEBUG} ${OPENGL_gl_LIBRARY} ${GLFW_LIB_DEBUG} ${YOCTO_LIB_DEBUG})
# #############################################################

# #############################################################
find_package(OpenGL REQUIRED)
target_link_libraries(lofx PUBLIC ${OPENGL_gl_LIBRARY})

add_subdirectory(${LOFX_EXTERNAL_SOURCES_DIR}/glfw)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
target_link_libraries(lofx PUBLIC glfw)

add_subdirectory(${LOFX_EXTERNAL_SOURCES_DIR}/glm)
target_link_libraries(lofx PUBLIC glm)

add_subdirectory(${LOFX_EXTERNAL_SOURCES_DIR}/lut)
set(FMT_INSTALL ON CACHE BOOL "" FORCE)
target_link_libraries(lofx PUBLIC lut)

add_subdirectory(${LOFX_EXTERNAL_SOURCES_DIR}/yocto-gl)
target_link_libraries(lofx PUBLIC yocto)
# #############################################################

# TESTS
option(LOFX_BUILD_TESTS "Activate to build LOFX tests" OFF)

if (${LOFX_BUILD_TESTS})
	add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
endif()