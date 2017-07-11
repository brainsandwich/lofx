#include "LUT/lut.hpp"
#include "LOFX/lofx.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
		std::string name;
		std::vector<Geometry> geometries;
	};

	struct Node {
		std::string name;
		const Node* parent;
		glm::mat4 transform;
		std::vector<const Node*> children;
		const Mesh* mesh;
	};

	void render(const Geometry* geometry, const lofx::DrawProperties& props) {
		lofx::DrawProperties drp = props;
		drp.indices = &geometry->indices;
		drp.attributes = &geometry->attributePack;
		lofx::draw(drp);
	}

	void render(const Mesh* mesh, const lofx::DrawProperties& props) {
		for (const auto& geom : mesh->geometries)
			render(&geom, props);
	}

	void render(const Node* node, const lofx::DrawProperties& props, const glm::mat4& parent = glm::mat4()) {
		glm::mat4 current = parent * node->transform;
		lofx::send(props.pipeline->stages.at(lofx::ShaderType::Vertex), lofx::Uniform("model", current));
		if (node->mesh)
			render(node->mesh, props);
		for (const auto& node : node->children)
			render(node, props, current);
	}

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

	Mesh parse(const ygltf::mesh_t& mesh, ygltf::glTF_t* root) {
		Mesh result;
		std::vector<lofx::Buffer> buffers;
		std::vector<lofx::BufferView> views;
		std::vector<lofx::BufferAccessor> accessors;

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

	void parseBuffers(ygltf::glTF_t* root,
		std::vector<lofx::Buffer>* buffers,
		std::vector<lofx::BufferView>* views,
		std::vector<lofx::BufferAccessor>* accessors)
	{
		for (auto& buf : root->bufferViews) {
			switch (buf.target) {
			case ygltf::bufferView_t::target_t::array_buffer_t:
				buffers->push_back(lofx::createBuffer(lofx::BufferType::Vertex, buf.byteLength));
				break;
			case ygltf::bufferView_t::target_t::element_array_buffer_t:
				buffers->push_back(lofx::createBuffer(lofx::BufferType::Index, buf.byteLength));
				break;
			}

			lofx::send(&buffers->back(), root->buffers[buf.buffer].data.data() + buf.byteOffset);

			views->push_back(detail::parse(buf));
			views->back().buffer = buffers->back();
		}

		for (auto& buf : root->accessors) {
			accessors->push_back(detail::parse(buf));
			accessors->back().view = views->at(buf.bufferView);
		}
	}

	void parseMeshes(ygltf::glTF_t* root, const std::vector<lofx::BufferAccessor>& accessors, std::vector<Mesh>* meshes) {
		meshes->reserve(root->meshes.size());
		for (const auto& ymesh : root->meshes) {
			meshes->push_back(Mesh());
			Mesh& mesh = meshes->back();

			mesh.name = ymesh.name;
			mesh.geometries.reserve(ymesh.primitives.size());

			for (const auto& yprim : ymesh.primitives) {
				mesh.geometries.push_back(Geometry());
				Geometry& geometry = mesh.geometries.back();
				geometry.indices = accessors[yprim.indices];

				for (const auto& pair : yprim.attributes) {
					const int attrib_id = detail::to_attrib_id(pair.first);
					const int accessor_id = pair.second;
					geometry.attributePack.attributes[attrib_id] = accessors[accessor_id];
					if (geometry.attributePack.bufferid == 0)
						geometry.attributePack.bufferid = geometry.attributePack.attributes[attrib_id].view.buffer.id;
				}
			}
		}
	}

	void parseNodes(ygltf::glTF_t* root, const std::vector<Mesh>& meshes, std::vector<Node>* nodes) {
		nodes->reserve(root->nodes.size());
		
		// Associate mesh and transform
		for (const auto& ynode : root->nodes) {
			nodes->push_back(Node());
			Node& node = nodes->back();

			node.transform = glm::make_mat4(ynode.matrix.data());
			node.name = ynode.name;
			node.mesh = ynode.mesh >= 0 ? &meshes[ynode.mesh] : nullptr;
		}
		
		// Associate children nodes
		for (std::size_t i = 0; i < root->nodes.size(); i++) {
			Node& node = nodes->at(i);
			ygltf::node_t& ynode = root->nodes[i];

			for (std::size_t k = 0; k < ynode.children.size(); k++)
				node.children.push_back(&nodes->at(ynode.children[k]));
		}
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
	colorout = vec4(fsin.normal, 1.0);
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

	// Parse buffers in file
	std::vector<lofx::Buffer> buffers;
	std::vector<lofx::BufferView> views;
	std::vector<lofx::BufferAccessor> accessors;
	d3::parseBuffers(gltf_model, &buffers, &views, &accessors);

	// Parse meshes in file
	std::vector<d3::Mesh> meshes;
	d3::parseMeshes(gltf_model, accessors, &meshes);

	// Parse nodes in file
	std::vector<d3::Node> nodes;
	d3::parseNodes(gltf_model, meshes, &nodes);

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

	lofx::DrawProperties drawProperties;
	drawProperties.fbo = &fbo;
	drawProperties.pipeline = &pipeline;

	// Loop while window is not closed
	float time = 0.0f;
	lofx::loop([&] {
		nodes[2].transform = glm::mat4();
		nodes[2].transform = glm::rotate(nodes[2].transform, time, glm::vec3(0.0f, 1.0f, 0.0f));
		time += 0.016f;

		for (const auto& ynode : gltf_model->scenes[gltf_model->scene].nodes)
			d3::render(&nodes[ynode], drawProperties);

		std::this_thread::sleep_for(16ms);
	});

	// Cleanup
	lofx::release(&pipeline);
	
	return 0;
}