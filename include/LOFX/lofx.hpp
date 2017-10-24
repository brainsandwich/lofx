#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl3w.h>
#include <glm/glm.hpp>

#include <functional>
#include <memory>
#include <unordered_map>

namespace lofx {

	inline void onerror(GLenum error) {
		if (error == GL_NO_ERROR)
			return;

		switch (error) {
		case GL_INVALID_ENUM: detail::yell("OpenGL : Invalid enum\n"); return;
		case GL_INVALID_VALUE: detail::yell("OpenGL : Invalid value\n"); return;
		case GL_INVALID_OPERATION: detail::yell("OpenGL : Invalid operation\n"); return;
		case GL_INVALID_FRAMEBUFFER_OPERATION: detail::yell("OpenGL : Invalid framebuffer operation\n"); return;
		case GL_OUT_OF_MEMORY: detail::yell("OpenGL : Out of memory\n"); return;
		case GL_STACK_UNDERFLOW: detail::yell("OpenGL : Stack underflow\n"); return;
		case GL_STACK_OVERFLOW: detail::yell("OpenGL : Stack overflow\n"); return;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////
	////////// SHADERS AND PROGRAMS ///////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	struct ShaderType {
		using type = uint8_t;
		static const type Vertex = 0x1 << 0;
		static const type TessellationControl = 0x1 << 1;
		static const type TessellationEvaluation = 0x1 << 2;
		static const type Geometry = 0x1 << 3;
		static const type Fragment = 0x1 << 4;
		static const type Compute = 0x1 << 5;
		static const type Any = 
			Vertex | TessellationControl |
			TessellationEvaluation | Geometry |
			Fragment | Compute;
	};

	struct Program {
		uint32_t id;
		ShaderType::type typemask;
		std::unordered_map<std::string, uint32_t> uniform_locations;
	};

	struct Pipeline {
		uint32_t id;
		std::list<Program*> stages;
	};

	enum class UniformType {
		UnsignedInt, Int,
		Float, Float2, Float3, Float4,
		Mat2, Mat3, Mat4
	};

	struct Uniform {
		std::string name;
		UniformType type;
		union {
			uint32_t uint_value;
			int32_t int_value;
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
			else if (tid == typeid(uint32_t)) type = UniformType::UnsignedInt;
			else if (tid == typeid(int32_t)) type = UniformType::Int;
			else return;

			assign_value(value);
		}

	private:
		void assign_value(uint32_t value) { uint_value = value; }
		void assign_value(int32_t value) { int_value = value; }
		void assign_value(float value) { float_value = value; }
		void assign_value(glm::vec2 value) { float2_value = value; }
		void assign_value(glm::vec3 value) { float3_value = value; }
		void assign_value(glm::vec4 value) { float4_value = value; }
		void assign_value(glm::mat2 value) { mat2_value = value; }
		void assign_value(glm::mat3 value) { mat3_value = value; }
		void assign_value(glm::mat4 value) { mat4_value = value; }
	};

	///////////////////////////////////////////////////////////////////////////////////////
	////////// GENERIC BUFFERS ////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
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
		using type = uint8_t;
		static const type Dynamic = 0x1;
		static const type MapRead = 0x1 << 1;
		static const type MapWrite = 0x1 << 2;
		static const type MapPersistent = 0x1 << 3;
		static const type MapCoherent = 0x1 << 4;
		static const type ClientStorage = 0x1 << 5;
	};

	struct Buffer {
		std::vector<uint8_t> data;
		std::size_t size = 0;
		BufferType type;
		uint32_t id = 0;
	};

	struct BufferView {
		Buffer buffer;
		std::size_t offset = 0;
		std::size_t length = 0;
		std::size_t stride = 0;
	};

	struct BufferAccessor {
		bool normalized = false;
		std::size_t offset = 0;
		uint32_t components = 0;
		uint32_t count = 0;
		BufferView view;
		AttributeType component_type;
	};

	struct AttributePack {
		uint32_t bufferid = 0;
		std::unordered_map<uint32_t, BufferAccessor> attributes;
	};


