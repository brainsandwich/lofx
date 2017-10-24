#include "lut/lut.hpp"
#include "lofx/lofx.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <yocto/yocto_gltf.h>

#include <fstream>
#include <filesystem>

//namespace fs = std::experimental::filesystem;
//
//namespace d3 {
//
//	struct Material {};
//
//	struct Geometry {
//		lofx::AttributePack attributePack;
//		lofx::BufferAccessor indices;
//		Material* material;
//	};
//
//	struct Mesh {
//		std::string name;
//		std::vector<Geometry> geometries;
//	};
//
//	struct Node {
//		std::string name;
//		const Node* parent;
//		glm::mat4 transform;
//		std::vector<const Node*> children;
//		const Mesh* mesh;
//	};
//
//	void release(Geometry& geometry) {
//		lofx::release(&geometry.indices.view.buffer);
//		for (auto& pair : geometry.attributePack.attributes)
//			lofx::release(&pair.second.view.buffer);
//	}
//
//	void render(const Geometry* geometry, const lofx::DrawProperties& props) {
//		lofx::DrawProperties drp = props;
//		drp.indices = &geometry->indices;
//		drp.attributes = &geometry->attributePack;
//		lofx::draw(drp);
//	}
//
//	void render(const Mesh* mesh, const lofx::DrawProperties& props) {
//		for (const auto& geom : mesh->geometries)
//			render(&geom, props);
//	}
//
//	void render(const Node* node, const lofx::DrawProperties& props, const glm::mat4& parent = glm::mat4()) {
//		glm::mat4 current = parent * node->transform;
//		const lofx::Program* vertex_stage = lofx::findStage(props.pipeline, lofx::ShaderType::Vertex);
//		if (vertex_stage)
//			lofx::send(vertex_stage, lofx::Uniform("model", current));
//
//		if (node->mesh)
//			render(node->mesh, props);
//		for (const auto& node : node->children)
//			render(node, props, current);
//	}
//
//	const Mesh* findMesh(const Node* root, const std::string& name) {
//		for (auto& child : root->children) {
//			if (child->mesh != nullptr && child->mesh->name == name)
//				return child->mesh;
//		}
//
//		for (auto& child : root->children) {
//			auto res = findMesh(child, name);
//			if (res != nullptr)
//				return res;
//		}
//
//		return nullptr;
//	}
//
//	const Node* findNode(const Node* root, const std::string& name) {
//		for (auto& child : root->children) {
//			if (child->name == name)
//				return child;
//		}
//
//		for (auto& child : root->children) {
//			auto res = findNode(child, name);
//			if (res != nullptr)
//				return res;
//		}
//
//		return nullptr;
//	}
//
//	namespace gltf {
//
//		namespace detail {
//			lofx::BufferAccessor parse(const ygltf::accessor_t& input) {
//				lofx::BufferAccessor result;
//				switch (input.type) {
//					case ygltf::accessor_t::type_t::scalar_t:
//						result.components = 1;
//						break;
//					case ygltf::accessor_t::type_t::vec2_t:
//						result.components = 2;
//						break;
//					case ygltf::accessor_t::type_t::vec3_t:
//						result.components = 3;
//						break;
//					case ygltf::accessor_t::type_t::vec4_t:
//					case ygltf::accessor_t::type_t::mat2_t:
//						result.components = 4;
//						break;
//					case ygltf::accessor_t::type_t::mat3_t:
//						result.components = 9;
//						break;
//					case ygltf::accessor_t::type_t::mat4_t:
//						result.components = 16;
//						break;
//				}
//				switch (input.componentType) {
//					case ygltf::accessor_t::componentType_t::byte_t:
//						result.component_type = lofx::AttributeType::Byte;
//						break;
//					case ygltf::accessor_t::componentType_t::unsigned_byte_t:
//						result.component_type = lofx::AttributeType::UnsignedByte;
//						break;
//					case ygltf::accessor_t::componentType_t::short_t:
//						result.component_type = lofx::AttributeType::Short;
//						break;
//					case ygltf::accessor_t::componentType_t::unsigned_short_t:
//						result.component_type = lofx::AttributeType::UnsignedShort;
//						break;
//					case ygltf::accessor_t::componentType_t::unsigned_int_t:
//						result.component_type = lofx::AttributeType::UnsignedInt;
//						break;
//					case ygltf::accessor_t::componentType_t::float_t:
//						result.component_type = lofx::AttributeType::Float;
//						break;
//				}
//				result.count = input.count;
//				result.normalized = input.normalized;
//				result.offset = input.byteOffset;
//				return result;
//			}
//
//			lofx::BufferView parse(const ygltf::bufferView_t& input) {
//				lofx::BufferView result;
//				result.length = input.byteLength;
//				result.offset = input.byteOffset;
//				result.stride = input.byteStride;
//				return result;
//			}
//
//			uint32_t to_attrib_id(const std::string& attrib_name) {
//				if (attrib_name == "POSITION") return 0;
//				else if (attrib_name == "NORMAL") return 1;
//				else if (attrib_name == "COLOR_0") return 2;
//				else if (attrib_name == "TEXCOORD_0") return 3;
//				else if (attrib_name == "TEXCOORD_1") return 4;
//				else if (attrib_name == "TANGENT") return 5;
//				else if (attrib_name == "JOINTS_0") return 6;
//				else if (attrib_name == "WEIGHTS_0") return 7;
//				return 8;
//			}
//		}
//
//		void parseBuffers(ygltf::glTF_t* root,
//			std::vector<lofx::Buffer>* buffers,
//			std::vector<lofx::BufferView>* views,
//			std::vector<lofx::BufferAccessor>* accessors)
//		{
//			for (auto& buf : root->bufferViews) {
//				switch (buf.target) {
//					case ygltf::bufferView_t::target_t::array_buffer_t:
//						buffers->push_back(lofx::createBuffer(lofx::BufferType::Vertex, buf.byteLength));
//						break;
//					case ygltf::bufferView_t::target_t::element_array_buffer_t:
//						buffers->push_back(lofx::createBuffer(lofx::BufferType::Index, buf.byteLength));
//						break;
//				}
//
//				lofx::send(&buffers->back(), root->buffers[buf.buffer].data.data() + buf.byteOffset);
//
//				views->push_back(detail::parse(buf));
//				views->back().buffer = buffers->back();
//			}
//
//			for (auto& buf : root->accessors) {
//				accessors->push_back(detail::parse(buf));
//				accessors->back().view = views->at(buf.bufferView);
//			}
//		}
//
//		void parseMeshes(ygltf::glTF_t* root, const std::vector<lofx::BufferAccessor>& accessors, std::vector<Mesh>* meshes) {
//			meshes->reserve(root->meshes.size());
//			for (const auto& ymesh : root->meshes) {
//				meshes->push_back(Mesh());
//				Mesh& mesh = meshes->back();
//
//				mesh.name = ymesh.name;
//				mesh.geometries.reserve(ymesh.primitives.size());
//
//				for (const auto& yprim : ymesh.primitives) {
//					mesh.geometries.push_back(Geometry());
//					Geometry& geometry = mesh.geometries.back();
//					geometry.indices = accessors[yprim.indices];
//
//					for (const auto& pair : yprim.attributes) {
//						const int attrib_id = detail::to_attrib_id(pair.first);
//						const int accessor_id = pair.second;
//						geometry.attributePack.attributes[attrib_id] = accessors[accessor_id];
//						if (geometry.attributePack.bufferid == 0)
//							geometry.attributePack.bufferid = geometry.attributePack.attributes[attrib_id].view.buffer.id;
//					}
//				}
//			}
//		}
//
//		void parseNodes(ygltf::glTF_t* root, const std::vector<Mesh>& meshes, std::vector<Node>* nodes) {
//			nodes->reserve(root->nodes.size());
//
//			// Associate mesh and transform
//			for (const auto& ynode : root->nodes) {
//				nodes->push_back(Node());
//				Node& node = nodes->back();
//
//				node.transform = glm::make_mat4(ynode.matrix.data());
//				glm::mat4 rotation = glm::mat4_cast(glm::quat(ynode.rotation[0], ynode.rotation[1], ynode.rotation[2], ynode.rotation[3]));
//				node.transform = glm::translate(node.transform, glm::vec3(ynode.translation[0], ynode.translation[1], ynode.translation[2]));
//				node.transform = node.transform * rotation;
//
//				node.name = ynode.name;
//				node.mesh = ynode.mesh >= 0 ? &meshes[ynode.mesh] : nullptr;
//			}
//
//			// Associate children nodes
//			for (std::size_t i = 0; i < root->nodes.size(); i++) {
//				Node& node = nodes->at(i);
//				ygltf::node_t& ynode = root->nodes[i];
//
//				for (std::size_t k = 0; k < ynode.children.size(); k++)
//					node.children.push_back(&nodes->at(ynode.children[k]));
//			}
//		}
//
//		void parseTextures(ygltf::glTF_t* root, std::vector<lofx::Texture>* textures, std::vector<lofx::TextureSampler>* samplers) {
//			for (const auto& ysamp : root->samplers) {
//				lofx::TextureSamplerParameters sampler_parameters;
//
//				switch (ysamp.magFilter) {
//					case ygltf::sampler_t::magFilter_t::linear_t: sampler_parameters.mag = lofx::TextureMagnificationFilter::Linear; break;
//					case ygltf::sampler_t::magFilter_t::nearest_t: sampler_parameters.mag = lofx::TextureMagnificationFilter::Nearest; break;
//				}
//
//				switch (ysamp.minFilter) {
//					case ygltf::sampler_t::minFilter_t::linear_t: sampler_parameters.min = lofx::TextureMinificationFilter::Linear; break;
//					case ygltf::sampler_t::minFilter_t::nearest_t: sampler_parameters.min = lofx::TextureMinificationFilter::Nearest; break;
//					case ygltf::sampler_t::minFilter_t::linear_mipmap_linear_t: sampler_parameters.min = lofx::TextureMinificationFilter::LinearMipmapLinear; break;
//					case ygltf::sampler_t::minFilter_t::linear_mipmap_nearest_t: sampler_parameters.min = lofx::TextureMinificationFilter::LinearMipmapNearest; break;
//					case ygltf::sampler_t::minFilter_t::nearest_mipmap_linear_t: sampler_parameters.min = lofx::TextureMinificationFilter::NearestMipmapLinear; break;
//					case ygltf::sampler_t::minFilter_t::nearest_mipmap_nearest_t: sampler_parameters.min = lofx::TextureMinificationFilter::NearestMipmapNearest; break;
//				}
//
//				switch (ysamp.wrapS) {
//					case ygltf::sampler_t::wrapS_t::clamp_to_edge_t: sampler_parameters.s = lofx::TextureWrappping::Clamp; break;
//					case ygltf::sampler_t::wrapS_t::mirrored_repeat_t: sampler_parameters.s = lofx::TextureWrappping::Mirrored; break;
//					case ygltf::sampler_t::wrapS_t::repeat_t: sampler_parameters.s = lofx::TextureWrappping::Repeat; break;
//				}
//
//				switch (ysamp.wrapT) {
//					case ygltf::sampler_t::wrapT_t::clamp_to_edge_t: sampler_parameters.t = lofx::TextureWrappping::Clamp; break;
//					case ygltf::sampler_t::wrapT_t::mirrored_repeat_t: sampler_parameters.t = lofx::TextureWrappping::Mirrored; break;
//					case ygltf::sampler_t::wrapT_t::repeat_t: sampler_parameters.t = lofx::TextureWrappping::Repeat; break;
//				}
//
//				samplers->push_back(lofx::createTextureSampler(sampler_parameters));
//			}
//
//			for (const auto& ytex : root->textures) {
//				const ygltf::image_t& yimg = root->images[ytex.source];
//				const lofx::TextureSampler* sampler = nullptr;
//				if (ytex.sampler != -1)
//					sampler = &samplers->at(ytex.sampler);
//
//				lofx::ImageDataType data_type = lofx::ImageDataType::UnsignedByte;
//				lofx::ImageDataFormat data_format;
//				lofx::TextureInternalFormat internal_format;
//
//				switch (yimg.data.ncomp) {
//					case 1:
//						internal_format = lofx::TextureInternalFormat::R8;
//						data_format = lofx::ImageDataFormat::R;
//						break;
//					case 2:
//						internal_format = lofx::TextureInternalFormat::RG8;
//						data_format = lofx::ImageDataFormat::RG;
//						break;
//					case 3:
//						internal_format = lofx::TextureInternalFormat::RGB8;
//						data_format = lofx::ImageDataFormat::RGB;
//						break;
//					case 4:
//						internal_format = lofx::TextureInternalFormat::RGBA8;
//						data_format = lofx::ImageDataFormat::RGBA;
//						break;
//				}
//
//				textures->push_back(lofx::createTexture(yimg.data.width, yimg.data.height, 0, sampler, lofx::TextureTarget::Texture2d, internal_format));
//				lofx::send(&textures->back(), yimg.data.datab.data(), data_format, data_type);
//			}
//		}
//	}
//
//	struct Skeleton {};
//
//}
//
//struct Camera {
//	glm::mat4 projection;
//	glm::mat4 view;
//};
//
namespace wireframe_shader_source {

