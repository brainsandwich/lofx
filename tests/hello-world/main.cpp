#include "lut.hpp"
#include "LOFX/lofx.hpp"

struct BufferGeometry {
	std::vector<uint32_t> indices;
	std::vector<char> vertices;

	BufferGeometry() {}

	template <typename ... Args>
	BufferGeometry(const std::vector<uint32_t>& indices, const std::vector<Args>&... data)
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
};

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

const BufferGeometry quad = BufferGeometry(
	// uint32 Indices
	std::vector<uint32_t> {
		0, 1, 2,
		0, 2, 3
	},
	// XYZ float positions
	std::vector<float> {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f
	},
	// ABGR uint32 color
	std::vector<uint32_t> {
		0x000000ff,
		0x0000ff00,
		0x00ff0000,
		0x00777777
	}
);

#include <thread>
#include "storage.hpp"

struct Packet {
	uint8_t* data = nullptr;
	std::size_t size = 0;

	Packet() {}

	template <typename T>
	Packet(const T& value) {
		size = sizeof(T);
		data = new uint8_t[size];
		memcpy(data, &value, size);
	}

	template <typename T> T& unpack() const {
		return *reinterpret_cast<T*>(data);
	}

	void clear() {
		delete[] data;
	}
};

struct Receiver {
	std::string type;
	std::function<void(const Packet&)> onmessage;

	Receiver() {}

	template <typename Func>
	Receiver(const std::string& type, const Func& onmessage)
		: type(type)
		, onmessage(onmessage) {}
};

struct Pigeon {
	static std::unordered_map<std::string, std::deque<Packet>> messages;
	static std::unordered_map<std::string, std::list<const Receiver*>> receivers;

	static void registerReceiver(const Receiver* receiver) {
		receivers[receiver->type].push_back(receiver);
	}

	static void unregisterReceiver(const Receiver* receiver) {
		receivers[receiver->type].remove(receiver);
	}

	static void send(const std::string& message) {
		messages[message].push_back(Packet(nullptr));
	}

	template <typename T>
	static void send(const std::string& message, const T& data) {
		messages[message].push_back(Packet(data));
	}

	static void dispatch() {
		for (auto& pair : messages) {
			for (const auto& msg : pair.second) {
				for (const auto& recv : receivers[pair.first])
					recv->onmessage(msg);
			}

			for (auto& msg : pair.second)
				msg.clear();

			pair.second.resize(0);
		}
	}
};

std::unordered_map<std::string, std::deque<Packet>> Pigeon::messages;
std::unordered_map<std::string, std::list<const Receiver*>> Pigeon::receivers;

struct Thesaurus;

struct Composite {
	lut::UUID uuid = lut::UUID::Null;
	lut::storage::Market* market = nullptr;
	Thesaurus* thesaurus = nullptr;
	std::unordered_map<std::type_index, lut::storage::handle_t> components;

	Composite() {}
	Composite(const lut::UUID& uuid, lut::storage::Market* market)
		: uuid(uuid)
		, market(market) {}

	template <typename T, typename ... Args>
	void attach(Args&& ... args) {
		auto tid = std::type_index(typeid(T));
		if (components.count(tid))
			return;

		components[tid] = thesaurus->components_market->create<T>(*this, std::forward<Args>(args) ...);
	}

	template <typename T>
	void detach() {
		auto tid = std::type_index(typeid(T));
		if (!components.count(tid))
			return;

		market->release<T>(components[tid]);
		components.erase(tid);
	}

	template <typename T>
	T& get() {
		auto tid = std::type_index(typeid(T));
		return market->get<T>(components[tid]);
	}

	template <typename T>
	const T& get() const {
		auto tid = std::type_index(typeid(T));
		return market->get<T>(components[tid]);
	}

	template <typename T>
	bool contains() {
		auto tid = std::type_index(typeid(T));
		return components.count(tid) && market->isvalid<T>(components[tid]);
	}
};

struct Thesaurus {
	static std::unique_ptr<lut::storage::Market> components_market;
	static std::unordered_map<lut::UUID, Composite> composites;

	static void init() {
		components_market = std::make_unique<lut::storage::Market>();
	}

	static void clear() {
		components_market.release();
	}

	static Composite& createComposite() {
		lut::UUID uuid = lut::UUID::random();
		composites[uuid] = Composite(uuid, components_market.get());
		Pigeon::send("composite-created", uuid);
		return composites[uuid];
	}

	static void destroyComposite(Composite& composite) {
		for (auto& pair : composite.components)
			components_market->release(pair.first, pair.second);
	}
};

std::unique_ptr<lut::storage::Market> Thesaurus::components_market = nullptr;
std::unordered_map<lut::UUID, Composite> Thesaurus::composites;

using namespace std::chrono_literals;
using clk = std::chrono::high_resolution_clock;
auto prtm = [](const std::string& msg, const clk::duration& duration, double mul = 1.0) { lut::trace("{0} : {1} sec\n", msg, (double) duration.count() / 1e9 * mul); };
auto w8usr = []() { int k; scanf("%d", &k); };

int main() {
	//lut::detail::logtoconsole = true;

	//Thesaurus::init();
	//Receiver crecv("composite-created", [](const Packet& message) {
	//	lut::trace(
	//		"A Composite has been created, UUID : {}\n",
	//		message.unpack<lut::UUID>().tostring()
	//	);
	//});

	//Pigeon::registerReceiver(&crecv);

	//auto composite = Thesaurus::createComposite();
	//Pigeon::dispatch();
	//Pigeon::unregisterReceiver(&crecv);

	//auto composite_dos = Thesaurus::createComposite();
	//Pigeon::dispatch();

	//Thesaurus::clear();

	//w8usr();
	//return 0;

	lut::detail::loglevel = lut::LogLevel::Warn;
	lut::detail::logtoconsole = true;

	// Init LOFX
	lofx::init(glm::u32vec2(800, 800), "4.4");
	atexit(lofx::terminate);

	// Preparing vertex buffer
	lofx::Buffer vbo = lofx::createBuffer(lofx::BufferType::Vertex, lofx::BufferUsage::StaticDraw, quad.vertices.size());
	lofx::send(&vbo, (void*) quad.vertices.data());
	lofx::AttributePack pack = lofx::buildSequentialAttributePack({
		lofx::VertexAttribute(0, lofx::AttributeType::Float, 3, false, 4),
		lofx::VertexAttribute(1, lofx::AttributeType::UnsignedByte, 4, true, 4)
	});

	// Preparing index buffer
	lofx::Buffer ebo = lofx::createBuffer(lofx::BufferType::Index, lofx::BufferUsage::StaticDraw, quad.indices.size() * sizeof(uint32_t));
	lofx::send(&ebo, (void*) quad.indices.data());

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

	glm::mat4 model;
	lofx::send(&vertex_program, lofx::Uniform("model", model));

	// Preparing draw command
	lofx::DrawProperties draw_properties;
	draw_properties.fbo = &fbo;
	draw_properties.ebo = &ebo;
	draw_properties.vbo = &vbo;
	draw_properties.pack = &pack;
	draw_properties.pipeline = &pipeline;

	// Loop while window is not closed
	lofx::loop([&] {
		model = glm::rotate(model, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
		lofx::send(&vertex_program, lofx::Uniform("model", model));

		lofx::draw(draw_properties);
		std::this_thread::sleep_for(16ms);
	});

	// Cleanup
	lofx::release(&vbo);
	lofx::release(&ebo);
	lofx::release(&pipeline);

	// Kill LOFX
	lofx::terminate();
	
	return 0;
}