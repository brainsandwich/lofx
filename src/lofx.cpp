#include "LOFX/lofx.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace lofx {

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
		GLenum translate(AttributeType value) {
			switch (value) {
			case AttributeType::Byte: return GL_BYTE;
			case AttributeType::UnsignedByte: return GL_UNSIGNED_BYTE;
			case AttributeType::Short: return GL_SHORT;
			case AttributeType::UnsignedShort: return GL_UNSIGNED_SHORT;
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
		State state;
	}

	Buffer createBuffer(BufferType buffer_type, std::size_t size, uint32_t buffer_storage) {
		Buffer result;
		result.type = buffer_type;
		result.size = size;
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
			result.push_back(BufferBlock{ buffer, offset, offset + sz });
			offset += sz;
		}
		return result;
	}

	void send(Buffer* buffer, const void* data) {
		glBufferSubData(gl::translate(buffer->type), 0, buffer->size, data);
	}

	void send(Buffer* buffer, const void* data, std::size_t origin, std::size_t size) {
		glBufferSubData(gl::translate(buffer->type), origin, size, data);
	}

	void send(BufferBlock* block, const void* data) {
		send(block->buffer, data, block->begin, block->end - block->begin);
	}

	void release(Buffer* buffer) {
		if (glIsBuffer(buffer->id))
			glDeleteBuffers(1, &buffer->id);
	}

	Texture createTexture(std::size_t width, std::size_t height) { return Texture(); }
	void send(Texture* texture, const void* data) {}
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
		case lofx::AttributeType::Short:
		case lofx::AttributeType::UnsignedShort:
			return 2;
		case lofx::AttributeType::Int:
		case lofx::AttributeType::UnsignedInt:
		case lofx::AttributeType::Float:
			return 4;
		case lofx::AttributeType::Double:
			return 8;
		}
		return 0;
	}

	AttributePack buildInterleavedAttributePack(const std::initializer_list<BufferAccessor>& attributes) {
		AttributePack pack;
		std::size_t total_stride = 0;
		uint32_t count = 0;
		for (const auto& attrib : attributes) {
			if (pack.bufferid == 0)
				pack.bufferid = attrib.view.buffer.id;
			pack.attributes[count] = attrib;
			pack.attributes[count].offset = total_stride;
			total_stride += pack.attributes[count].components * attribTypeSize(pack.attributes[count].component_type);
			count++;
		}

		for (auto& pair : pack.attributes)
			pair.second.view.stride = total_stride;

		return pack;
	}

	AttributePack buildSequentialAttributePack(const std::initializer_list<BufferAccessor>& attributes) {
		AttributePack pack;
		std::size_t total_length = 0;
		uint32_t count = 0;
		for (const auto& attrib : attributes) {
			if (pack.bufferid == 0)
				pack.bufferid = attrib.view.buffer.id;
			pack.attributes[count] = attrib;
			pack.attributes[count].offset = total_length;
			total_length += pack.attributes[count].components * pack.attributes[count].count * attribTypeSize(pack.attributes[count].component_type);
			pack.attributes[count].view.stride = 0;
		}
		return pack;
	}

	void bindAttributePack(AttributePack* pack) {
		int maxVertexAttribs = 0;
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

		for (const auto& attrib : pack->attributes) {
			glBindBuffer(GL_ARRAY_BUFFER, attrib.second.view.buffer.id);
			glEnableVertexAttribArray(attrib.first);
			const BufferAccessor& accessor = attrib.second;
			switch (attrib.second.component_type)
			{
			case lofx::AttributeType::Byte:
			case lofx::AttributeType::UnsignedByte:
			case lofx::AttributeType::Float:
			case lofx::AttributeType::Double:
				glVertexAttribPointer(attrib.first,
					accessor.components, gl::translate(accessor.component_type), accessor.normalized,
					(GLsizei) accessor.view.stride, (const void*) (accessor.offset));
				break;
			case lofx::AttributeType::Int:
			case lofx::AttributeType::UnsignedInt:
				glVertexAttribIPointer(attrib.first,
					accessor.components, gl::translate(accessor.component_type),
					(GLsizei) accessor.view.stride, (const void*) (accessor.offset));
			}
		}
	}

	void sync() {
		glFinish();
	}

	void init(const glm::u32vec2& size, const std::string& glversion) {
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
		}
		else {
			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);
			lut::trace("LOFX initialized ; Using OpenGL {0}.{1}\n", major, minor);
		}

		glGenVertexArrays(1, &detail::state.vao);
		glBindVertexArray(detail::state.vao);
	}

	void terminate() {
		if (glIsVertexArray(detail::state.vao))
			glDeleteVertexArrays(1, &detail::state.vao);

		glfwTerminate();
	}

	Framebuffer current_fbo() {
		return Framebuffer();
	}

	void draw(const DrawProperties& properties) {
		glBindFramebuffer(GL_FRAMEBUFFER, properties.fbo->id);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		lofx::use(properties.pipeline);
		lofx::bindAttributePack(properties.attributes);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, properties.indices->view.buffer.id);
		glDrawElements(GL_TRIANGLES, (GLsizei) properties.indices->count, gl::translate(properties.indices->component_type), (const void*) properties.indices->view.stride);
	}

}