	// Input data
	const std::string vertex = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

layout (location = 0) in vec3 coordinate;

out gl_PerVertex {
	vec4 gl_Position;
};

out VSOut {
	vec3 coordinate;
} vsout;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main() {	
	gl_Position = projection * view * model * vec4(coordinate, 1.0);
	vsout.coordinate = (model * vec4(coordinate, 1.0)).xyz;
}

)";

	const std::string geometry = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in VSOut {
	vec3 coordinate;
} gsin[];

out GSOut {
	vec3 coordinate;
	noperspective vec3 wireframe_dist;
} gsout;

in gl_PerVertex {
	vec4 gl_Position;
} gl_in[];

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	for (int i = 0; i < 3; i++) {
		gl_Position = gl_in[i].gl_Position;

		gsout.coordinate = gsin[i].coordinate;

		// This is the easiest scheme I could think of. The attribute will be interpolated, so
		// all you have to do is set the ith dimension to 1.0 to get barycentric coordinates
		// specific to this triangle. The frag shader will interpolate and then you can just use
		// a threshold in the frag shader to figure out if you're close to an edge
		gsout.wireframe_dist = vec3(0.0);
		gsout.wireframe_dist[i] = 1.0;
 
		EmitVertex();
	}
}
)";

	const std::string fragment = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in GSOut {
	vec3 coordinate;
	noperspective vec3 wireframe_dist;
} fsin;

layout (location = 0) out vec4 colorout;

uniform float time;

float edge(in vec3 baryc) {
	vec3 sm = smoothstep(vec3(0.0), 2.0 * fwidth(baryc), baryc);
	return min(min(sm.x, sm.y), sm.z);
}

