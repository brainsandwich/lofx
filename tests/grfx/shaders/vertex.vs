#version 440
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_packing : enable

layout (location = 0) in vec3 coordinate;
layout (location = 1) in vec3 normal;

out gl_PerVertex {
	vec4 gl_Position;
};

out output_block_t {
	vec3 coordinate;
	vec3 normal;
} vsout;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

mat3 computeTBN(in vec3 normal) {
	vec3 tangent;
	vec3 bitangent;
	
	vec3 c1 = cross(normal, vec3(0.0, 0.0, 1.0)); 
	vec3 c2 = cross(normal, vec3(0.0, 1.0, 0.0)); 
	
	if (length(c1) > length(c2))
		tangent = c1;
	else
		tangent = c2;
	
	tangent = normalize(tangent);
	
	bitangent = cross(normal, tangent); 
	bitangent = normalize(bitangent);

	return mat3(tangent, bitangent, normal);
}

void main() {
	vec3 transformed_coords	= coordinate;
	gl_Position = projection * view * model * vec4(coordinate, 1.0);
	vsout.coordinate = coordinate;
	vsout.normal = mat3(transpose(inverse(model))) * normal;
}