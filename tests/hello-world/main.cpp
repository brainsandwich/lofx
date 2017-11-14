#include "lofx/lofx.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <thread>
#include <fstream>
#include <filesystem>
#include <chrono>

using namespace std::chrono_literals;

namespace de {
	namespace detail {
		struct quad_t {
			static const float vertices[];
			static const uint8_t indices[];

			lofx::Buffer vertex_buffer;
			lofx::Buffer index_buffer;
			lofx::AttributePack attribute_pack;
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

				uniform mat3 uvtransform;
				uniform mat3 model;
				uniform mat3 view;

				void main() {
					vec3 coord = view * model * vec3(coordinate, 1.0);
					gl_Position = vec4(coord.xy, 0.0, 1.0);
					vout.position = coord.xy;
					vout.uv = (uvtransform * vec3(uv, 1.0)).xy;
				}
			)";

			const std::string fragment = R"(
				#version 440
				#extension GL_ARB_separate_shader_objects : enable
				#extension GL_ARB_shading_language_packing : enable

				layout (location = 0) out vec4 colorout;

				uniform sampler2D spritetexture;

				in vout_t {
					vec2 position;
					vec2 uv;
				} fsin;

				void main() {
					vec4 texcolor = texture(spritetexture, fsin.uv);
					colorout = vec4(texcolor.rgb, 1.0);
				}
			)";
		}
	}

	struct Sprite {
		lofx::Texture texture;

		glm::vec2 uv_offset = glm::uvec2();
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

	void draw(const Sprite& sprite, const lofx::DrawProperties& drawproperties) {
		if (detail::quad.vertex_buffer.id == 0) {
			detail::quad.vertex_buffer = lofx::createBuffer(lofx::BufferType::Vertex, sizeof(detail::quad.vertices));
			detail::quad.index_buffer = lofx::createBuffer(lofx::BufferType::Index, sizeof(detail::quad.indices));
			lofx::send(&detail::quad.vertex_buffer, detail::quad.vertices);
			lofx::send(&detail::quad.index_buffer, detail::quad.indices);

			lofx::BufferAccessor positions_accessor = lofx::createBufferAccessor(detail::quad.vertex_buffer, lofx::AttributeType::Float, 2, 4);
			lofx::BufferAccessor uvs_accessor = lofx::createBufferAccessor(detail::quad.vertex_buffer, lofx::AttributeType::Float, 2, 4);
			detail::quad.attribute_pack = lofx::buildFlatAttributePack({ positions_accessor, uvs_accessor });
			detail::quad.index_accessor = lofx::createBufferAccessor(detail::quad.index_buffer, lofx::AttributeType::UnsignedByte, 1, 6);
		}

		lofx::DrawProperties dp = drawproperties;

		dp.textures["spritetexture"] = &sprite.texture;
		dp.attributes = &detail::quad.attribute_pack;
		dp.indices = &detail::quad.index_accessor;

		glm::mat3 model_rotation = glm::mat3(
			cos(sprite.rotation), -sin(sprite.rotation), 0.0f,
			sin(sprite.rotation), cos(sprite.rotation), 0.0f,
			0.0f, 0.0f, 0.0f
		);
		glm::mat3 model_transform = model_rotation * glm::mat3(
			sprite.scale.x, 0.0f, 0.0f,
			0.0f, sprite.scale.y, 0.0f,
			sprite.position.x, sprite.position.y, 1.0f
		);

		glm::mat3 uv_rotation = glm::mat3(
			cos(sprite.uv_rotation), -sin(sprite.uv_rotation), 0.0f,
			sin(sprite.uv_rotation), cos(sprite.uv_rotation), 0.0f,
			0.0f, 0.0f, 1.0f
		);
		glm::mat3 uv_transform = uv_rotation * glm::mat3(
			sprite.uv_scale.x, 0.0f, sprite.uv_offset.x,
			0.0f, sprite.uv_scale.y, sprite.uv_offset.y,
			sprite.uv_offset.x, sprite.uv_offset.y, 1.0f
		);

		const lofx::Program* vertex_program = lofx::findStage(dp.pipeline, lofx::ShaderType::Vertex);
		lofx::send(vertex_program, lofx::Uniform("model", model_transform));
		lofx::send(vertex_program, lofx::Uniform("uvtransform", uv_transform));

		lofx::draw(dp);
	}

	void apply(const lofx::Program* program, View* view) {
		if (view->invalidated) {
			view->cached_transform = glm::mat3(
				view->scale.x * 1.0f / view->ratio, 0.0f, 0.0f,
				0.0f, view->scale.y, 0.0f,
				-view->center.x, -view->center.y, 1.0f
			);
			view->invalidated = false;
		}
			
		lofx::send(program, lofx::Uniform("view", view->cached_transform));
	}
}

