#include "LUT/lut.hpp"
#include "LOFX/lofx.hpp"

#include <fstream>
#include <filesystem>

namespace fs = std::experimental::filesystem;

namespace d3 {

	struct Geometry {
		std::vector<uint32_t> indices;
		std::vector<char> vertices;
		lofx::Buffer vbo, ebo;
		lofx::AttributePack pack;

		Geometry() {}

		template <typename ... Args>
		Geometry(const std::vector<uint32_t>& indices, const std::vector<Args>&... data)
			: indices(indices) {
			setVerticesData(data...);
		}

		template <typename T>
		void setVerticesData(const std::vector<T>& data) {
			auto size = vertices.size();
			vertices.resize(vertices.size() + data.size() * sizeof(T));
			memcpy(vertices.data() + size, data.data(), data.size() * sizeof(T));
		}

		template <typename T, typename ... Args>
		void setVerticesData(const std::vector<T>& data, const std::vector<Args>&... args) {
			setVerticesData(data);
			setVerticesData(args...);
		}

		void release() {
			lofx::release(&vbo);
			lofx::release(&ebo);
		}
	};

	struct Material {};
	struct Skeleton {};

	struct Model {
		glm::mat4 transform;
		std::vector<Geometry> geometries;
		std::vector<lofx::DrawProperties> draw_commands;

		Skeleton skeleton;
		Material* material;

		void render(lofx::Framebuffer* fbo, lofx::Pipeline* pipeline) {
			lofx::send(pipeline->stages[lofx::ShaderType::Vertex], lofx::Uniform("model", transform));

			if (draw_commands.size() != geometries.size())
				draw_commands.resize(geometries.size());

			for (std::size_t i = 0; i < geometries.size(); i++) {
				draw_commands[i].ebo = &geometries[i].ebo;
				draw_commands[i].vbo = &geometries[i].vbo;
				draw_commands[i].pack = &geometries[i].pack;
				draw_commands[i].pipeline = pipeline;
				draw_commands[i].fbo = fbo;

				lofx::draw(draw_commands[i]);
			}
		}

		void release() {
			for (auto& geom : geometries)
				geom.release();
		}
	};

}

namespace md5 {

	struct joint_t {
		char name[64];
		int parent;
		glm::vec3 pos;
		glm::quat orient;
	};

	struct vertex_t {
		glm::vec2 uv;
		int start; // start weight
		int count; // weight count
	};

	struct triangle_t {
		int index[3];
	};

	struct weight_t {
		int joint;
		float bias;
		glm::vec3 pos;
	};

	struct mesh_t {
		std::vector<vertex_t> vertices;
		std::vector<triangle_t> triangles;
		std::vector<weight_t> weights;
	};

	struct model_t {
		std::vector<joint_t> baseSkel;
		std::vector<mesh_t> meshes;
	};

