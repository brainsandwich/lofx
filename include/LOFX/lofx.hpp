#pragma once

#include "LUT/lut.hpp"

#include <GL/gl3w.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <map>

namespace lofx {

	enum class ShaderType {
		Vertex,
		TessellationControl,
		TessellationEvaluation,
		Geometry,
		Fragment,
		Compute
	};

	struct Program {
		uint32_t id;
		ShaderType type;
		std::map<std::string, uint32_t> uniform_locations;
	};

	struct Pipeline {
		uint32_t id;
		std::map<ShaderType, Program*> stages;
	};

	enum class UniformType {
		Float, Float2, Float3, Float4,
		Mat2, Mat3, Mat4
	};

	struct Uniform {
		std::string name;
		UniformType type;
		union {
			float float_value;
			glm::vec2 float2_value;
			glm::vec3 float3_value;
			glm::vec4 float4_value;
			glm::mat2 mat2_value;
			glm::mat3 mat3_value;
			glm::mat4 mat4_value;
		};

		Uniform() {}

		template <typename T> Uniform(const std::string& name, const T& value)
			: name(name) {
			const auto& tid = typeid(T);
			if (tid == typeid(float))type = UniformType::Float;
			else if (tid == typeid(glm::vec2)) type = UniformType::Float2;
			else if (tid == typeid(glm::vec3)) type = UniformType::Float3;
			else if (tid == typeid(glm::vec4)) type = UniformType::Float4;
			else if (tid == typeid(glm::mat2)) type = UniformType::Mat2;
			else if (tid == typeid(glm::mat3)) type = UniformType::Mat3;
			else if (tid == typeid(glm::mat4)) type = UniformType::Mat4;
			else return;

			assign_value(value);
		}

	private:
		void assign_value(float value) { float_value = value; }
		void assign_value(glm::vec2 value) { float2_value = value; }
		void assign_value(glm::vec3 value) { float3_value = value; }
		void assign_value(glm::vec4 value) { float4_value = value; }
		void assign_value(glm::mat2 value) { mat2_value = value; }
		void assign_value(glm::mat3 value) { mat3_value = value; }
		void assign_value(glm::mat4 value) { mat4_value = value; }
	};

	enum class BufferType {
		Vertex, Index
	};

	enum class BufferUsage {
		StreamDraw,
		StreamRead,
		StreamCopy,
		StaticDraw,
		StaticRead,
		StaticCopy,
		DynamicDraw,
		DynamicRead,
		DynamicCopy
	};

	struct BufferStorage {
		static const uint32_t Dynamic = 0x1;
		static const uint32_t MapRead = 0x1 << 1;
		static const uint32_t MapWrite = 0x1 << 2;
		static const uint32_t MapPersistent = 0x1 << 3;
		static const uint32_t MapCoherent = 0x1 << 4;
		static const uint32_t ClientStorage = 0x1 << 5;
	};

	struct Buffer {
		std::size_t size;
		BufferType type;
		BufferUsage usage;
		uint32_t id;
	};

	struct BufferBlock {
		Buffer* buffer;
		std::size_t begin;
		std::size_t end;
	};

	enum class AttributeType {
		Byte, UnsignedByte,
		Int, UnsignedInt,
		Float, Double
	};

	struct VertexAttribute {
		uint32_t id;
		uint32_t components;
		bool normalized;
		std::size_t stride;
		std::size_t offset;
		std::size_t length;
		AttributeType type;
		VertexAttribute(uint32_t id, AttributeType type, uint32_t components, bool normalized, std::size_t length = 0)
			: id(id)
			, type(type)
			, components(components)
			, normalized(normalized)
			, length(length) {}
	};

	struct AttributePack {
		std::vector<VertexAttribute> attributes;
	};

	struct Framebuffer {
		uint32_t id;
	};

	struct Texture {
		uint32_t width, height;
		uint32_t id;
	};

	struct TextureBlock {
		Texture* texture;
		glm::u64vec2 begin;
		glm::u64vec2 end;
	};

	struct DrawProperties {
		Framebuffer* fbo;
		Buffer* vbo;
		Buffer* ebo;
		AttributePack* pack;
		std::map<std::string, Texture*> textures;
		Pipeline* pipeline;
	};