void main() {
	vec3 barycoord = fsin.wireframe_dist;
	colorout = vec4(0.25 * fsin.coordinate.zzz + 0.5 - edge(barycoord), 1.0);
}

)";

}
//
//namespace terrain_shader_source {
//
//	// Input data
//	const std::string vertex = R"(
//#version 440
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_packing : enable
//
//layout (location = 0) in vec3 coordinate;
//layout (location = 1) in vec3 normal;
//
//out gl_PerVertex {
//	vec4 gl_Position;
//};
//
//out output_block_t {
//	vec3 coordinate;
//	vec3 normal;
//} vsout;
//
//uniform mat4 projection;
//uniform mat4 view;
//uniform mat4 model;
//
//mat3 computeTBN(in vec3 normal) {
//	vec3 tangent;
//	vec3 bitangent;
//	
//	vec3 c1 = cross(normal, vec3(0.0, 0.0, 1.0)); 
//	vec3 c2 = cross(normal, vec3(0.0, 1.0, 0.0)); 
//	
//	if (length(c1) > length(c2))
//		tangent = c1;
//	else
//		tangent = c2;
//	
//	tangent = normalize(tangent);
//	
//	bitangent = cross(normal, tangent); 
//	bitangent = normalize(bitangent);
//
//	return mat3(tangent, bitangent, normal);
//}
//
//void main() {	
//	gl_Position = projection * view * model * vec4(coordinate, 1.0);
//	vsout.coordinate = (model * vec4(coordinate, 1.0)).xyz;
//	vsout.normal = mat3(transpose(inverse(model))) * normal;
//}
//
//)";
//
//	const std::string fragment = R"(
//#version 440
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_packing : enable
//
//in input_block_t {
//	vec3 coordinate;
//	vec3 normal;
//} fsin;
//
//layout (location = 0) out vec4 colorout;
//
//uniform float time;
//
//void main() {
//	colorout = vec4(normalize(fsin.normal), 1.0);
//}
//
//)";
//
//}
//
//#include <glm/gtx/rotate_vector.hpp>
//#include <cmath>
//
//template <typename T>
//T random_range(const T& min, const T& max) {
//	T init = (T) rand() / (T) RAND_MAX;
//	return init * (max - min) + min;
//}
//
//namespace geotools {
//	struct Geometry {
//		std::vector<glm::vec3> positions;
//		std::vector<glm::vec3> normals;
//		std::vector<glm::vec2> uvs;
//		std::vector<uint32_t> indices;
//
//		void clear() {
//			positions.clear();
//			normals.clear();
//			uvs.clear();
//			indices.clear();
//			positions.shrink_to_fit();
//			normals.shrink_to_fit();
//			uvs.shrink_to_fit();
//			indices.shrink_to_fit();
//		}
//
//		Geometry& translate(const glm::vec3& value) {
//			for (auto& pos : positions)
//				pos += value;
//			return *this;
//		}
//
//		Geometry& rotate(float value, const glm::vec3& axis) {
//			for (auto& pos : positions)
//				pos = glm::rotate(pos, value, axis);
//			for (auto& nor : normals)
//				nor = glm::rotate(nor, value, axis);
//			return *this;
//		}
//
//		Geometry& scale(const glm::vec3& value) {
//			for (auto& pos : positions)
//				pos *= value;
//			return *this;
//		}
//
//		Geometry& recalculate_normals() {
//			normals.clear();
//			normals.resize(positions.size(), glm::vec3(0.0f));
//			for (uint32_t idx = 0; idx < indices.size(); idx += 3) {
//				uint32_t tri[3] = { indices[idx], indices[idx + 1], indices[idx + 2] };
//				glm::vec3 pos[3] = { positions[tri[0]], positions[tri[1]], positions[tri[2]] };
//
//				glm::vec3 u = pos[1] - pos[0];
//				glm::vec3 v = pos[2] - pos[0];
//				glm::vec3 normal = glm::cross(u, v);
//
//				normals[tri[0]] += normal;
//				normals[tri[1]] += normal;
//				normals[tri[2]] += normal;
//			}
//
//			for (auto& norm : normals)
//				norm = glm::normalize(norm);
//			return *this;
//		}
//
//		d3::Geometry generate_d3geom() {
//			d3::Geometry result;
//
//			lofx::Buffer ebo = lofx::createBuffer(lofx::BufferType::Index, indices.size() * sizeof(uint32_t));
//			lofx::send(&ebo, indices.data());
//
//			lofx::Buffer vbo = lofx::createBuffer(lofx::BufferType::Vertex, (positions.size() * 3 + normals.size() * 3) * sizeof(float));
//			lofx::send(&vbo, positions.data(), 0, positions.size() * 3 * sizeof(float));
//			lofx::send(&vbo, normals.data(), positions.size() * 3 * sizeof(float), normals.size() * 3 * sizeof(float));
//
//			{
//				lofx::BufferView view;
//				view.buffer = ebo;
//				view.length = ebo.size;
//				view.offset = 0;
//				view.stride = 0;
//
//				lofx::BufferAccessor accessor;
//				accessor.view = view;
//				accessor.offset = 0;
//				accessor.normalized = false;
//				accessor.component_type = lofx::AttributeType::UnsignedInt;
//				accessor.components = 1;
//				accessor.count = indices.size();
//				result.indices = accessor;
//			}
//
//			{
//				lofx::BufferView view;
//				view.buffer = vbo;
//				view.length = positions.size() * 3 * sizeof(float);
//				view.offset = 0;
//				view.stride = 0;
//
//				lofx::BufferAccessor accessor;
//				accessor.view = view;
//				accessor.offset = 0;
//				accessor.normalized = false;
//				accessor.component_type = lofx::AttributeType::Float;
//				accessor.components = 3;
//				accessor.count = positions.size();
//				result.attributePack.attributes[0] = accessor;
//			}
//
//			{
//				lofx::BufferView view;
//				view.buffer = vbo;
//				view.length = normals.size() * 3 * sizeof(float);
//				view.offset = positions.size() * 3 * sizeof(float);
//				view.stride = 0;
//
//				lofx::BufferAccessor accessor;
//				accessor.view = view;
//				accessor.offset = 0;
//				accessor.normalized = false;
//				accessor.component_type = lofx::AttributeType::Float;
//				accessor.components = 3;
//				accessor.count = normals.size();
//				result.attributePack.attributes[1] = accessor;
//			}
//
//			return result;
//		}
//	};
//
//	Geometry generate_quad(const glm::vec2& size) {
//		Geometry result;
//		result.indices = { 0, 1, 2, 0, 2, 3 };
//		result.positions = {
//			glm::vec3(0.0f, 0.0f, 0.0f),
//			glm::vec3(size.x, 0.0f, 0.0f),
//			glm::vec3(size.x, size.y, 0.0f),
//			glm::vec3(0.0f, size.y, 0.0f)
//		};
//		result.normals = {
//			glm::vec3(0.0f, 0.0f, 1.0f),
//			glm::vec3(0.0f, 0.0f, 1.0f),
//			glm::vec3(0.0f, 0.0f, 1.0f),
//			glm::vec3(0.0f, 0.0f, 1.0f)
//		};
//		result.uvs = {
//			glm::vec3(0.0f, 0.0f, 0.0f),
//			glm::vec3(1.0f, 0.0f, 0.0f),
//			glm::vec3(1.0f, 1.0f, 0.0f),
//			glm::vec3(0.0f, 1.0f, 0.0f)
//		};
//		return result;
//	}
//
//	Geometry generate_plane(const glm::uvec2& tilecount, const glm::vec2& tilesize) {
//		Geometry result;
//		glm::uvec2 count = tilecount + glm::uvec2(1, 1);
//		for (uint32_t y = 0; y < count.y; y++) {
//			for (uint32_t x = 0; x < count.x; x++) {
//				result.positions.push_back(glm::vec3((float) x * tilesize.x, (float) y * tilesize.y, 0.0f));
//				if (x < count.x - 1 && y < count.y - 1)
//					result.indices.insert(result.indices.end(), {
//						x + count.x * y,
//						(x + 1) + count.x * y,
//						(x + 1) + count.x * (y + 1),
//
//						x + count.x * y,
//						(x + 1) + count.x * (y + 1),
//						x + count.x * (y + 1)
//					});
//			}
//		}
//		return result;
//	}
//
//	Geometry quad_subdivision(const Geometry& input, uint32_t count = 1) {
//		if (count == 0)
//			return input;
//
//		const Geometry* src = &input;
//		Geometry result;
//		for (uint32_t i = 0; i < count; i++) {
//			Geometry temp;
//			uint32_t index_offset = 0;
//			for (uint32_t idx = 0; idx < src->indices.size(); idx += 6) {
//				glm::vec3 quad[4] = { src->positions[src->indices[idx]],
//					src->positions[src->indices[idx + 1]],
//					src->positions[src->indices[idx + 5]],
//					src->positions[src->indices[idx + 2]]
//				};
//
//				glm::vec3 bottom = (quad[0] + quad[1]) / 2.0f;
//				glm::vec3 top = (quad[2] + quad[3]) / 2.0f;
//				glm::vec3 left = (quad[0] + quad[2]) / 2.0f;
//				glm::vec3 right = (quad[1] + quad[3]) / 2.0f;
//				glm::vec3 middle = (quad[0] + quad[1] + quad[2] + quad[3]) / 4.0f;
//
//				temp.positions.insert(temp.positions.end(), {
//					quad[0], bottom, quad[1],
//					left, middle, right,
//					quad[2], top, quad[3]
//				});
//				temp.indices.insert(temp.indices.end(), {
//					index_offset + 0, index_offset + 1, index_offset + 4,
//					index_offset + 0, index_offset + 4, index_offset + 3,
//
//					index_offset + 1, index_offset + 2, index_offset + 5,
//					index_offset + 1, index_offset + 5, index_offset + 4,
//
//					index_offset + 3, index_offset + 4, index_offset + 7,
//					index_offset + 3, index_offset + 7, index_offset + 6,
//
//					index_offset + 4, index_offset + 5, index_offset + 8,
//					index_offset + 4, index_offset + 8, index_offset + 7
//				});
//				index_offset += 9;
//			}
//			result = temp;
//			src = &result;
//		}
//		return result;
//	}
//
//	struct NoiseModule {
//		std::list<NoiseModule*> attached_modules;
//		virtual float generate(float x, float y, float z) = 0;
//	};
//
//	struct RandomNoiseModule {
//		virtual float generate(float x, float y, float z) {
//			return random_range<float>(0.0f, 1.0f);
//		}
//	};
//
//	struct NoiseMap {
//		glm::vec3 tilesize;
//		glm::uvec3 tilecount;
//		std::vector<float> values;
//		NoiseModule* output_module;
//
//		void build() {
//			if (tilecount.y == 0) {
//				values.resize(tilecount.x);
//				for (uint32_t x = 0; x < tilecount.x; x++)
//					values[x] = output_module->generate(x * tilesize.x, 0, 0);
//			} else if (tilecount.z == 0) {
//				values.resize(tilecount.x * tilecount.y);
//				for (uint32_t y = 0; y < tilecount.y; y++)
//					for (uint32_t x = 0; x < tilecount.x; x++)
//						values[x + y * tilecount.x] = output_module->generate(x * tilesize.x, y * tilesize.y, 0);
//			} else {
//				values.resize(tilecount.x * tilecount.y * tilecount.z);
//				for (uint32_t z = 0; z < tilecount.z; z++)
//					for (uint32_t y = 0; y < tilecount.y; y++)
//						for (uint32_t x = 0; x < tilecount.x; x++)
//							values[x + y * tilecount.x + z * (tilecount.x * tilecount.y)] = output_module->generate(x * tilesize.x, y * tilesize.y, z * tilesize.z);
//			}
//		}
//
//		float get_value(uint32_t x, uint32_t y, uint32_t z) {
//			if (tilecount.y == 0)
//				return values[x];
//			else if (tilecount.z == 0)
//				return values[x + y * tilecount.x];
//			else
//				return values[x + y * tilecount.x + z * (tilecount.x * tilecount.y)];
//		}
//
//		float get_value(float x, float y, float z) {
//			if (tilecount.y == 0) {
//				uint32_t xsub = floor(x);
//				uint32_t xup = ceil(x);
//				float xfract = x - xsub;
//				return (1.0f - xfract) * values[xsub] + xfract * values[xup];
//			} else if (tilecount.z == 0) {
//				uint32_t xsub = floor(x);
//				uint32_t xup = ceil(x);
//				float xfract = x - xsub;
//
//				uint32_t ysub = floor(y);
//				uint32_t yup = ceil(y);
//				float yfract = y - ysub;
//
//				float bottom_left = values[xsub + ysub * tilecount.x];
//				float bottom_right = values[xsub + 1 + ysub * tilecount.x];
//				float top_left = values[xsub + (ysub + 1) * tilecount.x];
//				float top_right = values[xsub + 1 + (ysub + 1) * tilecount.x];
//
//				float dfx = bottom_right - bottom_left;
//				float dfy = top_left - bottom_left;
//				float dfxy = bottom_left + top_right - bottom_left - top_left;
//
//				return dfx * xfract + dfy * yfract + dfxy * xfract * yfract + bottom_left;
//			} else
//				return values[x + y * tilecount.x + z * (tilecount.x * tilecount.y)];
//		}
//	};
//
//	struct Terrain {
//		Geometry geometry;
//		glm::uvec2 tilecount;
//		glm::vec2 tilesize;
//
//		Terrain(const glm::uvec2& tilecount, const glm::vec2& tilesize)
//			: tilecount(tilecount)
//			, tilesize(tilesize) {
//			geometry = generate_plane(tilecount, tilesize);
//		}
//
//		Terrain& randomize_height(float power) {
//			glm::uvec2 count = tilecount + glm::uvec2(1, 1);
//			for (uint32_t y = 0; y < count.y; y++) {
//				for (uint32_t x = 0; x < count.x; x++) {
//					geometry.positions[x + count.x * y].z += random_range<float>(-power / 2.0f, power / 2.0f);
//				}
//			}
//			return *this;
//		}
//
//		Terrain& subdivide() {
//			Geometry newgeom;
//
//			// Generate vertices
//			{
//				uint32_t offset = 0;
//				glm::uvec2 vertexcount = tilecount + glm::uvec2(1, 1);
//				for (uint32_t y = 0; y < vertexcount.y; y++) {
//					std::vector<glm::vec3> newline;
//					std::vector<glm::vec3> newupline;
//					for (uint32_t x = 0; x < vertexcount.x; x++) {
//						newline.push_back(geometry.positions[offset]);
//
//						if (x < vertexcount.x - 1) {
//							glm::vec3 bottom = (geometry.positions[offset] + geometry.positions[offset + 1]) / 2.0f;
//							newline.push_back(bottom);
//						}
//						if (y < vertexcount.y - 1) {
//							glm::vec3 left = (geometry.positions[offset] + geometry.positions[offset + vertexcount.x]) / 2.0f;
//							newupline.push_back(left);
//						}
//						if (x < vertexcount.x - 1 && y < vertexcount.y - 1) {
//							glm::vec3 middle = (geometry.positions[offset]
//								+ geometry.positions[offset + 1]
//								+ geometry.positions[offset + vertexcount.x]
//								+ geometry.positions[offset + vertexcount.x + 1]) / 4.0f;
//							newupline.push_back(middle);
//						}
//
//						offset++;
//					}
//
//					newgeom.positions.insert(newgeom.positions.end(), newline.begin(), newline.end());
//					if (!newupline.empty())
//						newgeom.positions.insert(newgeom.positions.end(), newupline.begin(), newupline.end());
//				}
//			}
//
//			tilecount *= 2;
//			tilesize /= 2.0f;
//
//			// Generate indices
//			{
//				uint32_t linecount = 0;
//				uint32_t lineoffset = 0;
//				uint32_t offset = 0;
//				glm::uvec2 vertexcount = tilecount + glm::uvec2(1, 1);
//				while (linecount < vertexcount.y - 1) {
//					newgeom.indices.insert(newgeom.indices.end(), {
//						offset + 0, offset + 1, offset + vertexcount.x + 1,
//						offset + 0, offset + vertexcount.x + 1, offset + vertexcount.x
//					});
//					offset++;
//					lineoffset++;
//					if (lineoffset + 1 >= vertexcount.x) {
//						linecount++;
//						lineoffset = 0;
//						offset++;
//					}
//				}
//			}
//
//			geometry = newgeom;
//			return *this;
//		}
//	};
//}

