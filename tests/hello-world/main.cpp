#include "lut/lut.hpp"
#include "lofx/lofx.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

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
		const lofx::Program* vertex_stage = lofx::findStage(props.pipeline, lofx::ShaderType::Vertex);
		if (vertex_stage)
			lofx::send(vertex_stage, lofx::Uniform("model", current));

		if (node->mesh)
			render(node->mesh, props);
		for (const auto& node : node->children)
			render(node, props, current);
	}

	const Mesh* findMesh(const Node* root, const std::string& name) {
		for (auto& child : root->children) {
			if (child->mesh != nullptr && child->mesh->name == name)
				return child->mesh;
		}

		for (auto& child : root->children) {
			auto res = findMesh(child, name);
			if (res != nullptr)
				return res;
		}

		return nullptr;
	}

	const Node* findNode(const Node* root, const std::string& name) {
		for (auto& child : root->children) {
			if (child->name == name)
				return child;
		}

		for (auto& child : root->children) {
			auto res = findNode(child, name);
			if (res != nullptr)
				return res;
		}

		return nullptr;
	}

	namespace gltf {

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
				glm::mat4 rotation = glm::mat4_cast(glm::quat(ynode.rotation[0], ynode.rotation[1], ynode.rotation[2], ynode.rotation[3]));
				node.transform = glm::translate(node.transform, glm::vec3(ynode.translation[0], ynode.translation[1], ynode.translation[2]));
				node.transform = node.transform * rotation;

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

		void parseTextures(ygltf::glTF_t* root, std::vector<lofx::Texture>* textures, std::vector<lofx::TextureSampler>* samplers) {
			for (const auto& ysamp : root->samplers) {
				lofx::TextureSamplerParameters sampler_parameters;
				
				switch (ysamp.magFilter) {
				case ygltf::sampler_t::magFilter_t::linear_t: sampler_parameters.mag = lofx::TextureMagnificationFilter::Linear; break;
				case ygltf::sampler_t::magFilter_t::nearest_t: sampler_parameters.mag = lofx::TextureMagnificationFilter::Nearest; break;
				}

				switch (ysamp.minFilter) {
				case ygltf::sampler_t::minFilter_t::linear_t: sampler_parameters.min = lofx::TextureMinificationFilter::Linear; break;
				case ygltf::sampler_t::minFilter_t::nearest_t: sampler_parameters.min = lofx::TextureMinificationFilter::Nearest; break;
				case ygltf::sampler_t::minFilter_t::linear_mipmap_linear_t: sampler_parameters.min = lofx::TextureMinificationFilter::LinearMipmapLinear; break;
				case ygltf::sampler_t::minFilter_t::linear_mipmap_nearest_t: sampler_parameters.min = lofx::TextureMinificationFilter::LinearMipmapNearest; break;
				case ygltf::sampler_t::minFilter_t::nearest_mipmap_linear_t: sampler_parameters.min = lofx::TextureMinificationFilter::NearestMipmapLinear; break;
				case ygltf::sampler_t::minFilter_t::nearest_mipmap_nearest_t: sampler_parameters.min = lofx::TextureMinificationFilter::NearestMipmapNearest; break;
				}

				switch (ysamp.wrapS) {
				case ygltf::sampler_t::wrapS_t::clamp_to_edge_t: sampler_parameters.s = lofx::TextureWrappping::Clamp; break;
				case ygltf::sampler_t::wrapS_t::mirrored_repeat_t: sampler_parameters.s = lofx::TextureWrappping::Mirrored; break;
				case ygltf::sampler_t::wrapS_t::repeat_t: sampler_parameters.s = lofx::TextureWrappping::Repeat; break;
				}

				switch (ysamp.wrapT) {
				case ygltf::sampler_t::wrapT_t::clamp_to_edge_t: sampler_parameters.t = lofx::TextureWrappping::Clamp; break;
				case ygltf::sampler_t::wrapT_t::mirrored_repeat_t: sampler_parameters.t = lofx::TextureWrappping::Mirrored; break;
				case ygltf::sampler_t::wrapT_t::repeat_t: sampler_parameters.t = lofx::TextureWrappping::Repeat; break;
				}
				
				samplers->push_back(lofx::createTextureSampler(sampler_parameters));
			}

			for (const auto& ytex : root->textures) {
				const ygltf::image_t& yimg = root->images[ytex.source];
				const lofx::TextureSampler* sampler = nullptr;
				if (ytex.sampler != -1)
					sampler = &samplers->at(ytex.sampler);
				
				lofx::ImageDataType data_type = lofx::ImageDataType::UnsignedByte;
				lofx::ImageDataFormat data_format;
				lofx::TextureInternalFormat internal_format;
				
				switch (yimg.data.ncomp) {
				case 1:
					internal_format = lofx::TextureInternalFormat::R8;
					data_format = lofx::ImageDataFormat::R;
					break;
				case 2:
					internal_format = lofx::TextureInternalFormat::RG8;
					data_format = lofx::ImageDataFormat::RG;
					break;
				case 3:
					internal_format = lofx::TextureInternalFormat::RGB8;
					data_format = lofx::ImageDataFormat::RGB;
					break;
				case 4:
					internal_format = lofx::TextureInternalFormat::RGBA8;
					data_format = lofx::ImageDataFormat::RGBA;
					break;
				}

				textures->push_back(lofx::createTexture(yimg.data.width, yimg.data.height, 0, sampler, lofx::TextureTarget::Texture2d, internal_format));
				lofx::send(&textures->back(), yimg.data.datab.data(), data_format, data_type);
			}
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
	vec3 coordinate;
	vec3 color;
	vec3 normal;
	vec3 uv;
	vec3 viewdir;
	mat3 tbn;
} vsout;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform float time;

mat3 computeTBN(in vec3 normal) {
	vec3 tangent;
	vec3 bitangent;
	
	vec3 c1 = cross(normal, vec3(0.0, 0.0, 1.0)); 
	vec3 c2 = cross(normal, vec3(0.0, 1.0, 0.0)); 
	
	if (length(c1) > length(c2))
		tangent = c1;
	else
		tangent = c2;
	
	tangent = normalize(tangent);
	
	bitangent = cross(normal, tangent); 
	bitangent = normalize(bitangent);

	return mat3(tangent, bitangent, normal);
}

void main() {
	gl_Position = projection * view * model * vec4(coordinate, 1.0);

	vsout.coordinate = (model * vec4(coordinate, 1.0)).xyz;
	vsout.color = vec3(0.5, 0.5, 0.5);
	vsout.normal = mat3(transpose(inverse(model))) * normal;
	vsout.uv = uv;
	vsout.viewdir = (inverse(view) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vsout.tbn = computeTBN(vsout.normal);
}

)";

const std::string fragment_shader_source = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in VSOut {
	vec3 coordinate;
	vec3 color;
	vec3 normal;
	vec3 uv;
	vec3 viewdir;
	mat3 tbn;
} fsin;

layout (location = 0) out vec4 colorout;

uniform sampler2D albedo;
uniform sampler2D emissive;
uniform sampler2D occlusion;
uniform sampler2D normalmap;
uniform float time;

void main() {
	vec3 normal = fsin.normal;
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
	lofx::init(glm::u32vec2(2000, 1500), "4.4");
	atexit(lofx::terminate);

	// Load model
	auto gltf_model = ygltf::load_gltf("C:\\Users\\brainsandwich\\Documents\\dev\\polypack\\lofx\\tests\\hello-world\\man.gltf");

	// Parse buffers in file
	std::vector<lofx::Buffer> buffers;
	std::vector<lofx::BufferView> views;
	std::vector<lofx::BufferAccessor> accessors;
	d3::gltf::parseBuffers(gltf_model, &buffers, &views, &accessors);

	// Parse meshes in file
	std::vector<d3::Mesh> meshes;
	d3::gltf::parseMeshes(gltf_model, accessors, &meshes);

	// Parse nodes in file
	std::vector<d3::Node> nodes;
	d3::gltf::parseNodes(gltf_model, meshes, &nodes);

	// Parse textures
	std::vector<lofx::TextureSampler> samplers;
	std::vector<lofx::Texture> textures;
	d3::gltf::parseTextures(gltf_model, &textures, &samplers);

	// Preparing shader program
	lofx::Program vertex_program = lofx::createProgram(lofx::ShaderType::Vertex, { vertex_shader_source });
	lofx::Program fragment_program = lofx::createProgram(lofx::ShaderType::Fragment, { fragment_shader_source });
	lofx::Pipeline pipeline = lofx::createPipeline();
	pipeline.stages = { &vertex_program, &fragment_program };

	// Retrieving main framebuffer
	lofx::Framebuffer fbo = lofx::current_fbo();

	// Prepare camera and send uniforms
	Camera camera;
	camera.projection = glm::perspective(60.0f * glm::pi<float>() / 180.0f, 2000.0f / 1500.0f, 0.1f, 100.0f);
	camera.view = glm::lookAt(glm::vec3(0.0f, 2.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	lofx::send(&vertex_program, lofx::Uniform("view", camera.view));
	lofx::send(&vertex_program, lofx::Uniform("projection", camera.projection));

	lofx::DrawProperties drawProperties;
	drawProperties.fbo = &fbo;
	drawProperties.pipeline = &pipeline;
	//drawProperties.textures["albedo"] = &textures[0];
	//drawProperties.textures["normalmap"] = &textures[1];
	//drawProperties.textures["emissive"] = &textures[3];
	//drawProperties.textures["occlusion"] = &textures[4];

	// Loop while window is not closed
	//nodes[0].transform = glm::rotate(nodes[0].transform, glm::pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
	float time = 0.0f;
	lofx::loop([&] {
		camera.view = glm::lookAt(glm::vec3(5.0f * cos(0.25f * time), 2.0f, 5.0f * sin(0.25f * time)), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		lofx::send(&vertex_program, lofx::Uniform("view", camera.view));
		time += 0.016f;

		for (const auto& ynode : gltf_model->scenes[0].nodes)
			d3::render(&nodes[ynode], drawProperties);

		std::this_thread::sleep_for(16ms);
	});

	// Cleanup
	lofx::release(&pipeline);
	
	return 0;
}