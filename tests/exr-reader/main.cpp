#include "lofx/lofx.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <fstream>
#include <filesystem>

namespace postfx_shader_source {

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

uniform mat4 model;

void main() {	
	gl_Position = model * vec4(coordinate, 1.0);
	vsout.coordinate = coordinate.xyz;
}

)";

	const std::string fragment = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in VSOut {
	vec3 coordinate;
} fsin;

layout (location = 0) out float xout;
layout (location = 1) out float yout;
layout (location = 2) out float zout;

uniform sampler2DArray input_texture;

uniform float strength;
uniform float exponent;
uniform float k;
uniform float k_pow_e;
uniform float rcpYWhite2;
uniform float srcGain;

void main() {
	vec2 coords = vec2(fsin.coordinate.x, -fsin.coordinate.y);

	float X = texture(input_texture, vec3(coords, 0)).x;
	float Y = texture(input_texture, vec3(coords, 1)).x;
	float Z = texture(input_texture, vec3(coords, 2)).x;

	float luminance = srcGain * Y;

	float f_nom = exp(exponent * log(luminance * rcpYWhite2));
	float f_denom = 1.0f + exp(exponent * log(k * luminance));
	float f = (k_pow_e + f_nom) / f_denom;
	//float reinhard_factor = exp(strength * log(f));
	float reinhard_factor = 1.0f;
	
	vec3 xyz = vec3(X, Y, Z);
	xout = xyz.x;
	yout = xyz.y;
	zout = xyz.z;
}

)";

	const std::string passthrough_fragment = R"(
#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in VSOut {
	vec3 coordinate;
} fsin;

// layout (location = 0) out vec4 colorout;

layout (location = 0) out float xout;
layout (location = 1) out float yout;
layout (location = 2) out float zout;

uniform sampler2DArray input_texture;

const mat3 XYZ_to_RGB = mat3(
	3.2404542, -1.5371385, -0.4985314,
	-0.9692660,  1.8760108,  0.0415560,
	 0.0556434, -0.2040259,  1.0572252
);

void main() {
	vec2 coords = vec2(fsin.coordinate.x, fsin.coordinate.y);

	float X = texture(input_texture, vec3(coords, 0)).x;
	float Y = texture(input_texture, vec3(coords, 1)).x;
	float Z = texture(input_texture, vec3(coords, 2)).x;
	
	vec3 xyz = vec3(X, Y, Z);
	//vec3 rgb = pow(XYZ_to_RGB * xyz, vec3(1.0/2.2));
	//colorout = vec4(clamp(rgb, vec3(0.0), vec3(1.0)), 1.0);

	xout = X;
	yout = Y;
	zout = Z;
}

)";

}

#include "lodepng/lodepng.h"

#define TINYEXR_IMPLEMENTATION
#include "ext/tinyexr.h"

#include <chrono>
using clk = std::chrono::high_resolution_clock;
inline void print_time(const std::string& str, const clk::duration& diff, double factor = 1.0) {
	fprintf(stdout, "%s : %g\n ms", str.c_str(), ((double) diff.count()) / (factor * 1e6));
};

void saveEXR(const std::string& filepath, uint32_t width, uint32_t height, const std::vector<const float*>& buffers)
{
	EXRHeader header;
	InitEXRHeader(&header);

	EXRImage image;
	InitEXRImage(&image);

	image.num_channels = buffers.size();
	image.images = (uint8_t**) buffers.data();
	image.width = width;
	image.height = height;

	std::vector<EXRChannelInfo> channelInfos(image.num_channels);
	std::vector<int> pixelTypes(image.num_channels);
	std::vector<int> requestedPixelTypes(image.num_channels);
	if (channelInfos.size() == 3) {
		channelInfos[0].name[0] = 'X';
		channelInfos[0].name[1] = '\0';
		channelInfos[1].name[0] = 'Y';
		channelInfos[1].name[1] = '\0';
		channelInfos[2].name[0] = 'Z';
		channelInfos[2].name[1] = '\0';

		pixelTypes[0] = TINYEXR_PIXELTYPE_FLOAT;
		pixelTypes[1] = TINYEXR_PIXELTYPE_FLOAT;
		pixelTypes[2] = TINYEXR_PIXELTYPE_FLOAT;
	}
	requestedPixelTypes = pixelTypes;

	header.num_channels = image.num_channels;
	header.channels = channelInfos.data();
	header.pixel_types = pixelTypes.data();
	header.requested_pixel_types = requestedPixelTypes.data();

	const char* err;
	int ret = SaveEXRImageToFile(&image, &header, filepath.c_str(), &err);
	if (ret != 0) {
		fprintf(stderr, "error saving exr to %s : %s", filepath.c_str(), err);
		return;
	}

	//FreeEXRHeader(&header);
	//FreeEXRImage(&image);
}

