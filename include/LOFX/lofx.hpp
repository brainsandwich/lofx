#pragma once

#include "LUT/lut.hpp"

#define GLFW_INCLUDE_NONE
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <unordered_map>

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
		std::unordered_map<std::string, uint32_t> uniform_locations;
	};

	struct Pipeline {
		uint32_t id;
		std::unordered_map<ShaderType, Program*> stages;
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

	enum class AttributeType {
		Byte, UnsignedByte,
		Short, UnsignedShort,
		Int, UnsignedInt,
		Float, Double
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
		uint32_t id;
	};

	struct BufferView {
		Buffer buffer;
		std::size_t offset;
		std::size_t length;
		std::size_t stride;
	};

	struct BufferAccessor {
		bool normalized;
		std::size_t offset;
		uint32_t components;
		uint32_t count;
		BufferView view;
		AttributeType component_type;
	};

	struct AttributePack {
		uint32_t bufferid = 0;
		std::unordered_map<uint32_t, BufferAccessor> attributes;
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
		const Framebuffer* fbo;
		const BufferAccessor* indices;
		const AttributePack* attributes;
		std::unordered_map<std::string, const Texture*> textures;
		const Pipeline* pipeline;
	};

	namespace gl {
		GLbitfield translateBitfield(ShaderType value);
		GLenum translate(ShaderType value);
		GLenum translate(BufferType value);
		GLenum translate(AttributeType value);
		GLbitfield translateBufferStorage(uint32_t value);
	}

	namespace detail {
		struct State {
			GLFWwindow* window;
			uint32_t vao;
		};
		extern State state;
	}

	Buffer createBuffer(BufferType buffer_type, std::size_t size, uint32_t buffer_storage = BufferStorage::Dynamic);
	void send(const Buffer* buffer, const void* data);
	void send(const Buffer* buffer, const void* data, std::size_t origin, std::size_t size);
	void release(Buffer* buffer);

	Texture createTexture(std::size_t width, std::size_t height);
	void send(const Texture* texture, const void* data);
	void release(Texture* texture);
	
	Program createProgram(ShaderType shadertype, const std::initializer_list<std::string>& sources);
	Pipeline createPipeline();
	void send(const Program* program, const Uniform& uniform);
	void use(const Pipeline* pipeline);
	void release(Pipeline* pipeline);
	void release(Program* program);

	uint8_t attribTypeSize(AttributeType type);
	AttributePack buildInterleavedAttributePack(const std::initializer_list<BufferAccessor>& attributes);
	AttributePack buildSequentialAttributePack(const std::initializer_list<BufferAccessor>& attributes);
	void bindAttributePack(AttributePack* pack);

	void sync();
	void init(const glm::u32vec2& size, const std::string& glversion = "");
	void terminate();
	void draw(const DrawProperties& properties);

	template <typename Func>
	void loop(const Func& func) {
		while (!glfwWindowShouldClose(detail::state.window)) {
			func();
			glfwSwapBuffers(detail::state.window);
			glfwPollEvents();
		}
	}

	Framebuffer current_fbo();
}