	namespace gl {
		GLbitfield translateBitfield(ShaderType value) {
			switch (value) {
				case ShaderType::Vertex: return GL_VERTEX_SHADER_BIT;
				case ShaderType::TessellationControl: return GL_TESS_CONTROL_SHADER_BIT;
				case ShaderType::TessellationEvaluation: return GL_TESS_EVALUATION_SHADER_BIT;
				case ShaderType::Geometry: return GL_GEOMETRY_SHADER_BIT;
				case ShaderType::Fragment: return GL_FRAGMENT_SHADER_BIT;
				case ShaderType::Compute: return GL_COMPUTE_SHADER_BIT;
			}
			return GL_ALL_SHADER_BITS;
		}
		GLenum translate(ShaderType value) {
			switch (value) {
				case ShaderType::Vertex: return GL_VERTEX_SHADER;
				case ShaderType::TessellationControl: return GL_TESS_CONTROL_SHADER;
				case ShaderType::TessellationEvaluation: return GL_TESS_EVALUATION_SHADER;
				case ShaderType::Geometry: return GL_GEOMETRY_SHADER;
				case ShaderType::Fragment: return GL_FRAGMENT_SHADER;
				case ShaderType::Compute: return GL_COMPUTE_SHADER;
			}
			return GL_NONE;
		}
		GLenum translate(BufferType value) {
			switch (value) {
				case BufferType::Vertex: return GL_ARRAY_BUFFER;
				case BufferType::Index: return GL_ELEMENT_ARRAY_BUFFER;
			}
			return GL_NONE;
		}
		GLenum translate(BufferUsage value) {
			switch (value) {
				case BufferUsage::StreamDraw: return GL_STREAM_DRAW;
				case BufferUsage::StreamRead: return GL_STREAM_READ;
				case BufferUsage::StreamCopy: return GL_STREAM_COPY;
				case BufferUsage::StaticDraw: return GL_STATIC_DRAW;
				case BufferUsage::StaticRead: return GL_STATIC_READ;
				case BufferUsage::StaticCopy: return GL_STATIC_COPY;
				case BufferUsage::DynamicDraw: return GL_DYNAMIC_DRAW;
				case BufferUsage::DynamicRead: return GL_DYNAMIC_READ;
				case BufferUsage::DynamicCopy: return GL_DYNAMIC_COPY;
			}
			return GL_NONE;
		}
		GLenum translate(AttributeType value) {
			switch (value) {
				case AttributeType::Byte: return GL_BYTE;
				case AttributeType::UnsignedByte: return GL_UNSIGNED_BYTE;
				case AttributeType::Int: return GL_INT;
				case AttributeType::UnsignedInt: return GL_UNSIGNED_INT;
				case AttributeType::Float: return GL_FLOAT;
				case AttributeType::Double: return GL_DOUBLE;
			}
			return GL_NONE;
		}
		GLbitfield translateBufferStorage(uint32_t value) {
			GLbitfield result = 0;
			if (value & BufferStorage::Dynamic) result |= GL_DYNAMIC_STORAGE_BIT;
			if (value & BufferStorage::MapRead) result |= GL_MAP_READ_BIT;
			if (value & BufferStorage::MapWrite) result |= GL_MAP_WRITE_BIT;
			if (value & BufferStorage::MapPersistent) result |= GL_MAP_PERSISTENT_BIT;
			if (value & BufferStorage::MapCoherent) result |= GL_MAP_COHERENT_BIT;
			if (value & BufferStorage::ClientStorage) result |= GL_CLIENT_STORAGE_BIT;
			return result;
		}
	}

	namespace detail {
		struct State {
			GLFWwindow* window;
			uint32_t vao;
		} state;
	}

	Buffer createBuffer(BufferType buffer_type, BufferUsage buffer_usage, std::size_t size, uint32_t buffer_storage = BufferStorage::Dynamic) {
		Buffer result;
		result.type = buffer_type;
		result.size = size;
		result.usage = buffer_usage;
		glGenBuffers(1, &result.id);
		GLenum target = gl::translate(buffer_type);

		glBindBuffer(target, result.id);
		glBufferStorage(target, size, nullptr, gl::translateBufferStorage(buffer_storage));
		return result;
	}

	std::vector<BufferBlock> createBufferBlocks(Buffer* buffer, const std::initializer_list<std::size_t>& sizes) {
		std::vector<BufferBlock> result;
		std::size_t offset = 0;
		for (const auto& sz : sizes) {
			result.push_back(BufferBlock { buffer, offset, offset + sz });
			offset += sz;
		}
		return result;
	}