	///////////////////////////////////////////////////////////////////////////////////////
	////////// TEXTURES AND FRAMEBUFFERS //////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	enum class TextureMagnificationFilter {
		Nearest,
		Linear
	};

	enum class TextureMinificationFilter {
		Nearest,
		Linear,
		NearestMipmapNearest,
		LinearMipmapNearest,
		NearestMipmapLinear,
		LinearMipmapLinear
	};

	enum class TextureWrappping {
		Clamp,
		Mirrored,
		Repeat
	};

	enum class TextureCompareMode {};
	enum class TextureCompareFunc {};

	struct TextureSamplerParameters {
		TextureMagnificationFilter mag;
		TextureMinificationFilter min;
		TextureWrappping s;
		TextureWrappping t;
		float max_anisotropy;
		float min_lod;
		float max_lod;
		float border_color[4];
	};

	struct TextureSampler {
		uint32_t id;
		TextureSamplerParameters parameters;
	};

	enum class TextureInternalFormat {
		// Base
		DepthComponent,
		DepthStencil,
		R,
		RG,
		RGB,
		RGBA,

		// Sized
		R8,
		R8_SNORM,
		R16,
		R16_SNORM,
		RG8,
		RG8_SNORM,
		RG16,
		RG16_SNORM,
		R3_G3_B2,
		RGB4,
		RGB5,
		RGB8,
		RGB8_SNORM,
		RGB10,
		RGB12,
		RGB16_SNORM,
		RGBA2,
		RGBA4,
		RGB5_A1,
		RGBA8,
		RGBA8_SNORM,
		RGB10_A2,
		RGB10_A2UI,
		RGBA12,
		RGBA16,
		SRGB8,
		SRGB8_ALPHA8,
		R16F,
		RG16F,
		RGB16F,
		RGBA16F,
		R32F,
		RG32F,
		RGB32F,
		RGBA32F,
		R11F_G11F_B10F,
		RGB9_E5,
		R8I,
		R8UI,
		R16I,
		R16UI,
		R32I,
		R32UI,
		RG8I,
		RG8UI,
		RG16I,
		RG16UI,
		RG32I,
		RG32UI,
		RGB8I,
		RGB8UI,
		RGB16I,
		RGB16UI,
		RGB32I,
		RGB32UI,
		RGBA8I,
		RGBA8UI,
		RGBA16I,
		RGBA16UI,
		RGBA32I,
		RGBA32UI,

		// Compressed
		COMPRESSED_RED,
		COMPRESSED_RG,
		COMPRESSED_RGB,
		COMPRESSED_RGBA,
		COMPRESSED_SRGB,
		COMPRESSED_SRGB_ALPHA,
		COMPRESSED_RED_RGTC1,
		COMPRESSED_SIGNED_RED_RGTC1,
		COMPRESSED_RG_RGTC2,
		COMPRESSED_SIGNED_RG_RGTC2,
		COMPRESSED_RGBA_BPTC_UNORM,
		COMPRESSED_SRGB_ALPHA_BPTC_UNORM,
		COMPRESSED_RGB_BPTC_SIGNED_FLOAT,
		COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
	};

	enum class TextureTarget {
		// 1D
		Texture1d,
		ProxyTexture1d,

		// 2D
		Texture2d,
		ProxyTexture2d,
		Texture1dArray,
		ProxyTexture1dArray,
		TextureRectangle,
		ProxyTextureRectangle,
		TextureCubeMapPositiveX,
		TextureCubeMapNegativeX,
		TextureCubeMapPositiveY,
		TextureCubeMapNegativeY,
		TextureCubeMapPositiveZ,
		TextureCubeMapNegativeZ,
		ProxyTextureCubeMap,

		// 3D
		Texture3d,
		ProxyTexture3d,
		Texture2dArray,
		ProxyTexture2dArray
	};

	struct Texture {
		uint32_t id;
		uint32_t width, height, depth;
		TextureSampler sampler;
		TextureTarget target;
		TextureInternalFormat internal_format;
	};

	enum class ImageDataType {
		UnsignedByte,
		Byte,
		UnsignedShort,
		Short,
		UnsignedInt,
		Int,
		Float,
		UnsignedByte_3_3_2,
		UnsignedByte_2_3_3_rev,
		UnsignedShort_5_6_5,
		UnsignedShort_5_6_5_rev,
		UnsignedShort_4_4_4_4,
		UnsignedShort_4_4_4_4_rev,
		UnsignedShort_5_5_5_1,
		UnsignedShort_1_5_5_5_rev,
		UnsignedInt_8_8_8_8,
		UnsignedInt_8_8_8_8_rev,
		UnsignedInt_10_10_10_2,
		UnsignedInt_2_10_10_10_rev
	};

	enum class ImageDataFormat {
		R,
		RG,
		RGB,
		BGR,
		RGBA,
		DepthComponent,
		StencilIndex
	};

	struct Renderbuffer {
		uint32_t id;
		uint32_t width;
		uint32_t height;
	};

	struct Framebuffer {
		uint32_t id = 0;
		std::vector<Texture> attachments;
		Renderbuffer renderbuffer;
	};

	///////////////////////////////////////////////////////////////////////////////////////
	////////// COMMAND PROPERTIES /////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	struct ClearProperties {
		glm::vec4 color = glm::vec4();
		double depth = 1.0;
		uint32_t stencil = 0;
		bool clear_color = true;
		bool clear_depth = true;
		bool clear_stencil = false;
	};

	enum class CullFace {
		Front, Back, Both
	};

	enum class FrontFace {
		ClockWise, CounterClockWise
	};

	struct GraphicsProperties {
		bool depth_test = true;
		bool stencil_test = false;
		bool faceculling_test = false;
		CullFace cullface = CullFace::Back;
		FrontFace frontface = FrontFace::CounterClockWise;
	};

	struct DrawProperties {
		const Framebuffer* fbo = nullptr;
		const BufferAccessor* indices;
		const AttributePack* attributes;
		std::unordered_map<std::string, const Texture*> textures;
		const Pipeline* pipeline;

		GraphicsProperties graphics_properties = GraphicsProperties();
	};

	///////////////////////////////////////////////////////////////////////////////////////
	////////// STATE //////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////
	enum class DebugLevel {
		Trace, Warn, Error
	};

	using debug_callback_t = std::function<void(DebugLevel, const std::string&)>;

	namespace detail {
		template <typename ... Args> void trace(const std::string& msg, Args ... args) {
			if (state.debug_callback) state.debug_callback(DebugLevel::Trace, string_format(msg, args ...));
		}

		template <typename ... Args> void warn(const std::string& msg, Args ... args) {
			if (state.debug_callback) state.debug_callback(DebugLevel::Warn, string_format(msg, args ...));
		}

		template <typename ... Args> void yell(const std::string& msg, Args ... args) {
			if (state.debug_callback) state.debug_callback(DebugLevel::Error, string_format(msg, args ...));
		}

		template<typename ... Args> std::string string_format(const std::string& format, Args ... args) {
			size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
			std::unique_ptr<char[]> buf(new char[size]);
			snprintf(buf.get(), size, format.c_str(), args ...);
			return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
		}

		struct State {
			GLFWwindow* window;
			uint32_t vao;
			debug_callback_t debug_callback;
		};
		extern State state;
	}

	///////////////////////////////////////////////////////////////////////////////////////
	////////// FUNCTIONS //////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////

	// Translations
	namespace gl {
		GLenum translate(BufferType value);
		GLenum translate(AttributeType value);
		GLenum translate(ImageDataFormat value);
		GLenum translate(ImageDataType value);
		GLenum translate(TextureInternalFormat value);
		GLenum translate(TextureTarget value);
		GLenum translateShaderType(ShaderType::type value);
		GLbitfield translateShaderTypeMask(ShaderType::type value);
		GLbitfield translateBufferStorage(BufferStorage::type value);
		std::string translateFramebufferStatus(GLenum value);
	}

	// Buffers
	Buffer createBuffer(BufferType buffer_type, std::size_t size, uint32_t buffer_storage = BufferStorage::Dynamic);
	void send(const Buffer* buffer, const void* data);
	void send(const Buffer* buffer, const void* data, std::size_t origin, std::size_t size);
	void release(Buffer* buffer);
	uint8_t attribTypeSize(AttributeType type);
	BufferAccessor createBufferAccessor(lofx::Buffer buffer, lofx::AttributeType type, std::size_t components, std::size_t length);
	AttributePack buildFlatAttributePack(const std::initializer_list<BufferAccessor>& attributes);
	AttributePack buildInterleavedAttributePack(const std::initializer_list<BufferAccessor>& attributes);
	AttributePack buildSequentialAttributePack(const std::initializer_list<BufferAccessor>& attributes);
	void bind(AttributePack* pack);

	// Textures
	Texture createTexture(std::size_t width, std::size_t height, std::size_t depth, const TextureSampler* sampler, TextureTarget target = TextureTarget::Texture2d, TextureInternalFormat format = TextureInternalFormat::RGBA8);
	void send(const Texture* texture, const void* data, const glm::u32vec3& offset, const glm::u32vec3& size, ImageDataFormat format, ImageDataType data_type);
	void send(const Texture* texture, const void* data, const glm::u32vec3& offset, const glm::u32vec3& size);
	void send(const Texture* texture, const void* data, ImageDataFormat format = ImageDataFormat::RGBA, ImageDataType data_type = ImageDataType::UnsignedByte);
	void* read(const Texture* texture, ImageDataFormat format, ImageDataType data_type);
	void release(Texture* texture);

	// Samplers
	TextureSampler createTextureSampler(const TextureSamplerParameters& parameters);
	void release(TextureSampler* sampler);

	// Programs
	Program createProgram(ShaderType::type typemask, const std::initializer_list<std::string>& sources);
	Pipeline createPipeline(const std::initializer_list<Program*>& programs = {});
	const Program* findStage(const Pipeline* program, ShaderType::type typemask);
	void send(const Program* program, const Uniform& uniform);
	void use(const Pipeline* pipeline);
	void release(Pipeline* pipeline);
	void release(Program* program);

	// Renderbuffer
	Renderbuffer createRenderBuffer(uint32_t width, uint32_t height);
	void release(Renderbuffer* renderbuffer);

	// Framebuffer
	Framebuffer createFramebuffer();
	void build(Framebuffer* framebuffer);
	void release(Framebuffer* framebuffer);
	uint8_t* read(Framebuffer* framebuffer, uint32_t attachment, std::size_t width, std::size_t height, ImageDataFormat format, ImageDataType data_type);
	Framebuffer defaultFramebuffer();

	// State Management
	void sync();
	void init(const glm::u32vec2& size, const std::string& glversion = "", bool invisible = false);
	void terminate();
	void swapbuffers();
	void clear(Framebuffer* framebuffer, const ClearProperties& properties = ClearProperties());
	void draw(const DrawProperties& properties);
	void setdbgCallback(const debug_callback_t& callback);

	template <typename Func>
	void loop(const Func& func) {
		while (!glfwWindowShouldClose(detail::state.window)) {
			func();
			swapbuffers();
			glfwPollEvents();
		}
	}
}