#include <thread>

using namespace std::chrono_literals;
using clk = std::chrono::high_resolution_clock;
auto prtm = [](const std::string& msg, const clk::duration& duration, double mul = 1.0) { lut::trace("{0} : {1} sec\n", msg, (double) duration.count() / 1e9 * mul); };

//int main() {
//	lut::detail::loglevel = lut::LogLevel::Warn;
//	lut::detail::logtoconsole = true;
//
//	// Init LOFX
//	lofx::init(glm::u32vec2(1500, 1000), "4.4");
//	atexit(lofx::terminate);
//
//	geotools::Terrain terrain(glm::uvec2(2, 2), glm::vec2(10.f, 10.f));
//	float fp = 9.f;
//	for (int i = 1; i < 5; i++) {
//		terrain.randomize_height(fp).subdivide();
//		fp = .45f * fp;
//	}
//
//	terrain.geometry.recalculate_normals();
//
//	d3::Mesh plane_mesh;
//	plane_mesh.geometries.push_back(terrain.geometry.generate_d3geom());
//	terrain.geometry.clear();
//
//	d3::Node plane_node;
//	plane_node.mesh = &plane_mesh;
//	plane_node.transform = glm::mat4(1.0f);
//	plane_node.transform = glm::translate(plane_node.transform, glm::vec3(-(terrain.tilesize * glm::vec2(terrain.tilecount)) / 2.0f, 0.0f));
//
//	// Preparing shader program
//	lofx::Program wire_vp = lofx::createProgram(lofx::ShaderType::Vertex, { wireframe_shader_source::vertex });
//	lofx::Program wire_gp = lofx::createProgram(lofx::ShaderType::Geometry, { wireframe_shader_source::geometry });
//	lofx::Program wire_fp = lofx::createProgram(lofx::ShaderType::Fragment, { wireframe_shader_source::fragment });
//	lofx::Program terrain_vp = lofx::createProgram(lofx::ShaderType::Vertex, { terrain_shader_source::vertex });
//	lofx::Program terrain_fp = lofx::createProgram(lofx::ShaderType::Fragment, { terrain_shader_source::fragment });
//
//	// preparing shader pipelines
//	lofx::Pipeline wireframe_pipeline = lofx::createPipeline({ &wire_vp, &wire_fp, &wire_gp });
//	lofx::Pipeline terrain_pipeline = lofx::createPipeline({ &terrain_vp, &terrain_fp });
//
//	// Retrieving main framebuffer
//	lofx::Framebuffer fbo = lofx::defaultFramebuffer();
//
//	// Prepare camera and send uniforms
//	Camera camera;
//	camera.projection = glm::perspective(60.0f * glm::pi<float>() / 180.0f, 1.5f, 0.1f, 100.0f);
//	camera.view = glm::lookAt(glm::vec3(-5.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//	//lofx::send(&terrain_vp, lofx::Uniform("view", camera.view));
//	//lofx::send(&terrain_vp, lofx::Uniform("projection", camera.projection));
//	lofx::send(&wire_vp, lofx::Uniform("view", camera.view));
//	lofx::send(&wire_vp, lofx::Uniform("projection", camera.projection));
//
//	lofx::DrawProperties drawProperties;
//	drawProperties.fbo = &fbo;
//	//drawProperties.pipeline = &terrain_pipeline;
//	drawProperties.pipeline = &wireframe_pipeline;
//
//	// Loop while window is not closed
//	float time = 0.0f;
//	lofx::loop([&] {
//		camera.view = glm::lookAt(glm::vec3(-10.0f * cos(0.3f * time), 10.0f * sin(0.3f * time), 10.5f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//		//lofx::send(&terrain_vp, lofx::Uniform("view", camera.view));
//		lofx::send(&wire_vp, lofx::Uniform("view", camera.view));
//		time += 0.016f;
//
//		d3::render(&plane_node, drawProperties);
//		std::this_thread::sleep_for(16ms);
//	});
//
//	// Cleanup
//	lofx::release(&wireframe_pipeline);
//	d3::release(plane_mesh.geometries[0]);
//	
//	return 0;
//}

