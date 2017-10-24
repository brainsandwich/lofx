#include "lofx/lofx.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace lofx {

	///////////////////////////////////////////////////////////////////////////////////////
	////////// SHADERS AND PROGRAMS ///////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	Program createProgram(ShaderType::type typemask, const std::initializer_list<std::string>& sources) {
		Program result;
		GLenum type = gl::translateShaderType(typemask);
		std::string codeaccum;
		for (const auto& src : sources)
			codeaccum += src;

		const char* code = codeaccum.c_str();
		result.id = glCreateShaderProgramv(type, 1, &code);
		result.typemask = typemask;

		// Check program compilation / link status
		int success = 0;
		glGetProgramiv(result.id, GL_LINK_STATUS, &success);
		if (!success) {
			int length = 0;
			glGetProgramiv(result.id, GL_INFO_LOG_LENGTH, &length);
			char* infolog = new char[length];
			glGetProgramInfoLog(result.id, length, nullptr, infolog);
			detail::yell("Program link failure :\n%s", infolog);
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

	Pipeline createPipeline(const std::initializer_list<Program*>& programs) {
		Pipeline result;
		glGenProgramPipelines(1, &result.id);
		if (programs.size() != 0)
			result.stages = programs;
		return result;
	}

	const Program* findStage(const Pipeline* pipeline, ShaderType::type typemask) {
		for (const auto& stage : pipeline->stages) {
			if (stage->typemask & typemask)
				return stage;
		}
		return nullptr;
	}

	void send(const Program* program, const Uniform& uniform) {
		if (!program->uniform_locations.count(uniform.name)) {
			detail::warn("Location of \"%s\" uniform not found in shader program", uniform.name.c_str());
			return;
		}

		uint32_t location = program->uniform_locations.at(uniform.name);
		switch (uniform.type) {
		case UniformType::UnsignedInt:
			glProgramUniform1uiv(program->id, location, 1, &uniform.uint_value);
			break;
		case UniformType::Int:
			glProgramUniform1iv(program->id, location, 1, &uniform.int_value);
			break;
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

	void use(const Pipeline* pipeline) {
		glUseProgram(0);
		glUseProgramStages(pipeline->id, GL_ALL_SHADER_BITS, 0);
		for (const auto& stage : pipeline->stages) 
			glUseProgramStages(pipeline->id, gl::translateShaderTypeMask(stage->typemask), stage->id);

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

	///////////////////////////////////////////////////////////////////////////////////////
	////////// GENERIC BUFFERS ////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
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

	void send(const Buffer* buffer, const void* data) {
		glBufferSubData(gl::translate(buffer->type), 0, buffer->size, data);
	}

	void send(const Buffer* buffer, const void* data, std::size_t origin, std::size_t size) {
		glBufferSubData(gl::translate(buffer->type), origin, size, data);
	}

	void release(Buffer* buffer) {
		if (glIsBuffer(buffer->id))
			glDeleteBuffers(1, &buffer->id);
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

	BufferAccessor createBufferAccessor(lofx::Buffer buffer, lofx::AttributeType type, std::size_t components, std::size_t length) {
		lofx::BufferView buffer_view;
		buffer_view.buffer = buffer;
		buffer_view.offset = 0;

		lofx::BufferAccessor accessor;
		accessor.view = buffer_view;
		accessor.component_type = type;
		accessor.components = components;
		accessor.count = length;

		return accessor;
	}

	AttributePack buildFlatAttributePack(const std::initializer_list<BufferAccessor>& attributes) {
		AttributePack pack;
		std::size_t total_stride = 0;
		uint32_t count = 0;
		for (const auto& attrib : attributes) {
			if (pack.bufferid == 0)
				pack.bufferid = attrib.view.buffer.id;
			pack.attributes[count] = attrib;
			pack.attributes[count].view.stride = 0;
			pack.attributes[count].offset = 0;
		}

		return pack;
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
			count++;
		}
		return pack;
	}

	void bind(const AttributePack* pack) {
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
					(GLsizei) accessor.view.stride, (const void*) (accessor.view.offset + accessor.offset));
				break;
			case lofx::AttributeType::Int:
			case lofx::AttributeType::UnsignedInt:
				glVertexAttribIPointer(attrib.first,
					accessor.components, gl::translate(accessor.component_type),
					(GLsizei) accessor.view.stride, (const void*) (accessor.view.offset + accessor.offset));
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////
	////////// TEXTURES AND FRAMEBUFFERS //////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	Texture createTexture(std::size_t width, std::size_t height, std::size_t depth, const TextureSampler* sampler, TextureTarget target, TextureInternalFormat format) {
		Texture tex;
		if (sampler)
			tex.sampler = *sampler;
		tex.width = width;
		tex.height = height;
		tex.depth = depth;
		tex.target = target;
		tex.internal_format = format;

		glGenTextures(1, &tex.id);
		glBindTexture(gl::translate(tex.target), tex.id);
		switch (target) {
		case TextureTarget::Texture1d:
		case TextureTarget::ProxyTexture1d:
			glTexStorage1D(gl::translate(tex.target), 1, gl::translate(tex.internal_format), width);
			break;

		case TextureTarget::Texture2d:
		case TextureTarget::ProxyTexture2d:
		case TextureTarget::Texture1dArray:
		case TextureTarget::ProxyTexture1dArray:
		case TextureTarget::TextureRectangle:
		case TextureTarget::ProxyTextureRectangle:
		case TextureTarget::TextureCubeMapPositiveX:
		case TextureTarget::TextureCubeMapNegativeX:
		case TextureTarget::TextureCubeMapPositiveY:
		case TextureTarget::TextureCubeMapNegativeY:
		case TextureTarget::TextureCubeMapPositiveZ:
		case TextureTarget::TextureCubeMapNegativeZ:
		case TextureTarget::ProxyTextureCubeMap:
			glTexStorage2D(gl::translate(tex.target), 1, gl::translate(tex.internal_format), width, height);
			break;

		case TextureTarget::Texture3d:
		case TextureTarget::ProxyTexture3d:
		case TextureTarget::Texture2dArray:
		case TextureTarget::ProxyTexture2dArray:
			glTexStorage3D(gl::translate(tex.target), 1, gl::translate(tex.internal_format), width, height, depth);
			break;
		}

		if (sampler) {
			TextureMinificationFilter min = tex.sampler.parameters.min;
			if (min == TextureMinificationFilter::LinearMipmapLinear
				|| min == TextureMinificationFilter::LinearMipmapNearest
				|| min == TextureMinificationFilter::NearestMipmapLinear
				|| min == TextureMinificationFilter::NearestMipmapNearest)
			{
				glGenerateMipmap(gl::translate(tex.target));
			}
		}

		return tex;
	}

	void send(const Texture* texture, const void* data, const glm::u32vec3& offset, const glm::u32vec3& size, ImageDataFormat format, ImageDataType data_type) {
		glBindTexture(gl::translate(texture->target), texture->id);

		switch (texture->target) {
		case TextureTarget::Texture1d:
		case TextureTarget::ProxyTexture1d:
			glTexSubImage1D(gl::translate(texture->target), 0,
				offset.x, size.x,
				gl::translate(format), gl::translate(data_type),
				data);
			break;

		case TextureTarget::Texture2d:
		case TextureTarget::ProxyTexture2d:
		case TextureTarget::Texture1dArray:
		case TextureTarget::ProxyTexture1dArray:
		case TextureTarget::TextureRectangle:
		case TextureTarget::ProxyTextureRectangle:
		case TextureTarget::TextureCubeMapPositiveX:
		case TextureTarget::TextureCubeMapNegativeX:
		case TextureTarget::TextureCubeMapPositiveY:
		case TextureTarget::TextureCubeMapNegativeY:
		case TextureTarget::TextureCubeMapPositiveZ:
		case TextureTarget::TextureCubeMapNegativeZ:
		case TextureTarget::ProxyTextureCubeMap:
			glTexSubImage2D(gl::translate(texture->target), 0,
				offset.x, offset.y,
				size.x, size.y,
				gl::translate(format), gl::translate(data_type),
				data);
			break;

		case TextureTarget::Texture3d:
		case TextureTarget::ProxyTexture3d:
		case TextureTarget::Texture2dArray:
		case TextureTarget::ProxyTexture2dArray:
			glTexSubImage3D(gl::translate(texture->target), 0,
				offset.x, offset.y, offset.z,
				size.x, size.y, size.z,
				gl::translate(format), gl::translate(data_type),
				data);
			break;
		}
	}

	void send(const Texture* texture, const void* data,
		const glm::u32vec3& offset,
		const glm::u32vec3& size) {
		send(texture, data, size, offset, ImageDataFormat::RGBA, ImageDataType::UnsignedByte);
	}
	void send(const Texture* texture, const void* data,
		ImageDataFormat format,
		ImageDataType data_type) {
		send(texture, data, glm::u32vec3(), glm::u32vec3(texture->width, texture->height, texture->depth), format, data_type);
	}

	void* read(const Texture* texture, ImageDataFormat format, ImageDataType data_type) {
		float* pixels = nullptr;
		glBindTexture(gl::translate(texture->target), texture->id);
		glGetTexImage(gl::translate(texture->target), 0, gl::translate(format), gl::translate(data_type), pixels);
		return pixels;
	}

	void release(Texture* texture) {
		if (glIsTexture(texture->id))
			glDeleteTextures(1, &texture->id);
	}

	TextureSampler createTextureSampler(const TextureSamplerParameters& parameters) {
		TextureSampler sampler;
		glGenSamplers(1, &sampler.id);

		switch (parameters.min) {
		case TextureMinificationFilter::Linear: glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR); break;
		case TextureMinificationFilter::Nearest: glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST); break;
		case TextureMinificationFilter::LinearMipmapLinear: glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); break;
		case TextureMinificationFilter::LinearMipmapNearest: glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST); break;
		case TextureMinificationFilter::NearestMipmapLinear: glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR); break;
		case TextureMinificationFilter::NearestMipmapNearest: glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); break;
		}

		switch (parameters.mag) {
		case TextureMagnificationFilter::Linear: glSamplerParameteri(sampler.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR); break;
		case TextureMagnificationFilter::Nearest: glSamplerParameteri(sampler.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST); break;
		}

		sampler.parameters = parameters;
		return sampler;
	}
	
	void release(TextureSampler* sampler) {
		if (glIsSampler(sampler->id))
			glDeleteSamplers(1, &sampler->id);
	}

	///////////////////////////////////////////////////////////////////////////////////////
	////////// FRAMEBUFFER ////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////

	Renderbuffer createRenderBuffer(uint32_t width, uint32_t height) {
		Renderbuffer result;
		glGenRenderbuffers(1, &result.id);
		glBindRenderbuffer(GL_RENDERBUFFER, result.id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		return result;
	}

	void release(Renderbuffer* renderbuffer) {
		if (glIsRenderbuffer(renderbuffer->id))
			glDeleteRenderbuffers(1, &renderbuffer->id);
	}

	Framebuffer createFramebuffer() {
		Framebuffer result;
		glGenFramebuffers(1, &result.id);
		return result;
	}

	void build(Framebuffer* framebuffer) {
		if (!glIsRenderbuffer(framebuffer->renderbuffer.id)) {
			detail::yell("framebuffer has a bad renderbuffer attachment\n");
			return;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->id);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer->renderbuffer.id);

		std::size_t current_attachment = 0;
		std::vector<GLenum> draw_attachments;
		for (std::size_t i = 0; i < framebuffer->attachments.size(); i++) {
			const auto& texture = framebuffer->attachments[i];
			switch (texture.target) {
			case TextureTarget::Texture1d:
			case TextureTarget::ProxyTexture1d: {
				GLenum attachment = GL_COLOR_ATTACHMENT0 + current_attachment++;
				draw_attachments.push_back(attachment);
				glFramebufferTexture1D(GL_FRAMEBUFFER, attachment, gl::translate(texture.target), texture.id, 0);
				break;
			}

			case TextureTarget::Texture2d:
			case TextureTarget::ProxyTexture2d:
			case TextureTarget::TextureRectangle:
			case TextureTarget::ProxyTextureRectangle:
			case TextureTarget::TextureCubeMapPositiveX:
			case TextureTarget::TextureCubeMapNegativeX:
			case TextureTarget::TextureCubeMapPositiveY:
			case TextureTarget::TextureCubeMapNegativeY:
			case TextureTarget::TextureCubeMapPositiveZ:
			case TextureTarget::TextureCubeMapNegativeZ:
			case TextureTarget::ProxyTextureCubeMap: {
				GLenum attachment = GL_COLOR_ATTACHMENT0 + current_attachment++;
				draw_attachments.push_back(attachment);
				glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, gl::translate(texture.target), texture.id, 0);
				break;
			}

			case TextureTarget::Texture3d:
			case TextureTarget::ProxyTexture3d: {
				GLenum attachment = GL_COLOR_ATTACHMENT0 + current_attachment++;
				draw_attachments.push_back(attachment);
				glFramebufferTexture3D(GL_FRAMEBUFFER, attachment, gl::translate(texture.target), texture.id, 0, 0);
				break;
			}
			
			case TextureTarget::Texture1dArray:
			case TextureTarget::ProxyTexture1dArray:
			case TextureTarget::Texture2dArray:
			case TextureTarget::ProxyTexture2dArray: {
				for (std::size_t i = 0; i < texture.depth; i++) {
					GLenum attachment = GL_COLOR_ATTACHMENT0 + current_attachment++;
					draw_attachments.push_back(attachment);
					glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture.id, 0, i);
				}
				break;
			}
			}
		}

		int32_t max_color_attachments = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
		if (current_attachment > max_color_attachments) {
			detail::warn("framebuffer has too many color attachments ({}, max is {})",
				current_attachment,
				max_color_attachments);
		}

		glDrawBuffers(draw_attachments.size(), draw_attachments.data());
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			detail::yell("framebuffer incomplete : %s", gl::translateFramebufferStatus(glCheckFramebufferStatus(GL_FRAMEBUFFER)).c_str());

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void release(Framebuffer* framebuffer) {
		if (glIsFramebuffer(framebuffer->id))
			glDeleteFramebuffers(1, &framebuffer->id);
	}

	uint8_t* read(Framebuffer* framebuffer, uint32_t attachment, std::size_t width, std::size_t height, ImageDataFormat format, ImageDataType data_type) {
		uint8_t* pixels = new uint8_t[width * height * sizeof(float)];
		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment);
		glReadPixels(0, 0, width, height, gl::translate(format), gl::translate(data_type), pixels);
		return pixels;
	}

	Framebuffer defaultFramebuffer() {
		return Framebuffer();
	}

	///////////////////////////////////////////////////////////////////////////////////////
	////////// STATE //////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	namespace detail {
		State state;
	}

	void sync() {
		glFinish();
	}

	void init(const glm::u32vec2& size, const std::string& glversion, bool invisible) {
		if (!glfwInit()) {
			detail::yell("GLFW init failed");
			exit(-1);
		}

		glfwWindowHint(GLFW_VISIBLE, !invisible);
		int major, minor;
		if (glversion != "") {
			sscanf_s(glversion.c_str(), "%d.%d", &major, &minor);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
		}

		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		detail::state.window = glfwCreateWindow(size.x, size.y, "", nullptr, nullptr);

		glfwMakeContextCurrent(detail::state.window);
		if (gl3wInit()) {
			detail::yell("Failed to load OpenGL");
			exit(-1);
		}

		int context_major = 0, context_minor = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &context_major);
		glGetIntegerv(GL_MINOR_VERSION, &context_minor);
		if (glversion != "" && context_major < major || (context_major == major && context_minor < minor)) {
			detail::yell("OpenGL %d.%d is not supported ! Aborting ...", major, minor);
			exit(-1);
		}
		else {
			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);
		}
		detail::trace("LOFX initialized ; Using OpenGL %d.%d", major, minor);

		glGenVertexArrays(1, &detail::state.vao);
		glBindVertexArray(detail::state.vao);
	}

	void terminate() {
		if (glIsVertexArray(detail::state.vao))
			glDeleteVertexArrays(1, &detail::state.vao);

		glfwTerminate();
	}

	void swapbuffers() {
		glfwSwapBuffers(detail::state.window);
	}

	void clear(Framebuffer* framebuffer, const ClearProperties& properties) {
		if (!framebuffer)
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		else
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->id);

		glClearColor(properties.color.r, properties.color.g, properties.color.b, properties.color.a);
		glClearDepth(properties.depth);
		glClearStencil(properties.stencil);
		uint32_t clearflags = 0;
		clearflags = (properties.clear_color ? GL_COLOR_BUFFER_BIT : 0)
			| (properties.clear_depth ? GL_DEPTH_BUFFER_BIT : 0)
			| (properties.clear_stencil ? GL_STENCIL_BUFFER_BIT : 0);
		glClear(clearflags);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void draw(const DrawProperties& properties) {
		if (!properties.fbo)
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		else
			glBindFramebuffer(GL_FRAMEBUFFER, properties.fbo->id);

		const GraphicsProperties& gfxprop = properties.graphics_properties;

		if (gfxprop.depth_test) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);

		if (gfxprop.faceculling_test) glEnable(GL_CULL_FACE);
		else glDisable(GL_CULL_FACE);

		if (gfxprop.stencil_test) glEnable(GL_STENCIL_TEST);
		else glDisable(GL_STENCIL_TEST);

		glFrontFace(gfxprop.frontface == FrontFace::ClockWise ? GL_CW : GL_CCW);
		glCullFace(gfxprop.cullface == CullFace::Front ? GL_FRONT : gfxprop.cullface == CullFace::Back ? GL_BACK : GL_FRONT_AND_BACK);

		lofx::use(properties.pipeline);
		lofx::bind(properties.attributes);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, properties.indices->view.buffer.id);

		int32_t count = 0;
		for (const auto& pair : properties.textures) {
			glActiveTexture(GL_TEXTURE0 + count);

			if (glIsSampler(pair.second->sampler.id))
				glBindSampler(count, pair.second->sampler.id);

			for (const auto& stage : properties.pipeline->stages) {
				if (stage->uniform_locations.count(pair.first))
					send(stage, Uniform(pair.first, count));
			}

			glBindTexture(gl::translate(pair.second->target), pair.second->id);

			count++;
		}

		glActiveTexture(GL_TEXTURE0);
		glDrawElements(GL_TRIANGLES, (GLsizei) properties.indices->count, gl::translate(properties.indices->component_type), (const void*) properties.indices->view.stride);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void setdbgCallback(const debug_callback_t& callback) {
		detail::state.debug_callback = callback;
	}

	///////////////////////////////////////////////////////////////////////////////////////
	////////// TRANSLATIONS ///////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	namespace gl {
		GLbitfield translateBufferStorage(BufferStorage::type value) {
			GLbitfield result = 0;
			if (value & BufferStorage::Dynamic) result |= GL_DYNAMIC_STORAGE_BIT;
			if (value & BufferStorage::MapRead) result |= GL_MAP_READ_BIT;
			if (value & BufferStorage::MapWrite) result |= GL_MAP_WRITE_BIT;
			if (value & BufferStorage::MapPersistent) result |= GL_MAP_PERSISTENT_BIT;
			if (value & BufferStorage::MapCoherent) result |= GL_MAP_COHERENT_BIT;
			if (value & BufferStorage::ClientStorage) result |= GL_CLIENT_STORAGE_BIT;
			return result;
		}

		GLbitfield translateShaderTypeMask(ShaderType::type value) {
			GLbitfield result = 0;
			if (value & ShaderType::Vertex) result |= GL_VERTEX_SHADER_BIT;
			if (value & ShaderType::TessellationControl) result |= GL_TESS_CONTROL_SHADER_BIT;
			if (value & ShaderType::TessellationEvaluation) result |= GL_TESS_EVALUATION_SHADER_BIT;
			if (value & ShaderType::Geometry) result |= GL_GEOMETRY_SHADER_BIT;
			if (value & ShaderType::Fragment) result |= GL_FRAGMENT_SHADER_BIT;
			if (value & ShaderType::Compute) result |= GL_COMPUTE_SHADER_BIT;
			return result;
		}

		GLenum translateShaderType(ShaderType::type value) {
			if (value & ShaderType::Vertex) return GL_VERTEX_SHADER;
			if (value & ShaderType::TessellationControl) return GL_TESS_CONTROL_SHADER;
			if (value & ShaderType::TessellationEvaluation) return GL_TESS_EVALUATION_SHADER;
			if (value & ShaderType::Geometry) return GL_GEOMETRY_SHADER;
			if (value & ShaderType::Fragment) return GL_FRAGMENT_SHADER;
			if (value & ShaderType::Compute) return GL_COMPUTE_SHADER;
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

		GLenum translate(ImageDataFormat value) {
			switch (value) {
			case ImageDataFormat::R: return GL_RED;
			case ImageDataFormat::RG: return GL_RG;
			case ImageDataFormat::RGB: return GL_RGB;
			case ImageDataFormat::BGR: return GL_BGR;
			case ImageDataFormat::RGBA: return GL_RGBA;
			case ImageDataFormat::DepthComponent: return GL_DEPTH_COMPONENT;
			case ImageDataFormat::StencilIndex: return GL_STENCIL_INDEX;
			}
			return GL_NONE;
		}

		GLenum translate(ImageDataType value) {
			switch (value) {
			case ImageDataType::UnsignedByte: return GL_UNSIGNED_BYTE;
			case ImageDataType::Byte: return GL_BYTE;
			case ImageDataType::UnsignedShort: return GL_UNSIGNED_SHORT;
			case ImageDataType::Short: return GL_SHORT;
			case ImageDataType::UnsignedInt: return GL_UNSIGNED_INT;
			case ImageDataType::Int: return GL_INT;
			case ImageDataType::Float: return GL_FLOAT;
			case ImageDataType::UnsignedByte_3_3_2: return GL_UNSIGNED_BYTE_3_3_2;
			case ImageDataType::UnsignedByte_2_3_3_rev: return GL_UNSIGNED_BYTE_2_3_3_REV;
			case ImageDataType::UnsignedShort_5_6_5: return GL_UNSIGNED_SHORT_5_6_5;
			case ImageDataType::UnsignedShort_5_6_5_rev: return GL_UNSIGNED_SHORT_5_6_5_REV;
			case ImageDataType::UnsignedShort_4_4_4_4: return GL_UNSIGNED_SHORT_4_4_4_4;
			case ImageDataType::UnsignedShort_4_4_4_4_rev: return GL_UNSIGNED_SHORT_4_4_4_4_REV;
			case ImageDataType::UnsignedShort_5_5_5_1: return GL_UNSIGNED_SHORT_5_5_5_1;
			case ImageDataType::UnsignedShort_1_5_5_5_rev: return GL_UNSIGNED_SHORT_1_5_5_5_REV;
			case ImageDataType::UnsignedInt_8_8_8_8: return GL_UNSIGNED_INT_8_8_8_8;
			case ImageDataType::UnsignedInt_8_8_8_8_rev: return GL_UNSIGNED_INT_8_8_8_8_REV;
			case ImageDataType::UnsignedInt_10_10_10_2: return GL_UNSIGNED_INT_10_10_10_2;
			case ImageDataType::UnsignedInt_2_10_10_10_rev: return GL_UNSIGNED_INT_2_10_10_10_REV;
			}
			return GL_NONE;
		}

		GLenum translate(TextureInternalFormat value) {
			switch (value) {
			// Base
			case TextureInternalFormat::DepthComponent: return GL_DEPTH_COMPONENT;
			case TextureInternalFormat::DepthStencil: return GL_DEPTH_STENCIL;
			case TextureInternalFormat::R: return GL_RED;
			case TextureInternalFormat::RG: return GL_RG;
			case TextureInternalFormat::RGB: return GL_RGB;
			case TextureInternalFormat::RGBA: return GL_RGBA;

			// Sized
			case TextureInternalFormat::R8: return GL_R8;
			case TextureInternalFormat::R8_SNORM: return GL_R8_SNORM;
			case TextureInternalFormat::R16: return GL_R16;
			case TextureInternalFormat::R16_SNORM: return GL_R16_SNORM;
			case TextureInternalFormat::RG8: return GL_RG8;
			case TextureInternalFormat::RG8_SNORM: return GL_RG8_SNORM;
			case TextureInternalFormat::RG16: return GL_RG16;
			case TextureInternalFormat::RG16_SNORM: return GL_RG16_SNORM;
			case TextureInternalFormat::R3_G3_B2: return GL_R3_G3_B2;
			case TextureInternalFormat::RGB4: return GL_RGB4;
			case TextureInternalFormat::RGB5: return GL_RGB5;
			case TextureInternalFormat::RGB8: return GL_RGB8;
			case TextureInternalFormat::RGB8_SNORM: return GL_RGB8_SNORM;
			case TextureInternalFormat::RGB10: return GL_RGB10;
			case TextureInternalFormat::RGB12: return GL_RGB12;
			case TextureInternalFormat::RGB16_SNORM: return GL_RGB16_SNORM;
			case TextureInternalFormat::RGBA2: return GL_RGBA2;
			case TextureInternalFormat::RGBA4: return GL_RGBA4;
			case TextureInternalFormat::RGB5_A1: return GL_RGB5_A1;
			case TextureInternalFormat::RGBA8: return GL_RGBA8;
			case TextureInternalFormat::RGBA8_SNORM: return GL_RGBA8_SNORM;
			case TextureInternalFormat::RGB10_A2: return GL_RGB10_A2;
			case TextureInternalFormat::RGB10_A2UI: return GL_RGB10_A2UI;
			case TextureInternalFormat::RGBA12: return GL_RGBA12;
			case TextureInternalFormat::RGBA16: return GL_RGBA16;
			case TextureInternalFormat::SRGB8: return GL_SRGB8;
			case TextureInternalFormat::SRGB8_ALPHA8: return GL_SRGB8_ALPHA8;
			case TextureInternalFormat::R16F: return GL_R16F;
			case TextureInternalFormat::RG16F: return GL_RG16F;
			case TextureInternalFormat::RGB16F: return GL_RGB16F;
			case TextureInternalFormat::RGBA16F: return GL_RGBA16F;
			case TextureInternalFormat::R32F: return GL_R32F;
			case TextureInternalFormat::RG32F: return GL_RG32F;
			case TextureInternalFormat::RGB32F: return GL_RGB32F;
			case TextureInternalFormat::RGBA32F: return GL_RGBA32F;
			case TextureInternalFormat::R11F_G11F_B10F: return GL_R11F_G11F_B10F;
			case TextureInternalFormat::RGB9_E5: return GL_RGB9_E5;
			case TextureInternalFormat::R8I: return GL_R8I;
			case TextureInternalFormat::R8UI: return GL_R8UI;
			case TextureInternalFormat::R16I: return GL_R16I;
			case TextureInternalFormat::R16UI: return GL_R16UI;
			case TextureInternalFormat::R32I: return GL_R32I;
			case TextureInternalFormat::R32UI: return GL_R32UI;
			case TextureInternalFormat::RG8I: return GL_RG8I;
			case TextureInternalFormat::RG8UI: return GL_RG8UI;
			case TextureInternalFormat::RG16I: return GL_RG16I;
			case TextureInternalFormat::RG16UI: return GL_RG16UI;
			case TextureInternalFormat::RG32I: return GL_RG32I;
			case TextureInternalFormat::RG32UI: return GL_RG32UI;
			case TextureInternalFormat::RGB8I: return GL_RGB8I;
			case TextureInternalFormat::RGB8UI: return GL_RGB8UI;
			case TextureInternalFormat::RGB16I: return GL_RGB16I;
			case TextureInternalFormat::RGB16UI: return GL_RGB16UI;
			case TextureInternalFormat::RGB32I: return GL_RGB32I;
			case TextureInternalFormat::RGB32UI: return GL_RGB32UI;
			case TextureInternalFormat::RGBA8I: return GL_RGBA8I;
			case TextureInternalFormat::RGBA8UI: return GL_RGBA8UI;
			case TextureInternalFormat::RGBA16I: return GL_RGBA16I;
			case TextureInternalFormat::RGBA16UI: return GL_RGBA16UI;
			case TextureInternalFormat::RGBA32I: return GL_RGBA32I;
			case TextureInternalFormat::RGBA32UI: return GL_RGBA32UI;

			// Compressed
			case TextureInternalFormat::COMPRESSED_RED: return GL_COMPRESSED_RED;
			case TextureInternalFormat::COMPRESSED_RG: return GL_COMPRESSED_RG;
			case TextureInternalFormat::COMPRESSED_RGB: return GL_COMPRESSED_RGB;
			case TextureInternalFormat::COMPRESSED_RGBA: return GL_COMPRESSED_RGBA;
			case TextureInternalFormat::COMPRESSED_SRGB: return GL_COMPRESSED_SRGB;
			case TextureInternalFormat::COMPRESSED_SRGB_ALPHA: return GL_COMPRESSED_SRGB_ALPHA;
			case TextureInternalFormat::COMPRESSED_RED_RGTC1: return GL_COMPRESSED_RED_RGTC1;
			case TextureInternalFormat::COMPRESSED_SIGNED_RED_RGTC1: return GL_COMPRESSED_SIGNED_RED_RGTC1;
			case TextureInternalFormat::COMPRESSED_RG_RGTC2: return GL_COMPRESSED_RG_RGTC2;
			case TextureInternalFormat::COMPRESSED_SIGNED_RG_RGTC2: return GL_COMPRESSED_SIGNED_RG_RGTC2;
			case TextureInternalFormat::COMPRESSED_RGBA_BPTC_UNORM: return GL_COMPRESSED_RGBA_BPTC_UNORM;
			case TextureInternalFormat::COMPRESSED_SRGB_ALPHA_BPTC_UNORM: return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
			case TextureInternalFormat::COMPRESSED_RGB_BPTC_SIGNED_FLOAT: return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
			case TextureInternalFormat::COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
			}
			return GL_NONE;
		}

		GLenum translate(TextureTarget value) {
			switch (value) {
			// 1D
			case TextureTarget::Texture1d: return GL_TEXTURE_1D;
			case TextureTarget::ProxyTexture1d: return GL_PROXY_TEXTURE_1D;

			// 2D
			case TextureTarget::Texture2d: return GL_TEXTURE_2D;
			case TextureTarget::ProxyTexture2d: return GL_PROXY_TEXTURE_2D;
			case TextureTarget::Texture1dArray: return GL_TEXTURE_1D_ARRAY;
			case TextureTarget::ProxyTexture1dArray: return GL_PROXY_TEXTURE_1D_ARRAY;
			case TextureTarget::TextureRectangle: return GL_TEXTURE_RECTANGLE;
			case TextureTarget::ProxyTextureRectangle: return GL_PROXY_TEXTURE_RECTANGLE;
			case TextureTarget::TextureCubeMapPositiveX: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
			case TextureTarget::TextureCubeMapNegativeX: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
			case TextureTarget::TextureCubeMapPositiveY: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
			case TextureTarget::TextureCubeMapNegativeY: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			case TextureTarget::TextureCubeMapPositiveZ: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
			case TextureTarget::TextureCubeMapNegativeZ: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			case TextureTarget::ProxyTextureCubeMap: return GL_PROXY_TEXTURE_CUBE_MAP;

			// 3D
			case TextureTarget::Texture3d: return GL_TEXTURE_3D;
			case TextureTarget::ProxyTexture3d: return GL_PROXY_TEXTURE_3D;
			case TextureTarget::Texture2dArray: return GL_TEXTURE_2D_ARRAY;
			case TextureTarget::ProxyTexture2dArray: return GL_PROXY_TEXTURE_2D_ARRAY;
			}
			return GL_NONE;
		}
	
		std::string translateFramebufferStatus(GLenum value) {
			switch (value) {
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "incomplete attachment";
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "incomplete missing attachment";
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: return "incomplete draw buffer";
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: return "incomplete read buffer";
			case GL_FRAMEBUFFER_UNSUPPORTED: return "unsupported";
			case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return "incomplete multisample";
			case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: return "incomplete layer targets";
			}
			return "unknown";
		}

	}

}