	// Load an MD5 model from file.
	bool read(const std::string& filename, model_t* model) {
		if (!model) {
			lut::yell("(md5::read) A model pointer must be fed to the reader\n");
			return false;
		}

		fs::path path(filename);
		if (filename.length() == 0 || !fs::exists(path)) {
			lut::yell("(md5::read) File \'{}\' doesn\'t exist\n", filename);
			return false;
		}

		if (path.extension() != ".md5")
			lut::warn("(md5::read) File \'{}\' is not a md5 file\n", filename);

		std::ifstream ifs;
		ifs.open(path);

		int curr_mesh = 0;
		int num_joints = 0;
		int num_meshes = 1;
		model->meshes.resize(num_meshes);

		while (ifs.good()) {
			std::string line;
			std::getline(ifs, line);

			if (sscanf(line.c_str(), " numJoints %d", &num_joints) == 1)
				model->baseSkel.resize(num_joints);
			else if (sscanf(line.c_str(), " numMeshes %d", &num_meshes) == 1)
				model->meshes.resize(num_meshes);
			else if (strncmp(line.c_str(), "joints {", 8) == 0) {

				// Read each joint
				for (int i = 0; i < model->baseSkel.size(); ++i) {
					joint_t* joint = &model->baseSkel[i];

					std::string joint_line;
					std::getline(ifs, joint_line);
					if (8 == sscanf(joint_line.c_str(), "%s %d ( %f %f %f ) ( %f %f %f )",
						joint->name, &joint->parent,
						&joint->pos.x, &joint->pos.y, &joint->pos.z,
						&joint->orient.x, &joint->orient.y, &joint->orient.z))
					{
						float t = 1.0f - joint->orient.x * joint->orient.x - joint->orient.y - joint->orient.y - joint->orient.z * joint->orient.z;
						joint->orient.w = t < 0.0f ? 0.0f : -std::sqrt(t);
					}
				}
			}
			else if (strncmp(line.c_str(), "mesh {", 6) == 0) {
				mesh_t* mesh = &model->meshes[curr_mesh];
				int vert_index = 0;
				int tri_index = 0;
				int weight_index = 0;
				float fdata[4];
				int idata[3];

				int num_verts = 0;
				int num_tris = 0;
				int num_weights = 0;

				while ((line[0] != '}') && ifs.good()) {
					std::string mesh_line;
					std::getline(ifs, mesh_line);

					if (sscanf(mesh_line.c_str(), " numverts %d", &num_verts) == 1)
						mesh->vertices.resize(num_verts);
					else if (sscanf(mesh_line.c_str(), " numtris %d", &num_tris) == 1)
						mesh->triangles.resize(num_tris);
					else if (sscanf(mesh_line.c_str(), " numweights %d", &num_weights) == 1)
						mesh->triangles.resize(num_weights);
					else if (sscanf(mesh_line.c_str(), " vert %d ( %f %f ) %d %d", &vert_index,
						&fdata[0], &fdata[1], &idata[0], &idata[1]) == 5) {
						mesh->vertices[vert_index].uv.x = fdata[0];
						mesh->vertices[vert_index].uv.y = fdata[1];
						mesh->vertices[vert_index].start = idata[0];
						mesh->vertices[vert_index].count = idata[1];
					}
					else if (sscanf(mesh_line.c_str(), " tri %d %d %d %d", &tri_index,
						&idata[0], &idata[1], &idata[2]) == 4) {
						mesh->triangles[tri_index].index[0] = idata[0];
						mesh->triangles[tri_index].index[1] = idata[1];
						mesh->triangles[tri_index].index[2] = idata[2];
					}
					else if (sscanf(mesh_line.c_str(), " weight %d %d %f ( %f %f %f )",
						&weight_index, &idata[0], &fdata[3],
						&fdata[0], &fdata[1], &fdata[2]) == 6) {
						mesh->weights[weight_index].joint = idata[0];
						mesh->weights[weight_index].bias = fdata[3];
						mesh->weights[weight_index].pos.x = fdata[0];
						mesh->weights[weight_index].pos.y = fdata[1];
						mesh->weights[weight_index].pos.z = fdata[2];
					}
				}

				curr_mesh++;
			}
		}

		return true;
	}

	d3::Geometry to_geometry(const mesh_t& mesh) {
		d3::Geometry result;
		result.vertices.resize(mesh.weights.size() * 3 * sizeof(float) + mesh.vertices.size() * 2 * sizeof(float));
		result.indices.resize(mesh.triangles.size() * 3);

		{
			float* dataptr = reinterpret_cast<float*>(result.vertices.data());
			std::size_t offset = 0;
			for (std::size_t i = 0; i < mesh.weights.size(); i++) {
				dataptr[offset++] = mesh.weights[i].pos.x;
				dataptr[offset++] = mesh.weights[i].pos.y;
				dataptr[offset++] = mesh.weights[i].pos.z;
			}

			for (std::size_t i = 0; i < mesh.vertices.size(); i++) {
				dataptr[offset++] = mesh.vertices[i].uv.x;
				dataptr[offset++] = mesh.vertices[i].uv.y;
			}
		}

		{
			std::size_t offset = 0;
			for (std::size_t i = 0; i < mesh.triangles.size(); i++) {
				result.indices[offset++] = mesh.triangles[i].index[0];
				result.indices[offset++] = mesh.triangles[i].index[1];
				result.indices[offset++] = mesh.triangles[i].index[2];
			}
		}

		return result;
	}
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
layout (location = 1) in vec4 color;

out gl_PerVertex {
	vec4 gl_Position;
};

out VSOut {
	vec3 color;
} vsout;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main() {
	gl_Position = projection * view * model * vec4(coordinate, 1.0);
	vsout.color = color.rgb;
}

)";