	void send(Buffer* buffer, void* data) {
		glBufferSubData(gl::translate(buffer->type), 0, buffer->size, data);
	}

	void send(Buffer* buffer, void* data, std::size_t origin, std::size_t size) {
		glBufferSubData(gl::translate(buffer->type), origin, size, data);
	}

	void send(BufferBlock* block, void* data) {
		send(block->buffer, data, block->begin, block->end - block->begin);
	}

	void release(Buffer* buffer) {
		if (glIsBuffer(buffer->id))
			glDeleteBuffers(1, &buffer->id);
	}

	Texture createTexture(std::size_t width, std::size_t height) { return Texture(); }
	void send(Texture* texture, void* data) {}
	void release(Texture* texture) {}
	
	Program createProgram(ShaderType shadertype, const std::initializer_list<std::string>& sources) {
		Program result;
		GLenum type = gl::translate(shadertype);
		std::string codeaccum;
		for (const auto& src : sources)
			codeaccum += src;

		const char* code = codeaccum.c_str();
		result.id = glCreateShaderProgramv(type, 1, &code);
		result.type = shadertype;

		// Check program compilation / link status
		int success = 0;
		glGetProgramiv(result.id, GL_LINK_STATUS, &success);
		if (!success) {
			int length = 0;
			glGetProgramiv(result.id, GL_INFO_LOG_LENGTH, &length);
			char* infolog = new char[length];
			glGetProgramInfoLog(result.id, length, nullptr, infolog);
			lut::yell("Program link failure :\n{}\n", infolog);
			delete[] infolog;
		}

		// Retrieve uniform locations
		int32_t uniform_count = 0;
		glGetProgramiv(result.id, GL_ACTIVE_UNIFORMS, &uniform_count);
		for (uint32_t i = 0; i < uniform_count; i++) {
			char* name = new char[512];
			int32_t length = 0;
			glGetActiveUniformName(result.id, i, 512, &length, name);
			result.uniform_locations[std::string(name)] = i;
			delete[] name;
		}

		return result;
	}

	Pipeline createPipeline() {
		Pipeline result;
		glGenProgramPipelines(1, &result.id);
		return result;
	}

	void send(Program* program, const Uniform& uniform) {
		if (!program->uniform_locations.count(uniform.name)) {
			lut::warn("Location of \"{}\" uniform not found in shader program\n", uniform.name.c_str());
			return;
		}

		uint32_t location = program->uniform_locations[uniform.name];
		switch (uniform.type) {
			case UniformType::Float: 
				glProgramUniform1fv(program->id, location, 1, &uniform.float_value);
				break;
			case UniformType::Float2:
				glProgramUniform2fv(program->id, location, 1, glm::value_ptr(uniform.float2_value));
				break;
			case UniformType::Float3:
				glProgramUniform3fv(program->id, location, 1, glm::value_ptr(uniform.float3_value));
				break;
			case UniformType::Float4:
				glProgramUniform4fv(program->id, location, 1, glm::value_ptr(uniform.float4_value));
				break;
			case UniformType::Mat2:
				glProgramUniformMatrix2fv(program->id, location, 1, false, glm::value_ptr(uniform.mat2_value));
				break;
			case UniformType::Mat3:
				glProgramUniformMatrix3fv(program->id, location, 1, false, glm::value_ptr(uniform.mat3_value));
				break;
			case UniformType::Mat4:
				glProgramUniformMatrix4fv(program->id, location, 1, false, glm::value_ptr(uniform.mat4_value));
				break;
		}
	}

	void use(Pipeline* pipeline) {
		glUseProgram(0);
		glUseProgramStages(pipeline->id, GL_ALL_SHADER_BITS, 0);
		for (const auto& stage_pair : pipeline->stages) {
			GLenum type = gl::translateBitfield(stage_pair.first);
			glUseProgramStages(pipeline->id, type, stage_pair.second->id);
		}
		glBindProgramPipeline(pipeline->id);
	}

	void release(Pipeline* pipeline) {
		if (glIsProgramPipeline(pipeline->id))
			glDeleteProgramPipelines(1, &pipeline->id);
	}

	void release(Program* program) {
		if (glIsProgram(program->id))
			glDeleteProgram(program->id);
	}