namespace gx {
	lofx::BufferAccessor createattrib(lofx::Buffer buffer, lofx::AttributeType type, std::size_t offset, std::size_t components, std::size_t length) {
		lofx::BufferView buffer_view;
		buffer_view.buffer = buffer;
		buffer_view.offset = 0;

		lofx::BufferAccessor accessor;
		accessor.view = buffer_view;
		accessor.offset = offset;
		accessor.component_type = type;
		accessor.components = components;
		accessor.count = length;

		return accessor;
	}
}

namespace de {
	namespace detail {
		struct quad_t {
			static const float vertices[];
			static const uint8_t indices[];

			lofx::Buffer vertex_buffer;
			lofx::Buffer index_buffer;
			lofx::BufferAccessor positions_accessor;
			lofx::BufferAccessor uvs_accessor;
			lofx::BufferAccessor index_accessor;
		};

		const float quad_t::vertices[] = {
			// positions
			-1.0f, -1.0f,
			1.0f, -1.0f,
			1.0f, 1.0f,
			-1.0f, 1.0f,
			// uvs
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f
		};

		const uint8_t quad_t::indices[] = {
			0, 1, 2,
			0, 2, 3
		};

		quad_t quad;

		namespace shader {
			const std::string vertex = R"(
				#version 440
				#extension GL_ARB_separate_shader_objects : enable
				#extension GL_ARB_shading_language_packing : enable

				layout (location = 0) in vec2 coordinate;
				layout (location = 1) in vec2 uv;

				out gl_PerVertex {
					vec4 gl_Position;
				};

				out vout_t {
					vec2 position;
					vec2 uv;
				} vout;

				uniform mat3 transform;

				void main() {
					vec3 coord = transform * vec3(coordinate, 1.0);
					gl_Position = vec4(coord.xy, 0.0, 1.0);
					vout.position = coord.xy;
					vout.uv = uv;
				}
			)";