int main(int argc, char** argv) {

	// init
	// ----
	uint32_t scrw = 1000, scrh = 600;
	lofx::setdbgCallback([](lofx::DebugMessageDetails details, const std::string& msg) {
		std::string shown_level = "";
		std::string show_source = "";
		switch (details.level) {
			case lofx::DebugLevel::Trace: shown_level = "trace"; break;
			case lofx::DebugLevel::Warn: shown_level = "warn"; break;
			case lofx::DebugLevel::Error: shown_level = "err"; break;
		}
		switch (details.source) {
			case lofx::DebugSource::Lofx: show_source = "lofx"; break;
			case lofx::DebugSource::Glfw: show_source = "glfw"; break;
			case lofx::DebugSource::OpenGL: show_source = "opengl"; break;
		}
		fprintf(stdout, "[%s %s] : %s\n", show_source.c_str(), shown_level.c_str(), msg.c_str());
	});
	lofx::init(glm::u32vec2(scrw, scrh), "4.4");
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
	sampler_parameters.s = lofx::TextureWrappping::Mirrored;
	sampler_parameters.t = lofx::TextureWrappping::Mirrored;

	lofx::TextureSampler sampler = lofx::createTextureSampler(sampler_parameters);
	lofx::Texture checkerboard_texture = lofx::createTexture(8, 8, 0, &sampler);
	lofx::send(&checkerboard_texture, checkerboard_image);

	// checkerboard sprite
	// ---------------
	de::Sprite sprite;
	sprite.uv_scale = glm::vec2(0.5f);
	sprite.texture = checkerboard_texture;

	de::Sprite other_sprite;
	other_sprite.uv_scale = glm::vec2(0.8f);
	other_sprite.scale = glm::vec2(1.5f);
	other_sprite.texture = checkerboard_texture;

	// graphics pipeline
	// -----------------
	lofx::Program vertex_program = lofx::createProgram(lofx::ShaderType::Vertex, { de::detail::shader::vertex });
	lofx::Program fragment_program = lofx::createProgram(lofx::ShaderType::Fragment, { de::detail::shader::fragment });
	lofx::Pipeline pipeline = lofx::createPipeline({ &vertex_program, &fragment_program });

	// framebuffers
	// -----------------
	lofx::Framebuffer defaultframebuffer = lofx::defaultFramebuffer();

	// draw properties
	// ---------------
	lofx::ClearProperties clearProperties;
	clearProperties.color = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

	lofx::DrawProperties drawproperties;
	drawproperties.fbo = &defaultframebuffer;
	drawproperties.pipeline = &pipeline;

	de::View view; 
	view.scale = glm::vec2(0.1f);
	view.ratio = ((float) scrw) / ((float) scrh);

	// draw loop
	// ---------
	uint32_t counter = 0;
	lofx::loop([&] {
		counter++;

		lofx::clear(&defaultframebuffer, clearProperties);
		de::apply(&vertex_program, &view);
		sprite.position = glm::vec2(cos((float) counter / 50.0f), sin((float)counter / 50.0f));
		other_sprite.position = 1.7f * glm::vec2(cos((float) counter / 68.0f), sin((float) counter / 64.0f)) + glm::vec2(1.3f, 0.0f);
		de::draw(sprite, drawproperties);
		de::draw(other_sprite, drawproperties);

		std::this_thread::sleep_for(16ms);
	});

	return 0;
}