	uint8_t attribTypeSize(AttributeType type) {
		switch (type) {
			case lofx::AttributeType::Byte:
			case lofx::AttributeType::UnsignedByte:
				return 1;
			case lofx::AttributeType::Int:
			case lofx::AttributeType::UnsignedInt:
			case lofx::AttributeType::Float:
				return 4;
			case lofx::AttributeType::Double:
				return 8;
		}
		return 0;
	}

	AttributePack buildInterleavedAttributePack(const std::initializer_list<VertexAttribute>& attributes) {
		AttributePack pack;
		std::size_t total_stride = 0;
		for (const auto& attrib : attributes) {
			pack.attributes.push_back(attrib);
			pack.attributes.back().offset = total_stride;
			total_stride += pack.attributes.back().components * attribTypeSize(pack.attributes.back().type);
		}

		for (auto& attrib : pack.attributes)
			attrib.stride = total_stride;
		return pack;
	}

	AttributePack buildSequentialAttributePack(const std::initializer_list<VertexAttribute>& attributes) {
		AttributePack pack;
		std::size_t total_length = 0;
		for (const auto& attrib : attributes) {
			pack.attributes.push_back(attrib);
			pack.attributes.back().offset = total_length;
			total_length += pack.attributes.back().components * pack.attributes.back().length * attribTypeSize(pack.attributes.back().type);
			pack.attributes.back().stride = 0;
		}
		return pack;
	}

	void bindAttributePack(AttributePack* pack) {
		int maxVertexAttribs = 0;
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
		for (int i = 0; i < maxVertexAttribs; i++)
			glDisableVertexAttribArray(i);

		for (const auto& attrib : pack->attributes) {
			glEnableVertexAttribArray(attrib.id);
			switch (attrib.type)
			{
				case lofx::AttributeType::Byte:
				case lofx::AttributeType::UnsignedByte:
				case lofx::AttributeType::Float:
				case lofx::AttributeType::Double:
					glVertexAttribPointer(attrib.id, attrib.components, gl::translate(attrib.type), attrib.normalized, (GLsizei) attrib.stride, (const void*)attrib.offset);
					break;
				case lofx::AttributeType::Int:
				case lofx::AttributeType::UnsignedInt:
					glVertexAttribIPointer(attrib.id, attrib.components, gl::translate(attrib.type), (GLsizei) attrib.stride, (const void*)attrib.offset);
			}
		}
	}

	void sync() {
		glFinish();
	}

	void init(const glm::u32vec2& size, const std::string& glversion = "") {
		if (!glfwInit()) {
			lut::yell("GLFW init failed\n");
			Sleep(1000);
			exit(-1);
		}

		int major, minor;
		if (glversion != "") {
			sscanf_s(glversion.c_str(), "%d.%d", &major, &minor);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
		}

		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		detail::state.window = glfwCreateWindow(size.x, size.y, "hello", nullptr, nullptr);

		glfwMakeContextCurrent(detail::state.window);
		if (gl3wInit()) {
			lut::yell("Failed to load OpenGL\n");
			exit(-1);
		}

		if (glversion != "" && !gl3wIsSupported(major, minor)) {
			lut::yell("OpenGL {0}.{1} is not supported ! Aborting ...\n", major, minor);
			Sleep(1000);
			exit(-1);
		} else {
			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);
			lut::trace("LOFX initialized ; Using OpenGL {0}.{1}\n", major, minor);
		}

		glGenVertexArrays(1, &detail::state.vao);
		glBindVertexArray(detail::state.vao);
	}

	template <typename Func>
	void loop(const Func& func) {
		while (!glfwWindowShouldClose(detail::state.window)) {
			func();
			glfwSwapBuffers(detail::state.window);
			glfwPollEvents();
		}
	}

	void terminate() {
		if (glIsVertexArray(detail::state.vao))
			glDeleteVertexArrays(1, &detail::state.vao);

		glfwTerminate();
	}

	Framebuffer current_fbo() { return Framebuffer(); }

	void draw(const DrawProperties& properties) {
		glBindFramebuffer(GL_FRAMEBUFFER, properties.fbo->id);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		lofx::use(properties.pipeline);
		glBindBuffer(GL_ARRAY_BUFFER, properties.vbo->id);
		lofx::bindAttributePack(properties.pack);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, properties.ebo->id);
		glDrawElements(GL_TRIANGLES, (GLsizei) properties.ebo->size / (sizeof(uint32_t)), GL_UNSIGNED_INT, nullptr);
	}
}