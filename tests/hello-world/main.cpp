#include "LUT/lut.hpp"
#include "LOFX/lofx.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
			addVerticesData(data...);
		}

		void addVerticesData(const void* data, std::size_t size) {
			auto length = vertices.size();
			vertices.resize(vertices.size() + size);
			memcpy(vertices.data() + length, (void*) data, size);
		}

		template <typename T>
		void addVerticesData(const std::vector<T>& data) {
			auto size = vertices.size();
			vertices.resize(vertices.size() + data.size() * sizeof(T));
			memcpy(vertices.data() + size, data.data(), data.size() * sizeof(T));
		}

		template <typename T, typename ... Args>
		void addVerticesData(const std::vector<T>& data, const std::vector<Args>&... args) {
			addVerticesData(data);
			addVerticesData(args...);
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
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 uv;

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
	colorout = vec4(vec3(fsin.uv.x * (cos(1.5 * time + 3.14 / 2.0) / 2.0 + 1.0), fsin.uv.y * (sin(time) / 2.0 + 1.0), fsin.uv.z), 1.0);
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
	Assimp::Importer model_loader;
	const aiScene* scene = model_loader.ReadFile("C:\\Users\\brainsandwich\\Documents\\dev\\polypack\\lofx\\tests\\hello-world\\soldier1.fbx",
		aiProcess_Triangulate |
		aiProcess_FindInstances |
		aiProcess_OptimizeMeshes
	);

	if (!scene) {
		lut::yell("Couldn\'t load file {0} : {1}", "C:\\Users\\brainsandwich\\Documents\\dev\\polypack\\lofx\\tests\\hello-world\\soldier1.fbx", model_loader.GetErrorString());
		return -1;
	}

	d3::Model obj;
	obj.geometries.resize(scene->mNumMeshes);
	for (std::size_t i = 0; i < scene->mNumMeshes; i++) {
		const auto& mesh = scene->mMeshes[i];
		d3::Geometry& geometry = obj.geometries[i];

		geometry.addVerticesData((const void*) mesh->mVertices, mesh->mNumVertices * 3 * sizeof(float));
		geometry.addVerticesData((const void*) mesh->mNormals, mesh->mNumVertices * 3 * sizeof(float));
		//geometry.addVerticesData((const void*) mesh->mTextureCoords[0], mesh->mNumVertices * 3 * sizeof(float));
		
		// Preparing vertex buffer
		geometry.vbo = lofx::createBuffer(lofx::BufferType::Vertex, lofx::BufferUsage::StaticDraw, geometry.vertices.size());
		lofx::send(&geometry.vbo, (void*) geometry.vertices.data());
		geometry.pack = lofx::buildSequentialAttributePack({
			lofx::VertexAttribute(0, lofx::AttributeType::Float, 3, false, mesh->mNumVertices),
			lofx::VertexAttribute(1, lofx::AttributeType::Float, 3, false, mesh->mNumVertices)
			//lofx::VertexAttribute(2, lofx::AttributeType::Float, 3, true, mesh->mNumVertices)
		});
		
		// Preparing index buffer
		geometry.indices.resize(mesh->mNumFaces * 3);
		std::size_t offset = 0;
		for (std::size_t k = 0; k < mesh->mNumFaces; k++) {
			geometry.indices[offset++] = mesh->mFaces[k].mIndices[0];
			geometry.indices[offset++] = mesh->mFaces[k].mIndices[1];
			geometry.indices[offset++] = mesh->mFaces[k].mIndices[2];
		}

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
	camera.projection = glm::perspective(60.0f * glm::pi<float>() / 180.0f, 2000.0f / 1200.0f, 0.1f, 100.0f);
	camera.view = glm::lookAt(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	lofx::send(&vertex_program, lofx::Uniform("view", camera.view));
	lofx::send(&vertex_program, lofx::Uniform("projection", camera.projection));
	
	// Loop while window is not closed
	float time = 0.0f;
	lofx::loop([&] {
		lofx::send(&fragment_program, lofx::Uniform("time", time += 0.016f));
		obj.transform = glm::rotate(obj.transform, 0.01f, glm::vec3(0.0f, 0.0f, 1.0f));
		obj.render(&fbo, &pipeline);
		std::this_thread::sleep_for(16ms);
	});

	obj.release();

	// Cleanup
	lofx::release(&pipeline);
	
	return 0;
}