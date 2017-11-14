/*
constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

kernel void func(
		read_only image2d_array_t input,
		write_only image2d_t output
	) {

	const int4 pos = { get_global_id(0), get_global_id(1), 0, 0 };

	const int size = 10;
	const int layers = get_image_array_size(input);
	const int4 offset = { 0, 0, 0, 0 };

	float3 sum = read_imagef(input, sampler, pos).xyz;
	for (offset.z = 0; offset.z < layers; offset.z++) {
		float4 pixel_in = read_imagef(input, sampler, pos + offset);
		sum.xyz += pixel_in.xyz / (offset.z + 1);
	}
	write_imagef(output, pos.xy, (float4)(sum / layers, 1.0f));
}

kernel void copy(
		read_only image2d_array_t input,
		write_only image2d_array_t output
	) {

	const int4 pos = { get_global_id(0), get_global_id(1), 0, 0 };
	const int channel_count = get_image_array_size(input);

	for (int z = 0; z < channel_count; z++)
		write_imagef(output, (int4)(pos.x, pos.y, z, 0), read_imagef(input, sampler, (int4)(pos.x, pos.y, z, 0)));
}
*/

/*
const sampler_t sampler =
    CLK_NORMALIZED_COORDS_TRUE |
    CLK_ADDRESS_CLAMP |
    CLK_FILTER_LINEAR;

kernel void resize(
	read_only image2d_array_t input,
	write_only image2d_array_t output)
{
	const int4 pos = { get_global_id(0), get_global_id(1), 0, 0 };
	const int channel_count = get_image_array_size(input);

	int w = get_image_width(output);
	int h = get_image_height(output);

	float inX = pos.x / (float) w;
	float inY = pos.y / (float) h;
	float4 posIn = (float4) (inX, inY, 0, 0);

	for (int z = 0; z < channel_count; z++) {
		float4 pixel = read_imagef(input, sampler, posIn + (float4)(0, 0, z, 0));
		write_imagef(output, pos + (int4)(0, 0, z, 0), pixel);
	}
}
*/

const sampler_t sampler =
    CLK_NORMALIZED_COORDS_TRUE |
    CLK_ADDRESS_CLAMP |
    CLK_FILTER_LINEAR;

kernel void resize(
	read_only image2d_array_t input,
	write_only image2d_array_t output)
{
	const int channel_count = get_image_array_size(input);
	int4 output_coords = { get_global_id(0), get_global_id(1), 0, 0 };
	float4 input_coords = { (float) output_coords.x / (float) get_image_width(output), (float) output_coords.y / (float) get_image_height(output), 0, 0 };

	for (int z = 0; z < channel_count; z++) {
		uint4 pixel = read_imageui(input, sampler, input_coords + (float4)(0, 0, z, 0));
		write_imageui(output, output_coords + (int4)(0, 0, z, 0), pixel);
	}
}