add_executable(grfx main.cpp nanovg/nanovg.c
	nanovg/fontstash.h nanovg/nanovg.h nanovg/nanovg_gl_utils.h nanovg/stb_truetype.h
	nanovg/nanovg_gl.h nanovg/stb_image.h storage.hpp)

target_include_directories(grfx PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(grfx lofx)
set_target_properties (grfx PROPERTIES FOLDER lofx/tests)