			const std::string fragment = R"(
				#version 440
				#extension GL_ARB_separate_shader_objects : enable
				#extension GL_ARB_shading_language_packing : enable

				layout (location = 0) out vec4 colorout;

				uniform sampler2D checkerboard;

				in vout_t {
					vec2 position;
					vec2 uv;
				} fsin;

				void main() {
					vec4 texcolor = texture(checkerboard,fsin.uv);
					colorout = vec4(texcolor.rgb, 1.0);
				}
			)";
		}
	}

	struct Sprite {
		lofx::Texture texture;

		glm::uvec2 uv_offset = glm::uvec2();
		glm::vec2 uv_scale = glm::vec2(1.0f, 1.0f);
		float uv_rotation = 0.0f;
		glm::vec2 position = glm::vec2();
		glm::vec2 scale = glm::vec2(1.0f, 1.0f);
		float rotation = 0.0f;
	};

	struct View {
		glm::mat3 cached_transform;
		glm::vec2 center = glm::vec2(0.0f, 0.0f);
		float ratio = 1.0f;
		glm::vec2 scale = glm::vec2(1.0f, 1.0f);
		bool invalidated = true;
	};

	void apply(const lofx::Program* program, View* view) {
		if (view->invalidated) {
			view->cached_transform = glm::mat3(
				view->scale.x * 1.0f / view->ratio, 0.0f, 0.0f,
				0.0f, view->scale.y, 0.0f,
				-view->center.x, -view->center.y, 1.0f
			);
			view->invalidated = false;
		}
			
		lofx::send(program, lofx::Uniform("transform", view->cached_transform));
	}
}

