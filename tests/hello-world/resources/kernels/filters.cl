constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

#define MAX_CHANNELS 64

/**
 * CL version of filterSC_2DEX
 *
 * Blurs an image using a convolution filter and weighting it with
 * the SNR value at each pixel.
 *
 * Warning : The maximum number of channels allowed is hardcoded in this file. 
 *
 * @param snr : signal to noise ratio (2D R:Float)
 * @param input_channels : input channels (2D NxR:Float ; typically RGB)
 * @param output_channels : output channels (2D NxR:Float)
 * @param convol : convolution kernel (1D filtersize x Float)
 * @param strength : effect strength
 * @param filtersize : filter half width
 */
kernel void filterSC_2DEX(
	read_only image2d_t snr,
	read_only image2d_array_t input_channels,
	write_only image2d_array_t output_channels,
	global const float* convol,
	int filtersize,
	float strength)
{
	// Prepare variables
	const int channel_count = get_image_array_size(input_channels);
	const int4 pos = (int4)(get_global_id(0), get_global_id(1), 0, 0);
	float sum[MAX_CHANNELS];

	float current_snr = read_imagef(snr, sampler, pos.xy).x;
	float weight = 0.0f;
	for (int z = 0; z < channel_count; z++)
		sum[z] = 0.0f;

	// Convolve
	for (int y = -filtersize; y < filtersize; y++) {
		for (int x = -filtersize; x < filtersize; x++) {

			// Retrieve SNR at current pixel 
			float current_snrf = read_imagef(snr, sampler, pos.xy + (int2)(x, y)).x;

			// Square distance
			float dist2 = 0.0f;
			for (int z = 0; z < channel_count; z++) {
				float value = read_imagef(input_channels, sampler, pos + (int4)(0, 0, z, 0)).x;
				float value_offset = read_imagef(input_channels, sampler, pos + (int4)(x, y, z, 0)).x;
				float dist = current_snr * fabs(value - value_offset) / (value * strength);
				dist2 += dist * dist;
			}

			// Weight neighbor pixel
			float pix_weight = convol[abs(y)]
				* convol[abs(x)]
				* exp(-sqrt(dist2))
				* current_snrf;

			if (isfinite(pix_weight) == 1) {
				for (int z = 0; z < channel_count; z++)
					sum[z] += read_imagef(input_channels, sampler, pos + (int4)(x, y, z, 0)).x * pix_weight;

				weight += pix_weight;
			}
		}
	}

	float invweight = 1.0 / weight;

	// Write to output
	if (weight > 0) {
		for (int z = 0; z < channel_count; z++)
			write_imagef(output_channels, pos + (int4)(0, 0, z, 0), sum[z] * invweight);
	}

}

/**
 * CL version of filterSC_2DEX
 *
 * Blurs an image using a convolution filter and weighting it with
 * the SNR value at each pixel. Same as filterSC_2DEX but uses a 
 * seperated version. The user must feed a value to tell which pass (horizontal / vertical)
 * should be done.
 *
 * Warning : The maximum number of channels allowed is hardcoded in this file. 
 *
 * @param snr : signal to noise ratio (2D R:Float)
 * @param input_channels : input channels (2D NxR:Float ; typically RGB)
 * @param output_channels : output channels (2D NxR:Float)
 * @param convol : convolution kernel (1D filtersize x Float)
 * @param strength : effect strength
 * @param filtersize : filter half width
 * @param horizontal : when != 0, do an horizontal pass
 */
