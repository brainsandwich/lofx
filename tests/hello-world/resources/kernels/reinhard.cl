constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

/**
 * Reinhard Global filter
 *
 * @param input_channels : input channels (2D NxR:Float)
 * @param output_channels : output channels (2D NxR:Float)
 * @param strength
 * @param exponent
 * @param k
 * @param k_pow_e
 * @param rcpYWhite2
 * @param srcGain
 */
kernel void reinhard_global(
	read_only image2d_array_t input_channels,
	write_only image2d_array_t output_channels,
	float strength,
	float exponent,
	float k,
	float k_pow_e,
	float rcpYWhite2,
	float srcGain)
{
	const int4 pos = (int4)(get_global_id(0), get_global_id(1), 0, 0);

	// Get pixel XYZ values
	float x = read_imagef(input_channels, sampler, (int4)(pos.x, pos.y, 0, 0)).x;
	float y = read_imagef(input_channels, sampler, (int4)(pos.x, pos.y, 1, 0)).x;
	float z = read_imagef(input_channels, sampler, (int4)(pos.x, pos.y, 2, 0)).x;

	// Compute luminance
	float luminance = srcGain * y;

	// Intermediary 
	float f_nom = exp(exponent * log(luminance * rcpYWhite2));
	float f_denom = 1.0f + exp(exponent * log(k * luminance));
	float f = k_pow_e * f_nom / f_denom;

	// Final reinhard factor
	float reinhard_factor = exp(strength * log(f));

	// Write transformed pixels to output_channels
	write_imagef(output_channels, (int4)(pos.x, pos.y, 0, 0), (float4)(x * reinhard_factor, 0, 0, 0));
	write_imagef(output_channels, (int4)(pos.x, pos.y, 1, 0), (float4)(y * reinhard_factor, 0, 0, 0));
	write_imagef(output_channels, (int4)(pos.x, pos.y, 2, 0), (float4)(z * reinhard_factor, 0, 0, 0));
}