int main(int argc, char** argv) {

	// init
	// ----
	lofx::setdbgCallback([](lofx::DebugLevel level, const std::string& msg) {
		std::string shown_level = "";
		switch (level) {
		case lofx::DebugLevel::Trace: shown_level = "trace"; break;
		case lofx::DebugLevel::Warn: shown_level = "warn"; break;
		case lofx::DebugLevel::Error: shown_level = "err"; break;
		}
		fprintf(stdout, "[lofx %s] : %s\n", shown_level.c_str(), msg.c_str());
	});
	lofx::init(glm::u32vec2(1500, 1000), "4.4");
	atexit(lofx::terminate);

	// checkerboard texture
	// --------------------
	uint8_t checkerboard_image[8 * 8 * 4];
	uint8_t count = 0;
	bool op = false;
	for (std::size_t i = 0; i < 8 * 8 * 4; i += 4) {
		checkerboard_image[i + 0] = op ? 255 : 0;
		checkerboard_image[i + 1] = op ? 255 : 0;
		checkerboard_image[i + 2] = op ? 255 : 0;
		checkerboard_image[i + 3] = 255;
		op = !op;
		if (++count > 7) {
			count = 0;
			op = !op;
		}
	}

	lofx::TextureSamplerParameters sampler_parameters;
	sampler_parameters.mag = lofx::TextureMagnificationFilter::Nearest;
	sampler_parameters.min = lofx::TextureMinificationFilter::Linear;
	sampler_parameters.s = lofx::TextureWrappping::Repeat;
	sampler_parameters.t = lofx::TextureWrappping::Repeat;

	lofx::TextureSampler sampler = lofx::createTextureSampler(sampler_parameters);
	lofx::Texture checkerboard_texture = lofx::createTexture(8, 8, 0, &sampler);
	lofx::send(&checkerboard_texture, checkerboard_image);

	// sprite quad
	// -----------
	float quad_vertices[] = {
		// positions
		-1.0f, -1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f,
		-1.0f, 1.0f,
		// uvs
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	uint8_t quad_indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	lofx::Buffer quad_vertex_buffer = lofx::createBuffer(lofx::BufferType::Vertex, sizeof(quad_vertices));
	lofx::Buffer quad_index_buffer = lofx::createBuffer(lofx::BufferType::Index, sizeof(quad_indices));
	lofx::send(&quad_vertex_buffer, quad_vertices);
	lofx::send(&quad_index_buffer, quad_indices);

	lofx::BufferAccessor positions_accessor = lofx::createBufferAccessor(quad_vertex_buffer, lofx::AttributeType::Float, 2, 4);
	lofx::BufferAccessor uvs_accessor = lofx::createBufferAccessor(quad_vertex_buffer, lofx::AttributeType::Float, 2, 4);
	lofx::BufferAccessor indices_accessor = lofx::createBufferAccessor(quad_index_buffer, lofx::AttributeType::UnsignedByte, 1, 6);

	lofx::AttributePack attribute_pack = lofx::buildSequentialAttributePack({ positions_accessor , uvs_accessor});

	// graphics pipeline
	// -----------------
	lofx::Program vertex_program = lofx::createProgram(lofx::ShaderType::Vertex, { de::detail::shader::vertex });
	lofx::Program fragment_program = lofx::createProgram(lofx::ShaderType::Fragment, { de::detail::shader::fragment });
	lofx::Pipeline pipeline = lofx::createPipeline({ &vertex_program, &fragment_program });

	// framebuffers
	// -----------------
	lofx::Texture offscreen_texture = lofx::createTexture(1500, 1000, 0, nullptr);
	lofx::Renderbuffer renderbuffer = lofx::createRenderBuffer(1500, 1000);
	lofx::Framebuffer framebuffer = lofx::createFramebuffer();
	framebuffer.renderbuffer = renderbuffer;
	framebuffer.attachments.push_back(offscreen_texture);
	lofx::build(&framebuffer);

	lofx::Framebuffer defaultframebuffer = lofx::defaultFramebuffer();

	// draw properties
	// ---------------
	lofx::DrawProperties drawproperties;
	drawproperties.fbo = &framebuffer;
	drawproperties.pipeline = &pipeline;
	drawproperties.attributes = &attribute_pack;
	drawproperties.indices = &indices_accessor;
	drawproperties.textures["checkerboard"] = &checkerboard_texture;

	lofx::DrawProperties postfxproperties;
	postfxproperties.fbo = &defaultframebuffer;
	postfxproperties.pipeline = &pipeline;
	postfxproperties.attributes = &attribute_pack;
	postfxproperties.indices = &indices_accessor;
	postfxproperties.textures["checkerboard"] = &offscreen_texture;

	glm::mat3 identity = glm::mat3(
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	);
	glm::mat3 transform = identity;

	// draw loop
	// ---------
	uint32_t counter = 0;
	lofx::loop([&] {
		counter++;

		lofx::clear(&framebuffer);
		transform = glm::mat3(
			2.0f / 1.5f, 0.0f, 0.0f,
			0.0f, 2.0f, 0.0f,
			0.5f * cos((float)counter / 100.0f), 0.5f * sin((float)counter / 100.f), 1.0f
		);
		lofx::send(&vertex_program, lofx::Uniform("transform", transform));
		lofx::draw(drawproperties);

		lofx::clear(&defaultframebuffer);
		lofx::send(&vertex_program, lofx::Uniform("transform", identity));
		lofx::draw(postfxproperties);

		std::this_thread::sleep_for(16ms);
	});

	return 0;
}

//namespace postfx_shader_source {
//
//	// Input data
//	const std::string vertex = R"(
//#version 440
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_packing : enable
//
//layout (location = 0) in vec3 coordinate;
//
//out gl_PerVertex {
//	vec4 gl_Position;
//};
//
//out VSOut {
//	vec3 coordinate;
//} vsout;
//
//uniform mat4 model;
//
//void main() {	
//	gl_Position = model * vec4(coordinate, 1.0);
//	vsout.coordinate = coordinate.xyz;
//}
//
//)";
//
//	const std::string fragment = R"(
//#version 440
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_packing : enable
//
//in VSOut {
//	vec3 coordinate;
//} fsin;
//
//layout (location = 0) out float xout;
//layout (location = 1) out float yout;
//layout (location = 2) out float zout;
//
//uniform sampler2DArray input_texture;
//
//uniform float strength;
//uniform float exponent;
//uniform float k;
//uniform float k_pow_e;
//uniform float rcpYWhite2;
//uniform float srcGain;
//
//void main() {
//	vec2 coords = vec2(fsin.coordinate.x, -fsin.coordinate.y);
//
//	float X = texture(input_texture, vec3(coords, 0)).x;
//	float Y = texture(input_texture, vec3(coords, 1)).x;
//	float Z = texture(input_texture, vec3(coords, 2)).x;
//
//	float luminance = srcGain * Y;
//
//	float f_nom = exp(exponent * log(luminance * rcpYWhite2));
//	float f_denom = 1.0f + exp(exponent * log(k * luminance));
//	float f = (k_pow_e + f_nom) / f_denom;
//	//float reinhard_factor = exp(strength * log(f));
//	float reinhard_factor = 1.0f;
//	
//	vec3 xyz = vec3(X, Y, Z);
//	xout = xyz.x;
//	yout = xyz.y;
//	zout = xyz.z;
//}
//
//)";
//
//	const std::string passthrough_fragment = R"(
//#version 440
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_packing : enable
//
//in VSOut {
//	vec3 coordinate;
//} fsin;
//
//// layout (location = 0) out vec4 colorout;
//
//layout (location = 0) out float xout;
//layout (location = 1) out float yout;
//layout (location = 2) out float zout;
//
//uniform sampler2DArray input_texture;
//
//const mat3 XYZ_to_RGB = mat3(
//	3.2404542, -1.5371385, -0.4985314,
//	-0.9692660,  1.8760108,  0.0415560,
//	 0.0556434, -0.2040259,  1.0572252
//);
//
//void main() {
//	vec2 coords = vec2(fsin.coordinate.x, fsin.coordinate.y);
//
//	float X = texture(input_texture, vec3(coords, 0)).x;
//	float Y = texture(input_texture, vec3(coords, 1)).x;
//	float Z = texture(input_texture, vec3(coords, 2)).x;
//	
//	vec3 xyz = vec3(X, Y, Z);
//	//vec3 rgb = pow(XYZ_to_RGB * xyz, vec3(1.0/2.2));
//	//colorout = vec4(clamp(rgb, vec3(0.0), vec3(1.0)), 1.0);
//
//	xout = X;
//	yout = Y;
//	zout = Z;
//}
//
//)";
//
//}
//
//#include "lodepng/lodepng.h"
//
//#define TINYEXR_IMPLEMENTATION
//#include "ext/tinyexr.h"
//
//#include <chrono>
//using clk = std::chrono::high_resolution_clock;
//inline void print_time(const std::string& str, const clk::duration& diff, double factor = 1.0) {
//	fprintf(stdout, "%s : %g\n ms", str.c_str(), ((double) diff.count()) / (factor * 1e6));
//};

//void saveEXR(const std::string& filepath, uint32_t width, uint32_t height, const std::vector<const float*>& buffers)
//{
//	EXRHeader header;
//	InitEXRHeader(&header);
//
//	EXRImage image;
//	InitEXRImage(&image);
//
//	image.num_channels = buffers.size();
//	image.images = (uint8_t**) buffers.data();
//	image.width = width;
//	image.height = height;
//
//	std::vector<EXRChannelInfo> channelInfos(image.num_channels);
//	std::vector<int> pixelTypes(image.num_channels);
//	std::vector<int> requestedPixelTypes(image.num_channels);
//	if (channelInfos.size() == 3) {
//		channelInfos[0].name[0] = 'X';
//		channelInfos[0].name[1] = '\0';
//		channelInfos[1].name[0] = 'Y';
//		channelInfos[1].name[1] = '\0';
//		channelInfos[2].name[0] = 'Z';
//		channelInfos[2].name[1] = '\0';
//
//		pixelTypes[0] = TINYEXR_PIXELTYPE_FLOAT;
//		pixelTypes[1] = TINYEXR_PIXELTYPE_FLOAT;
//		pixelTypes[2] = TINYEXR_PIXELTYPE_FLOAT;
//	}
//	requestedPixelTypes = pixelTypes;
//
//	header.num_channels = image.num_channels;
//	header.channels = channelInfos.data();
//	header.pixel_types = pixelTypes.data();
//	header.requested_pixel_types = requestedPixelTypes.data();
//
//	const char* err;
//	int ret = SaveEXRImageToFile(&image, &header, filepath.c_str(), &err);
//	if (ret != 0) {
//		fprintf(stderr, "error saving exr to %s : %s", filepath.c_str(), err);
//		return;
//	}
//
//	//FreeEXRHeader(&header);
//	//FreeEXRImage(&image);
//}
//
//int main() {
//	lut::detail::loglevel = lut::LogLevel::Warn;
//	lut::detail::logtoconsole = true;
//
//	/////////////////////////////////////////////////////////////////////////////////////////
//	////////////// EXR LOADING
//
//	const std::string exr_filepath = "resources\\images\\null.exr";
//	const std::string output_filepath = exr_filepath.substr(0, exr_filepath.find(".exr")) + "-mod.exr";
//	int ret = 0;
//	const char* err;
//
//	// Parse Version
//	EXRVersion exr_version;
//	ret = ParseEXRVersionFromFile(&exr_version, exr_filepath.c_str());
//	if (ret != 0) {
//		fprintf(stderr, "could not read exr file : %s\n", exr_filepath.c_str());
//		return -1;
//	}
//
//	if (exr_version.multipart) {
//		fprintf(stderr, "this application cannot read multipart exr files\n");
//		return -1;
//	}
//
//	// Parse Header
//	EXRHeader exr_header;
//	InitEXRHeader(&exr_header);
//	ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, exr_filepath.c_str(), &err);
//	if (ret != 0) {
//		fprintf(stderr, "error reading exr file : %s\n", err);
//		return -1;
//	}
//
//	// Load actual image
//	EXRImage exr_image;
//	InitEXRImage(&exr_image);
//	ret = LoadEXRImageFromFile(&exr_image, &exr_header, exr_filepath.c_str(), &err);
//	if (ret != 0) {
//		fprintf(stderr, "error loading exr image : %s\n", err);
//		return -1;
//	}
//
//	float xyzrgb_matrix[] = {
//		2.0413690f, -0.5649464f, -0.3446944f,
//		-0.9692660f, 1.8760108f, 0.0415560f,
//		0.0134474f, -0.1183897f, 1.0154096f
//	};
//
//	// Prepare buffers
//	std::size_t buffer_length = exr_image.width * exr_image.height;
//	std::vector<uint8_t> rgb_buffer(buffer_length * 3);
//	const float* xbuf = (const float*)exr_image.images[0];
//	const float* ybuf = (const float*)exr_image.images[1];
//	const float* zbuf = (const float*)exr_image.images[2];
//
//	/////////////////////////////////////////////////////////////////////////////////////////
//	////////////// LOFX RENDERING
//
//	// Init LOFX
//	uint32_t window_width = exr_image.width;
//	uint32_t window_height = exr_image.height;
//	lofx::init(glm::u32vec2(window_width, window_height), "4.4", true);
//	atexit(lofx::terminate);
//
//	// Simple fullscreen quad
//	d3::Geometry quad = geotools::generate_quad(glm::vec2(1.0f, 1.0f))
//		.generate_d3geom();
//	glm::mat4 rectif = glm::mat4(1.0f);
//	rectif = glm::scale(rectif, glm::vec3(2.0f, 2.0f, 1.0f));
//	rectif = glm::translate(rectif, glm::vec3(-0.5f, -0.5f, 0.0f));
//
//	// Preparing shader program
//	lofx::Program passthrough_vertex_program = lofx::createProgram(lofx::ShaderType::Vertex, { postfx_shader_source::vertex });
//	lofx::Program reinhard_global_program = lofx::createProgram(lofx::ShaderType::Fragment, { postfx_shader_source::fragment });
//	lofx::Program passthrough_fragment_program = lofx::createProgram(lofx::ShaderType::Fragment, { postfx_shader_source::passthrough_fragment });
//
//	lofx::Pipeline postfx_pipeline = lofx::createPipeline();
//	lofx::Pipeline final_pipeline = lofx::createPipeline();
//	postfx_pipeline.stages = { &passthrough_vertex_program, &reinhard_global_program };
//	final_pipeline.stages = { &passthrough_vertex_program, &passthrough_fragment_program };
//
//	// Texture sampler
//	lofx::TextureSamplerParameters samplerParameters;
//	samplerParameters.mag = lofx::TextureMagnificationFilter::Nearest;
//	samplerParameters.min = lofx::TextureMinificationFilter::Linear;
//	samplerParameters.s = lofx::TextureWrappping::Clamp;
//	samplerParameters.t = lofx::TextureWrappping::Clamp;
//	lofx::TextureSampler sampler = lofx::createTextureSampler(samplerParameters);
//
//	// Texture
//	lofx::Texture texture = lofx::createTexture(window_width, window_height, 3, &sampler, lofx::TextureTarget::Texture2dArray, lofx::TextureInternalFormat::R32F);
//	lofx::send(&texture, xbuf, glm::u32vec3(0, 0, 0), glm::u32vec3(window_width, window_height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	lofx::send(&texture, ybuf, glm::u32vec3(0, 0, 1), glm::u32vec3(window_width, window_height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	lofx::send(&texture, zbuf, glm::u32vec3(0, 0, 2), glm::u32vec3(window_width, window_height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//
//	// Framebuffer
//	lofx::Renderbuffer renderbuffer = lofx::createRenderBuffer(window_width, window_height);
//	lofx::Texture framebufferTexture = lofx::createTexture(window_width, window_height, 3, nullptr, lofx::TextureTarget::Texture2dArray, lofx::TextureInternalFormat::R32F);
//	lofx::Framebuffer framebuffer = lofx::createFramebuffer();
//	framebuffer.renderbuffer = renderbuffer;
//	framebuffer.attachments = { framebufferTexture };
//	lofx::build(&framebuffer);
//
//	// Preparing draw properties
//	lofx::DrawProperties offscreenDrawProperties;
//	offscreenDrawProperties.pipeline = &final_pipeline;
//	offscreenDrawProperties.textures["input_texture"] = &texture;
//	offscreenDrawProperties.fbo = &framebuffer;
//
//	lofx::DrawProperties drawProperties;
//	drawProperties.pipeline = &final_pipeline;
//	drawProperties.textures["input_texture"] = &framebufferTexture;
//
//	/////////////////////////////////////////////////////////////////////////////////////////
//	// REINHARD GLOBAL CONSTANTS
//
//	// Initial constants
//	const float stops = 1.0f;
//	const float strength = 1.0f;
//
//	// Global reinhard based tonemapping. We do not do the auto-exposure
//	const float YWhite = pow(2.f, stops);
//	const float rcpYWhite2 = 1.f / (YWhite * YWhite);
//	const float a = 0.18f;
//
//	// Strength : 1 for Reinhard, lower for more subtle effect.
//	// 0.01 added for avoiding low values numerically instable
//	const float exponent = 1.01f / (strength + 0.01f);
//	const float k = pow((1.f - pow(a / (YWhite * YWhite), exponent)) / (1.f - pow(a, exponent)), strength);
//	const float k_pow_e = pow(k, exponent);
//
//	lofx::send(&reinhard_global_program, lofx::Uniform("strength", strength));
//	lofx::send(&reinhard_global_program, lofx::Uniform("exponent", exponent));
//	lofx::send(&reinhard_global_program, lofx::Uniform("k", k));
//	lofx::send(&reinhard_global_program, lofx::Uniform("k_pow_e", k_pow_e));
//	lofx::send(&reinhard_global_program, lofx::Uniform("rcpYWhite2", rcpYWhite2));
//	lofx::send(&reinhard_global_program, lofx::Uniform("srcGain", 1.0f));
//
//	/////////////////////////////////////////////////////////////////////////////////////////
//	// LOOP
//
//	//std::size_t count = 0;
//	//clk::time_point tp_start = clk::now();
//	//lofx::loop([&] {
//	//	lofx::send(&texture, xbuf, glm::u32vec3(0, 0, 0), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	//	lofx::send(&texture, ybuf, glm::u32vec3(0, 0, 1), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	//	lofx::send(&texture, zbuf, glm::u32vec3(0, 0, 2), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//
//	//	lofx::send(&passthrough_vertex_program, lofx::Uniform("model", rectif));
//	//	d3::render(&quad, offscreenDrawProperties);
//	//	d3::render(&quad, drawProperties);
//
//	//	float* xbuf_ret = (float*)lofx::read(&framebuffer, 0, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	//	float* ybuf_ret = (float*)lofx::read(&framebuffer, 1, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	//	float* zbuf_ret = (float*)lofx::read(&framebuffer, 2, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	//	delete xbuf_ret;
//	//	delete ybuf_ret;
//	//	delete zbuf_ret;
//
//	//	count++;
//	//	if (count > 50) {
//	//		count = 0;
//	//		clk::time_point tp_end = clk::now();
//	//		print_time("OpenCL Reinhard global filter time", tp_end - tp_start, 50.0);
//	//		tp_start = clk::now();
//	//	}
//	//});
//
//	clk::time_point tp_start = clk::now();
//	lofx::send(&texture, xbuf, glm::u32vec3(0, 0, 0), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	lofx::send(&texture, ybuf, glm::u32vec3(0, 0, 1), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	lofx::send(&texture, zbuf, glm::u32vec3(0, 0, 2), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//
//	lofx::send(&passthrough_vertex_program, lofx::Uniform("model", rectif));
//	d3::render(&quad, offscreenDrawProperties);
//	//d3::render(&quad, drawProperties);
//
//	float* xbuf_ret = (float*) lofx::read(&framebuffer, 0, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	float* ybuf_ret = (float*) lofx::read(&framebuffer, 1, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//	float* zbuf_ret = (float*) lofx::read(&framebuffer, 2, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
//
//	clk::time_point tp_end = clk::now();
//	print_time("postfx time", tp_end - tp_start);
//
//	saveEXR(output_filepath, exr_image.width, exr_image.height, { xbuf_ret, ybuf_ret, zbuf_ret });
//
//	delete xbuf_ret;
//	delete ybuf_ret;
//	delete zbuf_ret;
//
//	/////////////////////////////////////////////////////////////////////////////////////////
//	// CLEANUP
//
//	lofx::release(&sampler);
//	lofx::release(&texture);
//	lofx::release(&postfx_pipeline);
//	d3::release(quad);
//
//	return 0;
//}