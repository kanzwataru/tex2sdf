 /*
 	TODO: Add description
 */

#ifndef TEX2SDF_H
#define TEX2SDF_H

struct T2S_Image
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

struct T2S_Image t2s_convert(struct T2S_Image input);
const char *t2s_get_error_string(int error);

#endif // TEX2SDF_H

/*
 * Implementation Starts Here...
 * 
 *
 */
#ifdef TEX2SDF_IMPLEMENTATION

#include "math.h"  // for sqrtf, fabsf
#include "float.h" // for FLT_MAX

struct T2S_ImageChannel
{
	int width;
	int height;
	float *distance_buffer;
	unsigned char *edge_buffer;
};

void _t2s_load_from_texture_antialiased(const struct T2S_ImageChannel *channel, const struct T2S_Image *input, int input_channel);
void _t2s_eikonal_sweep(const struct T2S_ImageChannel *channel);

// TODO: Rename this to have a prefix
static int at(const struct T2S_Image *image, int x, int y, int channel)
{
	return (y * image->width + x) * image->channels + channel;
}

// TODO: Rename this to have a prefix
static int channel_at(const struct T2S_ImageChannel *channel, int x, int y)
{
	return y * channel->width + x;
}

static float t2s_lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

static float t2s_min(float a, float b)
{
	return a < b ? a : b;
}

//static float t2s_max(float a, float b)
//{
//	return a > b ? a : b;
//}

struct T2S_Image t2s_convert(struct T2S_Image input)
{
	// 1. Allocate memory for output
	struct T2S_Image output = input;
	output.data = malloc(output.width * output.height * output.channels);

	if(!output.data) {
		output = (struct T2S_Image){0};
		output.error = TEX2SDF_ERR_ALLOC;

		return output;
	}

	// 2. Allocate scratch memory
	struct T2S_ImageChannel scratch_channel = {
		.width = output.width,
		.height = output.height,
		.distance_buffer = malloc(output.width * output.height * sizeof(float)),
		.edge_buffer = malloc(output.width * output.height * sizeof(float))
	};

	if(!scratch_channel.distance_buffer || !scratch_channel.edge_buffer) {
		output = (struct T2S_Image){0};
		output.error = TEX2SDF_ERR_ALLOC;

		return output;
	}