int main() {

	/////////////////////////////////////////////////////////////////////////////////////////
	////////////// EXR LOADING

	const std::string exr_filepath = "resources\\images\\null.exr";
	const std::string output_filepath = exr_filepath.substr(0, exr_filepath.find(".exr")) + "-mod.exr";
	int ret = 0;
	const char* err;

	// Parse Version
	EXRVersion exr_version;
	ret = ParseEXRVersionFromFile(&exr_version, exr_filepath.c_str());
	if (ret != 0) {
		fprintf(stderr, "could not read exr file : %s\n", exr_filepath.c_str());
		return -1;
	}

	if (exr_version.multipart) {
		fprintf(stderr, "this application cannot read multipart exr files\n");
		return -1;
	}

	// Parse Header
	EXRHeader exr_header;
	InitEXRHeader(&exr_header);
	ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, exr_filepath.c_str(), &err);
	if (ret != 0) {
		fprintf(stderr, "error reading exr file : %s\n", err);
		return -1;
	}

	// Load actual image
	EXRImage exr_image;
	InitEXRImage(&exr_image);
	ret = LoadEXRImageFromFile(&exr_image, &exr_header, exr_filepath.c_str(), &err);
	if (ret != 0) {
		fprintf(stderr, "error loading exr image : %s\n", err);
		return -1;
	}

	float xyzrgb_matrix[] = {
		2.0413690f, -0.5649464f, -0.3446944f,
		-0.9692660f, 1.8760108f, 0.0415560f,
		0.0134474f, -0.1183897f, 1.0154096f
	};

	// Prepare buffers
	std::size_t buffer_length = exr_image.width * exr_image.height;
	std::vector<uint8_t> rgb_buffer(buffer_length * 3);
	const float* xbuf = (const float*)exr_image.images[0];
	const float* ybuf = (const float*)exr_image.images[1];
	const float* zbuf = (const float*)exr_image.images[2];

	/////////////////////////////////////////////////////////////////////////////////////////
	////////////// LOFX RENDERING

	// Init LOFX
	uint32_t window_width = exr_image.width;
	uint32_t window_height = exr_image.height;
	lofx::init(glm::u32vec2(window_width, window_height), "4.4", true);
	atexit(lofx::terminate);

	// Simple fullscreen quad
	d3::Geometry quad = geotools::generate_quad(glm::vec2(1.0f, 1.0f))
		.generate_d3geom();
	glm::mat4 rectif = glm::mat4(1.0f);
	rectif = glm::scale(rectif, glm::vec3(2.0f, 2.0f, 1.0f));
	rectif = glm::translate(rectif, glm::vec3(-0.5f, -0.5f, 0.0f));

	// Preparing shader program
	lofx::Program passthrough_vertex_program = lofx::createProgram(lofx::ShaderType::Vertex, { postfx_shader_source::vertex });
	lofx::Program reinhard_global_program = lofx::createProgram(lofx::ShaderType::Fragment, { postfx_shader_source::fragment });
	lofx::Program passthrough_fragment_program = lofx::createProgram(lofx::ShaderType::Fragment, { postfx_shader_source::passthrough_fragment });

	lofx::Pipeline postfx_pipeline = lofx::createPipeline();
	lofx::Pipeline final_pipeline = lofx::createPipeline();
	postfx_pipeline.stages = { &passthrough_vertex_program, &reinhard_global_program };
	final_pipeline.stages = { &passthrough_vertex_program, &passthrough_fragment_program };

	// Texture sampler
	lofx::TextureSamplerParameters samplerParameters;
	samplerParameters.mag = lofx::TextureMagnificationFilter::Nearest;
	samplerParameters.min = lofx::TextureMinificationFilter::Linear;
	samplerParameters.s = lofx::TextureWrappping::Clamp;
	samplerParameters.t = lofx::TextureWrappping::Clamp;
	lofx::TextureSampler sampler = lofx::createTextureSampler(samplerParameters);

	// Texture
	lofx::Texture texture = lofx::createTexture(window_width, window_height, 3, &sampler, lofx::TextureTarget::Texture2dArray, lofx::TextureInternalFormat::R32F);
	lofx::send(&texture, xbuf, glm::u32vec3(0, 0, 0), glm::u32vec3(window_width, window_height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	lofx::send(&texture, ybuf, glm::u32vec3(0, 0, 1), glm::u32vec3(window_width, window_height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	lofx::send(&texture, zbuf, glm::u32vec3(0, 0, 2), glm::u32vec3(window_width, window_height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);

	// Framebuffer
	lofx::Renderbuffer renderbuffer = lofx::createRenderBuffer(window_width, window_height);
	lofx::Texture framebufferTexture = lofx::createTexture(window_width, window_height, 3, nullptr, lofx::TextureTarget::Texture2dArray, lofx::TextureInternalFormat::R32F);
	lofx::Framebuffer framebuffer = lofx::createFramebuffer();
	framebuffer.renderbuffer = renderbuffer;
	framebuffer.attachments = { framebufferTexture };
	lofx::build(&framebuffer);

	// Preparing draw properties
	lofx::DrawProperties offscreenDrawProperties;
	offscreenDrawProperties.pipeline = &final_pipeline;
	offscreenDrawProperties.textures["input_texture"] = &texture;
	offscreenDrawProperties.fbo = &framebuffer;

	lofx::DrawProperties drawProperties;
	drawProperties.pipeline = &final_pipeline;
	drawProperties.textures["input_texture"] = &framebufferTexture;

	/////////////////////////////////////////////////////////////////////////////////////////
	// REINHARD GLOBAL CONSTANTS

	// Initial constants
	const float stops = 1.0f;
	const float strength = 1.0f;

	// Global reinhard based tonemapping. We do not do the auto-exposure
	const float YWhite = pow(2.f, stops);
	const float rcpYWhite2 = 1.f / (YWhite * YWhite);
	const float a = 0.18f;

	// Strength : 1 for Reinhard, lower for more subtle effect.
	// 0.01 added for avoiding low values numerically instable
	const float exponent = 1.01f / (strength + 0.01f);
	const float k = pow((1.f - pow(a / (YWhite * YWhite), exponent)) / (1.f - pow(a, exponent)), strength);
	const float k_pow_e = pow(k, exponent);

	lofx::send(&reinhard_global_program, lofx::Uniform("strength", strength));
	lofx::send(&reinhard_global_program, lofx::Uniform("exponent", exponent));
	lofx::send(&reinhard_global_program, lofx::Uniform("k", k));
	lofx::send(&reinhard_global_program, lofx::Uniform("k_pow_e", k_pow_e));
	lofx::send(&reinhard_global_program, lofx::Uniform("rcpYWhite2", rcpYWhite2));
	lofx::send(&reinhard_global_program, lofx::Uniform("srcGain", 1.0f));

	/////////////////////////////////////////////////////////////////////////////////////////
	// LOOP

	//std::size_t count = 0;
	//clk::time_point tp_start = clk::now();
	//lofx::loop([&] {
	//	lofx::send(&texture, xbuf, glm::u32vec3(0, 0, 0), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	//	lofx::send(&texture, ybuf, glm::u32vec3(0, 0, 1), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	//	lofx::send(&texture, zbuf, glm::u32vec3(0, 0, 2), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);

	//	lofx::send(&passthrough_vertex_program, lofx::Uniform("model", rectif));
	//	d3::render(&quad, offscreenDrawProperties);
	//	d3::render(&quad, drawProperties);

	//	float* xbuf_ret = (float*)lofx::read(&framebuffer, 0, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	//	float* ybuf_ret = (float*)lofx::read(&framebuffer, 1, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	//	float* zbuf_ret = (float*)lofx::read(&framebuffer, 2, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	//	delete xbuf_ret;
	//	delete ybuf_ret;
	//	delete zbuf_ret;

	//	count++;
	//	if (count > 50) {
	//		count = 0;
	//		clk::time_point tp_end = clk::now();
	//		print_time("OpenCL Reinhard global filter time", tp_end - tp_start, 50.0);
	//		tp_start = clk::now();
	//	}
	//});

	clk::time_point tp_start = clk::now();
	lofx::send(&texture, xbuf, glm::u32vec3(0, 0, 0), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	lofx::send(&texture, ybuf, glm::u32vec3(0, 0, 1), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	lofx::send(&texture, zbuf, glm::u32vec3(0, 0, 2), glm::u32vec3(exr_image.width, exr_image.height, 1), lofx::ImageDataFormat::R, lofx::ImageDataType::Float);

	lofx::send(&passthrough_vertex_program, lofx::Uniform("model", rectif));
	d3::render(&quad, offscreenDrawProperties);
	//d3::render(&quad, drawProperties);

	float* xbuf_ret = (float*) lofx::read(&framebuffer, 0, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	float* ybuf_ret = (float*) lofx::read(&framebuffer, 1, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);
	float* zbuf_ret = (float*) lofx::read(&framebuffer, 2, window_width, window_height, lofx::ImageDataFormat::R, lofx::ImageDataType::Float);

	clk::time_point tp_end = clk::now();
	print_time("postfx time", tp_end - tp_start);

	saveEXR(output_filepath, exr_image.width, exr_image.height, { xbuf_ret, ybuf_ret, zbuf_ret });

	delete xbuf_ret;
	delete ybuf_ret;
	delete zbuf_ret;

	/////////////////////////////////////////////////////////////////////////////////////////
	// CLEANUP

	lofx::release(&sampler);
	lofx::release(&texture);
	lofx::release(&postfx_pipeline);
	d3::release(quad);

	return 0;
}