const std::string fragment_shader_source = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in VSOut {
	vec3 color;
} fsin;

layout (location = 0) out vec4 colorout;

void main() {
	colorout = vec4(fsin.color, 1.0);
}

)";

//const d3::BufferGeometry quad = d3::BufferGeometry(
//	// uint32 Indices
//	std::vector<uint32_t> {
//		0, 1, 2,
//		0, 2, 3
//	},
//	// XYZ float positions
//	std::vector<float> {
//		-1.0f, -1.0f, 0.0f,
//		1.0f, -1.0f, 0.0f,
//		1.0f, 1.0f, 0.0f,
//		-1.0f, 1.0f, 0.0f
//	},
//	// ABGR uint32 color
//	std::vector<uint32_t> {
//		0x000000ff,
//		0x0000ff00,
//		0x00ff0000,
//		0x00777777
//	}
//);

#include <thread>

using namespace std::chrono_literals;
using clk = std::chrono::high_resolution_clock;
auto prtm = [](const std::string& msg, const clk::duration& duration, double mul = 1.0) { lut::trace("{0} : {1} sec\n", msg, (double) duration.count() / 1e9 * mul); };

int main() {
	lut::detail::loglevel = lut::LogLevel::Warn;
	lut::detail::logtoconsole = true;

	// Init LOFX
	lofx::init(glm::u32vec2(800, 800), "4.4");
	atexit(lofx::terminate);

	// Load model
	md5::model_t loaded_model;
	md5::read("model.md5", &loaded_model);
	d3::Model obj;
	for (const auto& mesh : loaded_model.meshes) {
		obj.geometries.push_back(md5::to_geometry(mesh));
		d3::Geometry& geometry = obj.geometries.back();
		
		// Preparing vertex buffer
		geometry.vbo = lofx::createBuffer(lofx::BufferType::Vertex, lofx::BufferUsage::StaticDraw, geometry.vertices.size());
		lofx::send(&geometry.vbo, (void*) geometry.vertices.data());
		geometry.pack = lofx::buildSequentialAttributePack({
			lofx::VertexAttribute(0, lofx::AttributeType::Float, 3, false, mesh.weights.size()),
			lofx::VertexAttribute(1, lofx::AttributeType::Float, 2, true, mesh.vertices.size())
		});
		
		// Preparing index buffer
		geometry.ebo = lofx::createBuffer(lofx::BufferType::Index, lofx::BufferUsage::StaticDraw, geometry.indices.size() * sizeof(uint32_t));
		lofx::send(&geometry.ebo, (void*) geometry.indices.data());
	}

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
	camera.projection = glm::perspective(80.0f, 800.0f / 800.0f, 0.1f, 100.0f);
	camera.view = glm::lookAt(glm::vec3(0.0f, 7.0f, 10.0f), glm::vec3(), glm::vec3(0.0f, 1.0f, 0.0f));
	lofx::send(&vertex_program, lofx::Uniform("view", camera.view));
	lofx::send(&vertex_program, lofx::Uniform("projection", camera.projection));
	
	// Loop while window is not closed
	lofx::loop([&] {
		
		obj.render(&fbo, &pipeline);
		std::this_thread::sleep_for(16ms);
	});

	obj.release();

	// Cleanup
	lofx::release(&pipeline);
	
	return 0;
}