	// 3. Run SDF conversion (Eikonal sweep)
	for(int channel = 0; channel < input.channels; ++channel)
	{
		// - Populate distance/edge buffers
		_t2s_load_from_texture_antialiased(&scratch_channel, &input, channel);

		// - Sweep Eikonal
		_t2s_eikonal_sweep(&scratch_channel);

		// - Write into output image
		for(int y = 0; y < output.height; ++y) {
			for(int x = 0; x < output.width; ++x) {
				//const float sdf_range = 128.0f; // TODO: Don't hard-code
				const float sdf_range = 32.0f; // TODO: Don't hard-code

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

const char *t2s_get_error_string(int error)
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

/*
 * [Begin port of chriscummings100/signeddistancefields code]
 *
 * All code that follows is licensed as MIT from:
 * https://github.com/chriscummings100/signeddistancefields
 */

int _t2s_is_outer_pixel(const struct T2S_ImageChannel *channel, int x, int y);
int _t2s_is_edge_pixel(const struct T2S_ImageChannel *channel, int x, int y);
float _t2s_solve_eikonal_equation(float horizontal, float vertical);
void _t2s_solve_eikonal(const struct T2S_ImageChannel *channel, int x, int y);
void _t2s_clear_and_mark_non_edge_pixels(const struct T2S_ImageChannel *channel);

int _t2s_is_outer_pixel(const struct T2S_ImageChannel *channel, int x, int y)
{
    //test if we consider pixel as outside the geometry (+ve distance)
    //note: pixels outside the bounds are considered 'outer'
	if (x < 0 || y < 0 || x >= channel->width || y >= channel->height) {
        return 1;
	}
    else {
        return channel->distance_buffer[y * channel->width + x] >= 0;
    }
}

int _t2s_is_edge_pixel(const struct T2S_ImageChannel *channel, int x, int y)
{
	// TODO: This is copied mostly-verbatim, maybe clean it up to be clearer what's going on.
	int is_outer = _t2s_is_outer_pixel(channel, x, y);
    if (is_outer != _t2s_is_outer_pixel(channel, x - 1, y - 1)) return 1; //[-1,-1]
    if (is_outer != _t2s_is_outer_pixel(channel, x, y - 1)) return 1;     //[ 0,-1]
    if (is_outer != _t2s_is_outer_pixel(channel, x + 1, y - 1)) return 1; //[+1,-1]
    if (is_outer != _t2s_is_outer_pixel(channel, x - 1, y)) return 1;     //[-1, 0]
    if (is_outer != _t2s_is_outer_pixel(channel, x + 1, y)) return 1;     //[+1, 0]
    if (is_outer != _t2s_is_outer_pixel(channel, x - 1, y + 1)) return 1; //[-1,+1]
    if (is_outer != _t2s_is_outer_pixel(channel, x, y + 1)) return 1;     //[ 0,+1]
    if (is_outer != _t2s_is_outer_pixel(channel, x + 1, y + 1)) return 1; //[+1,+1]
    return 0;
}

void _t2s_clear_and_mark_non_edge_pixels(const struct T2S_ImageChannel *channel)
{
	// 1. Clear and mark non-edge pixels
    // Cleans the field down so only pixels that lie on an edge 
    // contain a valid value. All others will either contain a
    // very large -ve or +ve value just to indicate inside/outside
    for(int y = 0; y < channel->height; ++y) {
    	for(int x = 0; x < channel->width; ++x) {
    		const int i = y * channel->width + x;

    		const int is_edge = _t2s_is_edge_pixel(channel, x, y);

    		channel->edge_buffer[i] = (unsigned char)is_edge;
    		if(!is_edge) {
    			channel->distance_buffer[i] = channel->distance_buffer[i] > 0.0f ? 99999.0f : -99999.0f;
    		}
    	}
    }
}

void _t2s_load_from_texture_antialiased(const struct T2S_ImageChannel *channel, const struct T2S_Image *input, int input_channel)
{
	for(int y = 0; y < input->height; ++y) {
		for(int x = 0; x < input->width; ++x) {
	        //r==1 means solid pixel, and r==0 means empty pixel and r==0.5 means half way between the 2
	        //interpolate between 'a bit outside' and 'a bit inside' to get approximate distance
			const float pixel_value = (float)input->data[at(input, x, y, input_channel)] / 255.0f;
			channel->distance_buffer[y * input->width + x] = t2s_lerp(0.75f, -0.75f, pixel_value);
		}		
	}
}

float _t2s_solve_eikonal_equation(float horizontal, float vertical)
{
    if (fabsf(horizontal - vertical) < 1.0f)
    {
    	// Solve Eikonal 2D
        float sum = horizontal + vertical;
        float dist = sum * sum - 2.0f * (horizontal * horizontal + vertical * vertical - 1.0f);
        return 0.5f * (sum + sqrtf(dist));
    }
    else
    {
    	// Solve Eikonal 1D
        return t2s_min(horizontal, vertical) + 1.0f;
    }
}

void _t2s_solve_eikonal(const struct T2S_ImageChannel *channel, int x, int y)
{
	if(channel->edge_buffer[channel_at(channel, x, y)]) {
		return;
	}

	float distance = channel->distance_buffer[channel_at(channel, x, y)];

    //read current and sign, then correct sign to work with +ve distance
    float current = distance;
    float sign = current < 0 ? -1.0f : 1.0f;
    current *= sign;

    //find the smallest of the 2 horizontal neighbours (correcting for sign)
    float horizontalmin = FLT_MAX;
    if (x > 0) horizontalmin = t2s_min(horizontalmin, sign * channel->distance_buffer[channel_at(channel, x - 1, y)]);
    if (x < channel->width - 1) horizontalmin = t2s_min(horizontalmin, sign * channel->distance_buffer[channel_at(channel, x + 1, y)]);

    //find the smallest of the 2 vertical neighbours
    float verticalmin = FLT_MAX;
    if (y > 0) verticalmin = t2s_min(verticalmin, channel->distance_buffer[channel_at(channel, x, y - 1)]);
    if (y < channel->height - 1) verticalmin = t2s_min(verticalmin, sign * channel->distance_buffer[channel_at(channel, x, y + 1)]);

	//solve eikonal equation in 2D
    float eikonal = _t2s_solve_eikonal_equation(horizontalmin, verticalmin);

    //either keep the current distance, or take the eikonal solution if it is smaller
    distance = sign * t2s_min(current, eikonal);

    //write
    channel->distance_buffer[channel_at(channel, x, y)] = distance;
}

void _t2s_eikonal_sweep(const struct T2S_ImageChannel *channel)
{	
    //clean the field so any none edge pixels simply contain 99999 for outer
    //pixels, or -99999 for inner pixels. also marks pixels as edge/not edge
	_t2s_clear_and_mark_non_edge_pixels(channel);

	//sweep using eikonal algorithm in all 4 diagonal directions
    for(int x = 0; x < channel->width; ++x) {
        for(int y = 0; y < channel->height; ++y) {
            _t2s_solve_eikonal(channel, x, y);
        }
        for(int y = channel->height - 1; y >= 0; --y) {
            _t2s_solve_eikonal(channel, x, y);
        }
    }
    for (int x = channel->width - 1; x >=0; --x) {
        for(int y = 0; y < channel->height; ++y) {
            _t2s_solve_eikonal(channel, x, y);
        }
        for(int y = channel->height - 1; y >= 0; --y) {
            _t2s_solve_eikonal(channel, x, y);
        }
    }
}

/*
 * [End port of chriscummings100/signeddistancefields code]
 */

#endif // TEX2SDF_IMPLEMENTATION
