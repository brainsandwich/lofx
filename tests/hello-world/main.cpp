#include "LUT/lut.hpp"
#include "LOFX/lofx.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <yocto/yocto_gltf.h>

#include <fstream>
#include <filesystem>

namespace fs = std::experimental::filesystem;

namespace d3 {

	struct Material {};
	
	struct Geometry {
		lofx::AttributePack attributePack;
		lofx::BufferAccessor indices;
		Material* material;
	};

	struct Mesh {
		std::vector<Geometry> geometries;
	};

	namespace detail {
		lofx::BufferAccessor parse(const ygltf::accessor_t& input) {
			lofx::BufferAccessor result;
			switch (input.type) {
			case ygltf::accessor_t::type_t::scalar_t:
				result.components = 1;
				break;
			case ygltf::accessor_t::type_t::vec2_t:
				result.components = 2;
				break;
			case ygltf::accessor_t::type_t::vec3_t:
				result.components = 3;
				break;
			case ygltf::accessor_t::type_t::vec4_t:
			case ygltf::accessor_t::type_t::mat2_t:
				result.components = 4;
				break;
			case ygltf::accessor_t::type_t::mat3_t:
				result.components = 9;
				break;
			case ygltf::accessor_t::type_t::mat4_t:
				result.components = 16;
				break;
			}
			switch (input.componentType) {
			case ygltf::accessor_t::componentType_t::byte_t:
				result.component_type = lofx::AttributeType::Byte;
				break;
			case ygltf::accessor_t::componentType_t::unsigned_byte_t:
				result.component_type = lofx::AttributeType::UnsignedByte;
				break;
			case ygltf::accessor_t::componentType_t::short_t:
				result.component_type = lofx::AttributeType::Short;
				break;
			case ygltf::accessor_t::componentType_t::unsigned_short_t:
				result.component_type = lofx::AttributeType::UnsignedShort;
				break;
			case ygltf::accessor_t::componentType_t::unsigned_int_t:
				result.component_type = lofx::AttributeType::UnsignedInt;
				break;
			case ygltf::accessor_t::componentType_t::float_t:
				result.component_type = lofx::AttributeType::Float;
				break;
			}
			result.count = input.count;
			result.normalized = input.normalized;
			result.offset = input.byteOffset;
			return result;
		}

		lofx::BufferView parse(const ygltf::bufferView_t& input) {
			lofx::BufferView result;
			result.length = input.byteLength;
			result.offset = input.byteOffset;
			result.stride = input.byteStride;
			return result;
		}

		uint32_t to_attrib_id(const std::string& attrib_name) {
			if (attrib_name == "POSITION") return 0;
			else if (attrib_name == "NORMAL") return 1;
			else if (attrib_name == "COLOR_0") return 2;
			else if (attrib_name == "TEXCOORD_0") return 3;
			else if (attrib_name == "TEXCOORD_1") return 4;
			else if (attrib_name == "TANGENT") return 5;
			else if (attrib_name == "JOINTS_0") return 6;
			else if (attrib_name == "WEIGHTS_0") return 7;
			return 8;
		}
	}

	Mesh fromgltf(const ygltf::mesh_t& mesh, ygltf::glTF_t* root) {
		Mesh result;
		std::vector<lofx::Buffer> buffers;
		std::vector<lofx::BufferView> views;
		std::vector<lofx::BufferAccessor> accessors;

		for (auto& buf : root->buffers) {
			
		}

		for (auto& buf : root->bufferViews) {
			switch (buf.target) {
			case ygltf::bufferView_t::target_t::array_buffer_t:
				buffers.push_back(lofx::createBuffer(lofx::BufferType::Vertex, buf.byteLength));
				break;
			case ygltf::bufferView_t::target_t::element_array_buffer_t:
				buffers.push_back(lofx::createBuffer(lofx::BufferType::Index, buf.byteLength));
				break;
			}

			lofx::send(&buffers.back(), root->buffers[buf.buffer].data.data() + buf.byteOffset);

			views.push_back(detail::parse(buf));
			views.back().buffer = buffers.back();
		}

		for (auto& buf : root->accessors) {
			accessors.push_back(detail::parse(buf));
			accessors.back().view = views[buf.bufferView];
		}

		result.geometries.reserve(mesh.primitives.size());
		for (const auto& primitive : mesh.primitives) {
			result.geometries.push_back(Geometry());
			Geometry& geometry = result.geometries.back();

			geometry.indices = accessors[primitive.indices];
			for (const auto& pair : primitive.attributes) {
				const int attrib_id = detail::to_attrib_id(pair.first);
				const int accessor_id = pair.second;
				geometry.attributePack.attributes[attrib_id] = accessors[accessor_id];
				if (geometry.attributePack.bufferid == 0)
					geometry.attributePack.bufferid = geometry.attributePack.attributes[attrib_id].view.buffer.id;
			}
		}
		return result;
	}

