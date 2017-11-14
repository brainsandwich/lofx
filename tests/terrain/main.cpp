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

	void release(Geometry& geometry) {
		lofx::release(&geometry.indices.view.buffer);
		for (auto& pair : geometry.attributePack.attributes)
			lofx::release(&pair.second.view.buffer);
	}

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

namespace terrain_shader_source {

	// Input data
	const std::string vertex = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

layout (location = 0) in vec3 coordinate;
layout (location = 1) in vec3 normal;

out gl_PerVertex {
	vec4 gl_Position;
};

out output_block_t {
	vec3 coordinate;
	vec3 normal;
} vsout;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

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
	vsout.normal = mat3(transpose(inverse(model))) * normal;
}

)";

	const std::string fragment = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in input_block_t {
	vec3 coordinate;
	vec3 normal;
} fsin;

layout (location = 0) out vec4 colorout;

uniform float time;

void main() {
	colorout = vec4(normalize(fsin.normal), 1.0);
}

)";

}

#include <glm/gtx/rotate_vector.hpp>
#include <cmath>

template <typename T>
T random_range(const T& min, const T& max) {
	T init = (T) rand() / (T) RAND_MAX;
	return init * (max - min) + min;
}

namespace geotools {
	struct Geometry {
		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;
		std::vector<uint32_t> indices;

		void clear() {
			positions.clear();
			normals.clear();
			uvs.clear();
			indices.clear();
			positions.shrink_to_fit();
			normals.shrink_to_fit();
			uvs.shrink_to_fit();
			indices.shrink_to_fit();
		}

		Geometry& translate(const glm::vec3& value) {
			for (auto& pos : positions)
				pos += value;
			return *this;
		}

		Geometry& rotate(float value, const glm::vec3& axis) {
			for (auto& pos : positions)
				pos = glm::rotate(pos, value, axis);
			for (auto& nor : normals)
				nor = glm::rotate(nor, value, axis);
			return *this;
		}

		Geometry& scale(const glm::vec3& value) {
			for (auto& pos : positions)
				pos *= value;
			return *this;
		}

		Geometry& recalculate_normals() {
			normals.clear();
			normals.resize(positions.size(), glm::vec3(0.0f));
			for (uint32_t idx = 0; idx < indices.size(); idx += 3) {
				uint32_t tri[3] = { indices[idx], indices[idx + 1], indices[idx + 2] };
				glm::vec3 pos[3] = { positions[tri[0]], positions[tri[1]], positions[tri[2]] };

				glm::vec3 u = pos[1] - pos[0];
				glm::vec3 v = pos[2] - pos[0];
				glm::vec3 normal = glm::cross(u, v);

				normals[tri[0]] += normal;
				normals[tri[1]] += normal;
				normals[tri[2]] += normal;
			}

			for (auto& norm : normals)
				norm = glm::normalize(norm);
			return *this;
		}

		d3::Geometry generate_d3geom() {
			d3::Geometry result;

			lofx::Buffer ebo = lofx::createBuffer(lofx::BufferType::Index, indices.size() * sizeof(uint32_t));
			lofx::send(&ebo, indices.data());

			lofx::Buffer vbo = lofx::createBuffer(lofx::BufferType::Vertex, (positions.size() * 3 + normals.size() * 3) * sizeof(float));
			lofx::send(&vbo, positions.data(), 0, positions.size() * 3 * sizeof(float));
			lofx::send(&vbo, normals.data(), positions.size() * 3 * sizeof(float), normals.size() * 3 * sizeof(float));

			{
				lofx::BufferView view;
				view.buffer = ebo;
				view.length = ebo.size;
				view.offset = 0;
				view.stride = 0;

				lofx::BufferAccessor accessor;
				accessor.view = view;
				accessor.offset = 0;
				accessor.normalized = false;
				accessor.component_type = lofx::AttributeType::UnsignedInt;
				accessor.components = 1;
				accessor.count = indices.size();
				result.indices = accessor;
			}

			{
				lofx::BufferView view;
				view.buffer = vbo;
				view.length = positions.size() * 3 * sizeof(float);
				view.offset = 0;
				view.stride = 0;

				lofx::BufferAccessor accessor;
				accessor.view = view;
				accessor.offset = 0;
				accessor.normalized = false;
				accessor.component_type = lofx::AttributeType::Float;
				accessor.components = 3;
				accessor.count = positions.size();
				result.attributePack.attributes[0] = accessor;
			}

			{
				lofx::BufferView view;
				view.buffer = vbo;
				view.length = normals.size() * 3 * sizeof(float);
				view.offset = positions.size() * 3 * sizeof(float);
				view.stride = 0;

				lofx::BufferAccessor accessor;
				accessor.view = view;
				accessor.offset = 0;
				accessor.normalized = false;
				accessor.component_type = lofx::AttributeType::Float;
				accessor.components = 3;
				accessor.count = normals.size();
				result.attributePack.attributes[1] = accessor;
			}

			return result;
		}
	};

