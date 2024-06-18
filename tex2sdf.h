 /*
 	TODO: Add description
 */

#ifndef TEX2SDF_H
#define TEX2SDF_H

struct Tex2SDF_Image
{
	unsigned char *data;
	int width;
	int height;
	int channels;

	int error;
};

enum {
	TEX2SDF_ERR_NONE,
	TEX2SDF_ERR_ALLOC,

	TEX2SDF_ERR_COUNT
};

struct Tex2SDF_Image tex2sdf_convert(struct Tex2SDF_Image input);
const char *tex2sdf_get_error_string(int error);

#endif // TEX2SDF_H

#ifdef TEX2SDF_IMPLEMENTATION

struct Tex2SDF_ImageChannel
{
	float *distance_buffer;
	unsigned char *edge_buffer;
};

void _tex2sdf_load_from_texture_antialiased(const struct Tex2SDF_ImageChannel *channel, const struct Tex2SDF_Image *input, int input_channel);
void _tex2sdf_eikonal_sweep(const struct Tex2SDF_ImageChannel *channel, const struct Tex2SDF_Image *input);

// TODO: Rename this to have a prefix
static int at(const struct Tex2SDF_Image *image, int x, int y, int channel)
{
	return y * image->width + x * image->channels + channel;
}

static float tex2sdf_lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

struct Tex2SDF_Image tex2sdf_convert(struct Tex2SDF_Image input)
{
	// 1. Allocate memory for output
	struct Tex2SDF_Image output = input;
	output.data = malloc(output.width * output.height * output.channels);

	if(!output.data) {
		output = (struct Tex2SDF_Image){0};
		output.error = TEX2SDF_ERR_ALLOC;

		return output;
	}

	// 2. Allocate scratch memory
	struct Tex2SDF_ImageChannel scratch_channel = {
		.distance_buffer = malloc(output.width * output.height * sizeof(float)),
		.edge_buffer = malloc(output.width * output.height * sizeof(float))
	};

	if(!scratch_channel.distance_buffer || !scratch_channel.edge_buffer) {
		output = (struct Tex2SDF_Image){0};
		output.error = TEX2SDF_ERR_ALLOC;

		return output;
	}

	// 3. Run SDF conversion (Eikonal sweep)
	for(int channel = 0; channel < input.channels; ++channel)
	{
		// - Populate distance/edge buffers
		_tex2sdf_load_from_texture_antialiased(&scratch_channel, &input, channel);

		// - Sweep Eikonal
		_tex2sdf_eikonal_sweep(&scratch_channel, &input);

		// - Write into output image
		for(int y = 0; y < output.height; ++y) {
			for(int x = 0; x < output.width; ++x) {
				const float sdf_range = 128.0f; // TODO: Don't hard-code

				float value = scratch_channel.distance_buffer[y * output.width + x];
				value /= sdf_range;
				value = value < -1.0f ? -1.0f : value;
				value = value >  1.0f ?  1.0f : value;

				const unsigned char value_unorm = (unsigned char)((value * 0.5 + 0.5) * 255);

				output.data[at(&output, x, y, channel)] = value_unorm;
			}
		}
	}

	// 4. Cleanup
	free(scratch_channel.distance_buffer);
	free(scratch_channel.edge_buffer);

	return output;
}

const char *tex2sdf_get_error_string(int error)
{
	static const char *error_table[TEX2SDF_ERR_COUNT] = {
		[TEX2SDF_ERR_NONE] = "Success",
		[TEX2SDF_ERR_ALLOC] = "Failed to allocate memory"
	};

	if(error < 0 || error >= TEX2SDF_ERR_COUNT) {
		return "Invalid error code!";
	}

	return error_table[error];
}

void _tex2sdf_load_from_texture_antialiased(const struct Tex2SDF_ImageChannel *channel, const struct Tex2SDF_Image *input, int input_channel)
{
	// Licensed as MIT from:
	// https://github.com/chriscummings100/signeddistancefields

	for(int y = 0; y < input->height; ++y) {
		for(int x = 0; x < input->width; ++x) {
	        //r==1 means solid pixel, and r==0 means empty pixel and r==0.5 means half way between the 2
	        //interpolate between 'a bit outside' and 'a bit inside' to get approximate distance
			const float pixel_value = (float)input->data[at(input, x, y, input_channel)] / 255.0f;
			channel->distance_buffer[y * input->width + x] = tex2sdf_lerp(0.75f, -0.75f, pixel_value);
		}		
	}
}

void _tex2sdf_eikonal_sweep(const struct Tex2SDF_ImageChannel *channel, const struct Tex2SDF_Image *input)
{
	// Licensed as MIT from:
	// https://github.com/chriscummings100/signeddistancefields

	// TODO: Implement
}

#endif // TEX2SDF_IMPLEMENTATION