	struct Skeleton {};

}

struct Camera {
	glm::mat4 projection;
	glm::mat4 view;
};

// Input data
const std::string vertex_shader_source = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

layout (location = 0) in vec3 coordinate;
layout (location = 1) in vec3 normal;
layout (location = 3) in vec3 uv;

out gl_PerVertex {
	vec4 gl_Position;
};

out VSOut {
	vec3 color;
	vec3 normal;
	vec3 uv;
} vsout;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform float time;

void main() {
	gl_Position = projection * view * model * vec4(coordinate, 1.0);
	vsout.color = vec3(0.5, 0.5, 0.5);
	vsout.normal = normal;
	vsout.uv = uv;
}

)";

const std::string fragment_shader_source = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in VSOut {
	vec3 color;
	vec3 normal;
	vec3 uv;
} fsin;

layout (location = 0) out vec4 colorout;

uniform float time;

void main() {
	colorout = vec4(fsin.uv, 1.0);
}

)";

#include <thread>

using namespace std::chrono_literals;
using clk = std::chrono::high_resolution_clock;
auto prtm = [](const std::string& msg, const clk::duration& duration, double mul = 1.0) { lut::trace("{0} : {1} sec\n", msg, (double) duration.count() / 1e9 * mul); };

int main() {
	lut::detail::loglevel = lut::LogLevel::Warn;
	lut::detail::logtoconsole = true;

	// Init LOFX
	lofx::init(glm::u32vec2(2000, 1200), "4.4");
	atexit(lofx::terminate);

	// Load model
	auto gltf_model = ygltf::load_gltf("C:\\Users\\brainsandwich\\Downloads\\glTF-Sample-Models-master\\2.0\\Duck\\glTF-Embedded\\Duck.gltf");
	d3::Mesh mesh = d3::fromgltf(gltf_model->meshes[0], gltf_model);

	// Preparing shader program
	lofx::Program vertex_program = lofx::createProgram(lofx::ShaderType::Vertex, { vertex_shader_source });
	lofx::Program fragment_program = lofx::createProgram(lofx::ShaderType::Fragment, { fragment_shader_source });
	lofx::Pipeline pipeline = lofx::createPipeline();
	pipeline.stages[lofx::ShaderType::Vertex] = &vertex_program;
	pipeline.stages[lofx::ShaderType::Fragment] = &fragment_program;

	// Retrieving main framebuffer
	lofx::Framebuffer fbo = lofx::current_fbo();

	// Prepare camera and send uniforms
	Camera camera;
	camera.projection = glm::perspective(60.0f * glm::pi<float>() / 180.0f, 2000.0f / 1200.0f, 0.1f, 100.0f);
	camera.view = glm::lookAt(glm::vec3(0.0f, 2.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	lofx::send(&vertex_program, lofx::Uniform("view", camera.view));
	lofx::send(&vertex_program, lofx::Uniform("projection", camera.projection));

	lofx::DrawProperties draw;
	draw.attributes = &mesh.geometries[0].attributePack;
	draw.indices = &mesh.geometries[0].indices;
	draw.fbo = &fbo;
	draw.pipeline = &pipeline;

	// Loop while window is not closed
	float time = 0.0f;
	lofx::loop([&] {
		glm::mat4 duck_model;
		duck_model = glm::rotate(duck_model, time, glm::vec3(0.0f, 1.0f, 0.0f));
		duck_model = glm::scale(duck_model, glm::vec3(0.01f));
		time += 0.016f;
		lofx::send(&vertex_program, lofx::Uniform("model", duck_model));
		for (auto& geom : mesh.geometries) {
			draw.indices = &geom.indices;
			draw.attributes = &geom.attributePack;
			lofx::draw(draw);
		}

		std::this_thread::sleep_for(16ms);
	});

	// Cleanup
	lofx::release(&pipeline);
	
	return 0;
}