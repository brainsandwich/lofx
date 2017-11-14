#include <lofx/lofx.hpp>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg/nanovg.h>
#include <nanovg/nanovg_gl.h>

//#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <map>
#include <utility>
#include <algorithm>

#include "storage.hpp"

namespace fs = std::experimental::filesystem;
using namespace std::chrono_literals;

void debug_callback(lofx::DebugMessageDetails details, const std::string& msg);

struct Context {
	lut::storage::Market resources;
};

int main(int argc, char** argv) {

	// Init lofx
	lofx::setdbgCallback(debug_callback);
	lofx::init(glm::u32vec2(1000, 1000), "4.4");
	atexit(lofx::terminate);

	return 0;
}

/////////////////////////////////////////////////

void debug_callback(lofx::DebugMessageDetails details, const std::string& msg) {
	std::string shown_level = "";
	switch (details.level) {
		case lofx::DebugLevel::Trace: shown_level = "trace"; break;
		case lofx::DebugLevel::Warn: shown_level = "warn"; break;
		case lofx::DebugLevel::Error: shown_level = "error"; break;
	}

	switch (details.source) {
		case lofx::DebugSource::Lofx:
			fprintf(stdout, "[lofx %s] : %s\n", shown_level.c_str(), msg.c_str());
			break;
		case lofx::DebugSource::Glfw: {
			lofx::GlfwErrorParams params;
			deserialize(details.params, reinterpret_cast<uint8_t*>(&params));
			fprintf(stdout, "[glfw %s] %s : %s\n", params.errcode_str().c_str(), shown_level.c_str(), msg.c_str());
			break;
		}
		case lofx::DebugSource::OpenGL: {
			lofx::OpenGLErrorParams params;
			deserialize(details.params, reinterpret_cast<uint8_t*>(&params));
			fprintf(stdout, "[opengl %s]\n > source : %s\n > type : %s\n > severity : %s\n%s\n",
				shown_level.c_str(),
				params.source_str().c_str(),
				params.type_str().c_str(),
				params.severity_str().c_str(),
				msg.c_str());
			break;
		}
	}
}