	Geometry generate_quad(const glm::vec2& size) {
		Geometry result;
		result.indices = { 0, 1, 2, 0, 2, 3 };
		result.positions = {
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(size.x, 0.0f, 0.0f),
			glm::vec3(size.x, size.y, 0.0f),
			glm::vec3(0.0f, size.y, 0.0f)
		};
		result.normals = {
			glm::vec3(0.0f, 0.0f, 1.0f),
			glm::vec3(0.0f, 0.0f, 1.0f),
			glm::vec3(0.0f, 0.0f, 1.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		};
		result.uvs = {
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(1.0f, 0.0f, 0.0f),
			glm::vec3(1.0f, 1.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		};
		return result;
	}

	Geometry generate_plane(const glm::uvec2& tilecount, const glm::vec2& tilesize) {
		Geometry result;
		glm::uvec2 count = tilecount + glm::uvec2(1, 1);
		for (uint32_t y = 0; y < count.y; y++) {
			for (uint32_t x = 0; x < count.x; x++) {
				result.positions.push_back(glm::vec3((float) x * tilesize.x, (float) y * tilesize.y, 0.0f));
				if (x < count.x - 1 && y < count.y - 1)
					result.indices.insert(result.indices.end(), {
						x + count.x * y,
						(x + 1) + count.x * y,
						(x + 1) + count.x * (y + 1),

						x + count.x * y,
						(x + 1) + count.x * (y + 1),
						x + count.x * (y + 1)
					});
			}
		}
		return result;
	}

	Geometry quad_subdivision(const Geometry& input, uint32_t count = 1) {
		if (count == 0)
			return input;

		const Geometry* src = &input;
		Geometry result;
		for (uint32_t i = 0; i < count; i++) {
			Geometry temp;
			uint32_t index_offset = 0;
			for (uint32_t idx = 0; idx < src->indices.size(); idx += 6) {
				glm::vec3 quad[4] = { src->positions[src->indices[idx]],
					src->positions[src->indices[idx + 1]],
					src->positions[src->indices[idx + 5]],
					src->positions[src->indices[idx + 2]]
				};

				glm::vec3 bottom = (quad[0] + quad[1]) / 2.0f;
				glm::vec3 top = (quad[2] + quad[3]) / 2.0f;
				glm::vec3 left = (quad[0] + quad[2]) / 2.0f;
				glm::vec3 right = (quad[1] + quad[3]) / 2.0f;
				glm::vec3 middle = (quad[0] + quad[1] + quad[2] + quad[3]) / 4.0f;

				temp.positions.insert(temp.positions.end(), {
					quad[0], bottom, quad[1],
					left, middle, right,
					quad[2], top, quad[3]
				});
				temp.indices.insert(temp.indices.end(), {
					index_offset + 0, index_offset + 1, index_offset + 4,
					index_offset + 0, index_offset + 4, index_offset + 3,

					index_offset + 1, index_offset + 2, index_offset + 5,
					index_offset + 1, index_offset + 5, index_offset + 4,

					index_offset + 3, index_offset + 4, index_offset + 7,
					index_offset + 3, index_offset + 7, index_offset + 6,

					index_offset + 4, index_offset + 5, index_offset + 8,
					index_offset + 4, index_offset + 8, index_offset + 7
				});
				index_offset += 9;
			}
			result = temp;
			src = &result;
		}
		return result;
	}

	struct NoiseModule {
		std::list<NoiseModule*> attached_modules;
		virtual float generate(float x, float y, float z) = 0;
	};

	struct RandomNoiseModule {
		virtual float generate(float x, float y, float z) {
			return random_range<float>(0.0f, 1.0f);
		}
	};

	struct NoiseMap {
		glm::vec3 tilesize;
		glm::uvec3 tilecount;
		std::vector<float> values;
		NoiseModule* output_module;

		void build() {
			if (tilecount.y == 0) {
				values.resize(tilecount.x);
				for (uint32_t x = 0; x < tilecount.x; x++)
					values[x] = output_module->generate(x * tilesize.x, 0, 0);
			} else if (tilecount.z == 0) {
				values.resize(tilecount.x * tilecount.y);
				for (uint32_t y = 0; y < tilecount.y; y++)
					for (uint32_t x = 0; x < tilecount.x; x++)
						values[x + y * tilecount.x] = output_module->generate(x * tilesize.x, y * tilesize.y, 0);
			} else {
				values.resize(tilecount.x * tilecount.y * tilecount.z);
				for (uint32_t z = 0; z < tilecount.z; z++)
					for (uint32_t y = 0; y < tilecount.y; y++)
						for (uint32_t x = 0; x < tilecount.x; x++)
							values[x + y * tilecount.x + z * (tilecount.x * tilecount.y)] = output_module->generate(x * tilesize.x, y * tilesize.y, z * tilesize.z);
			}
		}

		float get_value(uint32_t x, uint32_t y, uint32_t z) {
			if (tilecount.y == 0)
				return values[x];
			else if (tilecount.z == 0)
				return values[x + y * tilecount.x];
			else
				return values[x + y * tilecount.x + z * (tilecount.x * tilecount.y)];
		}

		float get_value(float x, float y, float z) {
			if (tilecount.y == 0) {
				uint32_t xsub = floor(x);
				uint32_t xup = ceil(x);
				float xfract = x - xsub;
				return (1.0f - xfract) * values[xsub] + xfract * values[xup];
			} else if (tilecount.z == 0) {
				uint32_t xsub = floor(x);
				uint32_t xup = ceil(x);
				float xfract = x - xsub;

				uint32_t ysub = floor(y);
				uint32_t yup = ceil(y);
				float yfract = y - ysub;

				float bottom_left = values[xsub + ysub * tilecount.x];
				float bottom_right = values[xsub + 1 + ysub * tilecount.x];
				float top_left = values[xsub + (ysub + 1) * tilecount.x];
				float top_right = values[xsub + 1 + (ysub + 1) * tilecount.x];

				float dfx = bottom_right - bottom_left;
				float dfy = top_left - bottom_left;
				float dfxy = bottom_left + top_right - bottom_left - top_left;

				return dfx * xfract + dfy * yfract + dfxy * xfract * yfract + bottom_left;
			} else
				return values[x + y * tilecount.x + z * (tilecount.x * tilecount.y)];
		}
	};

	struct Terrain {
		Geometry geometry;
		glm::uvec2 tilecount;
		glm::vec2 tilesize;

		Terrain(const glm::uvec2& tilecount, const glm::vec2& tilesize)
			: tilecount(tilecount)
			, tilesize(tilesize) {
			geometry = generate_plane(tilecount, tilesize);
		}

		Terrain& randomize_height(float power) {
			glm::uvec2 count = tilecount + glm::uvec2(1, 1);
			for (uint32_t y = 0; y < count.y; y++) {
				for (uint32_t x = 0; x < count.x; x++) {
					geometry.positions[x + count.x * y].z += random_range<float>(-power / 2.0f, power / 2.0f);
				}
			}
			return *this;
		}

		Terrain& subdivide() {
			Geometry newgeom;

			// Generate vertices
			{
				uint32_t offset = 0;
				glm::uvec2 vertexcount = tilecount + glm::uvec2(1, 1);
				for (uint32_t y = 0; y < vertexcount.y; y++) {
					std::vector<glm::vec3> newline;
					std::vector<glm::vec3> newupline;
					for (uint32_t x = 0; x < vertexcount.x; x++) {
						newline.push_back(geometry.positions[offset]);

						if (x < vertexcount.x - 1) {
							glm::vec3 bottom = (geometry.positions[offset] + geometry.positions[offset + 1]) / 2.0f;
							newline.push_back(bottom);
						}
						if (y < vertexcount.y - 1) {
							glm::vec3 left = (geometry.positions[offset] + geometry.positions[offset + vertexcount.x]) / 2.0f;
							newupline.push_back(left);
						}
						if (x < vertexcount.x - 1 && y < vertexcount.y - 1) {
							glm::vec3 middle = (geometry.positions[offset]
								+ geometry.positions[offset + 1]
								+ geometry.positions[offset + vertexcount.x]
								+ geometry.positions[offset + vertexcount.x + 1]) / 4.0f;
							newupline.push_back(middle);
						}

						offset++;
					}

					newgeom.positions.insert(newgeom.positions.end(), newline.begin(), newline.end());
					if (!newupline.empty())
						newgeom.positions.insert(newgeom.positions.end(), newupline.begin(), newupline.end());
				}
			}

			tilecount *= 2;
			tilesize /= 2.0f;

			// Generate indices
			{
				uint32_t linecount = 0;
				uint32_t lineoffset = 0;
				uint32_t offset = 0;
				glm::uvec2 vertexcount = tilecount + glm::uvec2(1, 1);
				while (linecount < vertexcount.y - 1) {
					newgeom.indices.insert(newgeom.indices.end(), {
						offset + 0, offset + 1, offset + vertexcount.x + 1,
						offset + 0, offset + vertexcount.x + 1, offset + vertexcount.x
					});
					offset++;
					lineoffset++;
					if (lineoffset + 1 >= vertexcount.x) {
						linecount++;
						lineoffset = 0;
						offset++;
					}
				}
			}

			geometry = newgeom;
			return *this;
		}
	};
}

#include <thread>

using namespace std::chrono_literals;
using clk = std::chrono::high_resolution_clock;
auto prtm = [](const std::string& msg, const clk::duration& duration, double mul = 1.0) { lofx::detail::trace("{0} : {1} sec\n", msg, (double) duration.count() / 1e9 * mul); };

int main() {
	// Init LOFX
	lofx::init(glm::u32vec2(1500, 1000), "4.4");
	atexit(lofx::terminate);

	geotools::Terrain terrain(glm::uvec2(2, 2), glm::vec2(10.f, 10.f));
	float fp = 9.f;
	for (int i = 1; i < 5; i++) {
		terrain.randomize_height(fp).subdivide();
		fp = .45f * fp;
	}

	terrain.geometry.recalculate_normals();

	d3::Mesh plane_mesh;
	plane_mesh.geometries.push_back(terrain.geometry.generate_d3geom());
	terrain.geometry.clear();

	d3::Node plane_node;
	plane_node.mesh = &plane_mesh;
	plane_node.transform = glm::mat4(1.0f);
	plane_node.transform = glm::translate(plane_node.transform, glm::vec3(-(terrain.tilesize * glm::vec2(terrain.tilecount)) / 2.0f, 0.0f));

	// Preparing shader program
	lofx::Program wire_vp = lofx::createProgram(lofx::ShaderType::Vertex, { wireframe_shader_source::vertex });
	lofx::Program wire_gp = lofx::createProgram(lofx::ShaderType::Geometry, { wireframe_shader_source::geometry });
	lofx::Program wire_fp = lofx::createProgram(lofx::ShaderType::Fragment, { wireframe_shader_source::fragment });
	lofx::Program terrain_vp = lofx::createProgram(lofx::ShaderType::Vertex, { terrain_shader_source::vertex });
	lofx::Program terrain_fp = lofx::createProgram(lofx::ShaderType::Fragment, { terrain_shader_source::fragment });

	// preparing shader pipelines
	lofx::Pipeline wireframe_pipeline = lofx::createPipeline({ &wire_vp, &wire_fp, &wire_gp });
	lofx::Pipeline terrain_pipeline = lofx::createPipeline({ &terrain_vp, &terrain_fp });

	// Retrieving main framebuffer
	lofx::Framebuffer fbo = lofx::defaultFramebuffer();

	// Prepare camera and send uniforms
	Camera camera;
	camera.projection = glm::perspective(60.0f * glm::pi<float>() / 180.0f, 1.5f, 0.1f, 100.0f);
	camera.view = glm::lookAt(glm::vec3(-5.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//lofx::send(&terrain_vp, lofx::Uniform("view", camera.view));
	//lofx::send(&terrain_vp, lofx::Uniform("projection", camera.projection));
	lofx::send(&wire_vp, lofx::Uniform("view", camera.view));
	lofx::send(&wire_vp, lofx::Uniform("projection", camera.projection));

	lofx::DrawProperties drawProperties;
	drawProperties.fbo = &fbo;
	//drawProperties.pipeline = &terrain_pipeline;
	drawProperties.pipeline = &wireframe_pipeline;

	// Loop while window is not closed
	float time = 0.0f;
	lofx::loop([&] {
		camera.view = glm::lookAt(glm::vec3(-10.0f * cos(0.3f * time), 10.0f * sin(0.3f * time), 10.5f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		//lofx::send(&terrain_vp, lofx::Uniform("view", camera.view));
		lofx::send(&wire_vp, lofx::Uniform("view", camera.view));
		time += 0.016f;

		d3::render(&plane_node, drawProperties);
		std::this_thread::sleep_for(16ms);
	});

	// Cleanup
	lofx::release(&wireframe_pipeline);
	d3::release(plane_mesh.geometries[0]);
	
	return 0;
}
