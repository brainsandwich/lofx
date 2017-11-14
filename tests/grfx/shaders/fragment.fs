#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

in input_block_t {
	vec3 coordinate;
	vec3 normal;
} fsin;

layout (location = 0) out vec4 colorout;

uniform float time;

void main() {
	colorout = vec4(fsin.coordinate.xyz, 1.0);
}