kernel void separated_filterSC_2DEX(
	read_only image2d_t snr,
	read_only image2d_array_t input_channels,
	write_only image2d_array_t output_channels,
	global const float* convol,
	int filtersize,
	float strength,
	int horizontal)
{
	// Prepare variables
	const int channel_count = get_image_array_size(input_channels);
	const int4 pos = (int4)(get_global_id(0), get_global_id(1), 0, 0);
	float sum[MAX_CHANNELS];

	float current_snr = read_imagef(snr, sampler, pos.xy).x;
	float weight = 0.0f;
	for (int z = 0; z < channel_count; z++)
		sum[z] = 0.0f;

	// Convolve
	if (horizontal != 0) {
		for (int x = -filtersize; x < filtersize; x++) {

			// Retrieve SNR at current pixel 
			float current_snrf = read_imagef(snr, sampler, pos.xy + (int2)(x, 0)).x;

			// Square distance
			float dist = 0.0f;
			for (int z = 0; z < channel_count; z++) {
				float value = read_imagef(input_channels, sampler, pos + (int4)(0, 0, z, 0)).x;
				float value_offset = read_imagef(input_channels, sampler, pos + (int4)(x, 0, z, 0)).x;
				dist = current_snr * fabs(value - value_offset) / (value * strength);
			}

			// Weight neighbor pixel
			float pix_weight = convol[abs(x)]
				* exp(-dist)
				* current_snrf;

			if (isfinite(pix_weight) == 1) {
				for (int z = 0; z < channel_count; z++)
					sum[z] += read_imagef(input_channels, sampler, pos + (int4)(x, 0, z, 0)).x * pix_weight;

				weight += pix_weight;
			}
		}
	} else {
		for (int y = -filtersize; y < filtersize; y++) {

			// Retrieve SNR at current pixel 
			float current_snrf = read_imagef(snr, sampler, pos.xy + (int2)(0, y)).x;

			// Square distance
			float dist = 0.0f;
			for (int z = 0; z < channel_count; z++) {
				float value = read_imagef(input_channels, sampler, pos + (int4)(0, 0, z, 0)).x;
				float value_offset = read_imagef(input_channels, sampler, pos + (int4)(0, y, z, 0)).x;
				dist = current_snr * fabs(value - value_offset) / (value * strength);
			}

			// Weight neighbor pixel
			float pix_weight = convol[abs(y)]
				* exp(-dist)
				* current_snrf;

			if (isfinite(pix_weight) == 1) {
				for (int z = 0; z < channel_count; z++)
					sum[z] += read_imagef(input_channels, sampler, pos + (int4)(0, y, z, 0)).x * pix_weight;

				weight += pix_weight;
			}
		}
	}

	// Write to output
	if (weight > 0) {
		for (int z = 0; z < channel_count; z++)
			write_imagef(output_channels, pos + (int4)(0, 0, z, 0), sum[z]);
	}

}

/**
 * Bloom filter
 *
 * Does a simple bloom
 *
 * @param input : input image with N channels
 * @param output : output image with N channels
 */
kernel void bloom(
		read_only image2d_array_t input,
		write_only image2d_array_t output)
{

	const int4 pos = { get_global_id(0), get_global_id(1), 0, 0 };
	const int channel_count = get_image_array_size(input);
	const int filtersize = 16;

	float sum = 0.0f;
	for (int y = -filtersize; y < filtersize; y++) {
		for (int x = -filtersize; x < filtersize; x++) {
			for (int z = 0; z < channel_count; z++)
				sum += read_imagef(input, sampler, (int4)(pos.x, pos.y, z, 0) + (int4)(x, y, z, 0)).x;
		}
	}

	sum /= ((filtersize * 2) + 1) * channel_count;

	for (int z = 0; z < channel_count; z++)
		write_imagef(output, (int4)(pos.x, pos.y, z, 0), sum * read_imagef(input, sampler, (int4)(pos.x, pos.y, z, 0)));
}

const sampler_t samplerIn =
    CLK_NORMALIZED_COORDS_TRUE |
    CLK_ADDRESS_CLAMP |
    CLK_FILTER_LINEAR;

const sampler_t samplerOut =
    CLK_NORMALIZED_COORDS_FALSE |
    CLK_ADDRESS_CLAMP |
    CLK_FILTER_NEAREST;

kernel void resize(
	read_only image2d_array_t input
	read_only image2d_array_t output,
	float xratio,
	float yratio)
{
	const int4 pos = { get_global_id(0), get_global_id(1), 0, 0 };
	const int channel_count = get_image_array_size(input);

	int w = get_image_width(output);
	int h = get_image_height(output);

	int outX = get_global_id(0);
	int outY = get_global_id(1);
	int2 posOut = {outX, outY};

	float inX = outX / (float) w;
	float inY = outY / (float) h;
	float2 posIn = (float2) (inX, inY);

	float4 pixel = read_imagef(input, samplerIn, posIn);
	write_imagef(output, posOut, pixel);
}

/**
 * CL version of filter_TV
 *
 * Blurs an image using a convolution filter and weighting it with
 * the SNR value at each pixel.
 *
 * Warning : The maximum number of channels allowed is hardcoded in this file. 
 *
 * @param snr : signal to noise ratio (2D R:Float)
 * @param input_channels : input channels (2D NxR:Float ; typically RGB)
 * @param output_channels : output channels (2D NxR:Float)
 * @param convol : convolution kernel (1D filtersize x Float)
 * @param strength : effect strength
 * @param filtersize : filter half width
 */
kernel void filter_TV(
	read_only image2d_t snr,
	read_only image2d_array_t input_channels,
	write_only image2d_array_t output_channels,
	global const float* convol,
	int filtersize,
	float strength,
	int horizontal)
{

}