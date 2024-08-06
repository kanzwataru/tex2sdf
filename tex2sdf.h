/*
This is tex2sdf, a single-header C library for taking a black-and-white mask texture and creating an SDF out of it.
Please take a look at https://github.com/kanzwataru/tex2sdf for more info. (and/or the README.md)

#####################
#### QUICK START ####
#####################

Here is sample code for the simplest possible usage of this library.

	```
	#define TEX2SDF_IMPLEMENTATION
	#include "tex2sdf.h"

	{
		...
		struct T2S_Image input  = { .data = my_texture_data_u8, .width = 512, height = 512, channels = 1 };
		struct T2S_Image output = t2s_convert(input, t2s_get_default_options());
		...
	}
	```

This is an stb-style header-only library. Here is how to add the library to your codebase.
1. In one .c file, do #define TEX2SDF_IMPLEMENTATION followed by including the header
2. In wherever the library is used, include the header

Specific to this library, here is how to use it:
1. We need to fill out a T2S_Image with our input texture.
   Make sure that the pixel format is 8-bit UNORM.
   Color textures also work, they will have SDFs converted per-channel.

2. Fill out the T2S_Options struct.
   Use t2s_get_default_options() to first get working defaults, if you don't intend on changing all the options.

3. Run the t2s_convert() function with all the data passed into it.
   The texture returned will be in 8-bit UNORM format, with the same number of channels as the input.

4. When you are done with the output texture, call free() on the "data" field.

#########################
#### CHARACTERISTICS ####
#########################

The code has the following characteristics
* Can be compiled as C99 (C++ coming soon)
* Does not have any asserts or aborts or panics in non-debug mode
* Portable for all 64-bit platforms, does not have platform-specific or compiler-specific code
* Minimal standard library usage
* Malloc can be avoided (by using the _noalloc version of the function)

#######################
#### API STRUCTURE ####
#######################

This library is designed to be as simple and easy-to-use as possible, while still allowing more advanced features if needed.
Override the default options to change the behaviour of the conversion.

For now the library only has the one main function "t2s_convert()".
There is also a _noalloc version, but it is essentially the same function.
Backwards compatibility here is guaranteed, all behaviour changes would be done as extra options in T2S_Options.

##################################
#### CUSTOM MEMORY ALLOCATION ####
##################################

If you need to avoid memory allocations, use the _noalloc version (t2s_convert_noalloc).
You will need to pass in a T2S_Allocation with the necessary memory buffers filled in.

Since the amount of memory needed depends on the input settings, we cannot statically determine the sizes.
To know how much to allocate, first call the function with a zeroed-out allocation struct.

See the function comments for sample code.

#################
#### LICENSE ####
#################

This code is licensed under the MIT license.
Original C# Unity code written by Chris Cummings. (github.com/chriscummings100/signeddistancefields)
C port by Denis Patrut.

Please take a look at LICENSE.md and README.md
The original code was granted by MIT license in a comment in the author's blog article. The repo does not have a LICENSE.md.
All parts taken from the original repo are marked as such.
*/

#ifndef TEX2SDF_H
#define TEX2SDF_H

#include <stddef.h> // For size_t

/* Describes a texture
 * INPUT:  Fill this out with the texture input, to be passed to t2s_convert()
 * OUTPUT: You get out the result of the conversion.
 */
struct T2S_Image
{
	unsigned char *data; // Non-owning for INPUT and if calling t2s_convert_noalloc(). For OUTPUT with t2s_convert(), must call free().
	int width;
	int height;
	int channels; 		 // Number of channels in the texture. 4 for RGBA, 3 for RGB, 2 for RG, 1 for grayscale.

	int error; 		     // The error enum is stored here. If 0, there is no error. Call t2s_get_error_string() to get the error message. Do not set this yourself.
	int data_is_owned;   // Whether data is owned by this struct. Do not set this yourself.
};

/* Supply the options here.
 * For defaults, call t2s_get_default_options().
 */
struct T2S_Options
{
	float sdf_range; // How much to scale the SDF values. A larger value "spreads" the shape out further.
};

/* A memory region.
 * If you use the _noalloc API, you will need to allocate the "memory" field with the size in the "capacity" field.
 *
 * This struct is used in the T2S_Allocation below.
 */
struct T2S_MemoryRegion
{
	void *memory;
	size_t top;
	size_t capacity;
};

/* The memory regions that are needed for the function to run.
 * If you use the _noalloc API, you will need to allocate memory for each of these.
 */
struct T2S_Allocation
{
	struct T2S_MemoryRegion temporary_memory;    // Memory only needed during execution of the function. Free after calling.
	struct T2S_MemoryRegion return_data_memory;  // Memory that stores the data returned. Free whenever you're finished with the data.
};

/* Error enum values. The first (0) is success. */
enum
{
	TEX2SDF_ERR_NONE,
	TEX2SDF_ERR_ALLOC_FAILURE,
	TEX2SDF_ERR_PREALLOCATED_MEMORY_INCORRECT,
	TEX2SDF_ERR_TRIED_TO_FREE_NON_OWNING_IMAGE,

	TEX2SDF_ERR_COUNT
};

/* Get default working options. */
struct T2S_Options t2s_get_default_options(void);

/* Convert mask texture to SDF. This is the main function.
 * Fill in the structs that are passed to this function.
 *
 * MEMORY
 * This allocates with malloc!
 * The returned struct has an owning pointer ("data").
 * You will need to use free() on it.
 * Consider t2s_convert_noalloc() if you want to allocate manually.
 */
struct T2S_Image t2s_convert(struct T2S_Image input, struct T2S_Options options);

/* A version of the main conversion function that does not allocate.
 *
 * Call this once with "alloc" pointing to a zeroed-out T2S_Allocation.
 * The allocation struct will contain the memory sizes needed.
 * Provide pointers to the allocation, and call it again.
 *
 * Example code:
 * {
 *		...
 * 		struct T2S_Allocation alloc = {0};
 * 		t2s_convert_noalloc(input, options, &alloc);
 * 		alloc.temporary_memory.memory = calloc(alloc.temporary_memory.capacity, 1);
 * 		alloc.return_data_memory.memory = calloc(alloc.return_data_memory.capacity, 1);
 *
 * 		struct T2S_Image image = t2s_convert_noalloc(input, options, &alloc);
 * 		...
 * }
 */
struct T2S_Image t2s_convert_noalloc(struct T2S_Image input, struct T2S_Options options, struct T2S_Allocation *alloc);

/* Free the image returned.
 * This is necessary for images returned by t2s_convert.
 * This is meaningless for t2s_convert_noalloc, since you handle memory yourself inside of T2S_Allocation.
 */
int t2s_free_image(struct T2S_Image *image);

/* Call this with a valid error enum to get an error message in string format. */
const char *t2s_get_error_string(int error);

#endif // TEX2SDF_H

/*
 *
 *
 *    #####################################
 *   ##### IMPLEMENTATION BEGINS HERE ####
 *  #####################################
 *
 *
 * 
 *
 *
 *
 *
*/
#ifdef TEX2SDF_IMPLEMENTATION

#include <math.h>  // for sqrtf, fabsf
#include <float.h> // for FLT_MAX

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
static int t2s_at(const struct T2S_Image *image, int x, int y, int channel)
{
	return (y * image->width + x) * image->channels + channel;
}

// TODO: Rename this to have a prefix
static int t2s_channel_at(const struct T2S_ImageChannel *channel, int x, int y)
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

static void *_t2s_memory_region_alloc(struct T2S_MemoryRegion *region, size_t size)
{
	if(region->top + size > region->capacity) {
		return NULL;
	}

	void *out_pointer = (char *)region->memory + region->top;
	region->top += size;

	return out_pointer;
}

struct T2S_Options t2s_get_default_options(void)
{
	return (struct T2S_Options){
		.sdf_range = 32.0f
	};
}

struct T2S_Image t2s_convert(struct T2S_Image input, struct T2S_Options options)
{
	// 1. Find out how much memory to allocate
	struct T2S_Allocation allocation = {0};
	t2s_convert_noalloc(input, options, &allocation);

	// 2. Allocate
	allocation.temporary_memory.memory = calloc(allocation.temporary_memory.capacity, 1);
	allocation.return_data_memory.memory = calloc(allocation.return_data_memory.capacity, 1);

	if(!allocation.temporary_memory.memory || !allocation.return_data_memory.memory) {
		return (struct T2S_Image) { .error = TEX2SDF_ERR_ALLOC_FAILURE };
	}

	// 3. Execute
	struct T2S_Image image = t2s_convert_noalloc(input, options, &allocation);
	image.data_is_owned = 1; // Mark as owned, so it can be freed with t2s_free_image()

	// 4. Free the temporary memory
	free(allocation.temporary_memory.memory);

	// 5. Done
	return image;
}

struct T2S_Image t2s_convert_noalloc(struct T2S_Image input, struct T2S_Options options, struct T2S_Allocation *alloc)
{
	// 1. Determine amount of memory needed

	// Memory needed to return the data produced here
	const size_t return_data_memory_size = input.width * input.height * input.channels;

	// Memory needed temporarily while computing
	const size_t distance_buffer_size = input.width * input.height * sizeof(float);
	const size_t edge_buffer_size = input.width * input.height * sizeof(unsigned char);

	const size_t temporary_memory_size = distance_buffer_size + edge_buffer_size;

	// Check if we have enough memory, according to calculation above.
	if(alloc->return_data_memory.capacity != return_data_memory_size ||
	   alloc->temporary_memory.capacity != temporary_memory_size ||
	   !alloc->temporary_memory.memory ||
	   !alloc->return_data_memory.memory)
	{
		// NOTE: We expect this function to be called with no memory first, so this should not be a fatal error.
		//		 Just fill out the memory we expect and return.
		alloc->return_data_memory.capacity = return_data_memory_size;
		alloc->return_data_memory.top = 0;
		
		alloc->temporary_memory.capacity = temporary_memory_size;
		alloc->temporary_memory.top = 0;

		return (struct T2S_Image) {
			.error = TEX2SDF_ERR_PREALLOCATED_MEMORY_INCORRECT
		};
	}

	// 2. Suballocate the buffers
	struct T2S_Image output = input;
	output.data = alloc->return_data_memory.memory;

	struct T2S_ImageChannel scratch_channel = {
		.width = output.width,
		.height = output.height,
		.distance_buffer = _t2s_memory_region_alloc(&alloc->temporary_memory, distance_buffer_size),
		.edge_buffer = _t2s_memory_region_alloc(&alloc->temporary_memory, edge_buffer_size)
	};

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
				float value = scratch_channel.distance_buffer[y * output.width + x];
				value /= options.sdf_range;
				value = value < -1.0f ? -1.0f : value;
				value = value >  1.0f ?  1.0f : value;

				const unsigned char value_unorm = (unsigned char)((value * 0.5 + 0.5) * 255);
				output.data[t2s_at(&output, x, y, channel)] = value_unorm;
			}
		}
	}

	// 4. Done
	return output;
}

int t2s_free_image(struct T2S_Image *image)
{
	if(!image->data_is_owned) {
		return TEX2SDF_ERR_TRIED_TO_FREE_NON_OWNING_IMAGE;
	}

	free(image->data);
	image->data = NULL;

	return TEX2SDF_ERR_NONE;
}

const char *t2s_get_error_string(int error)
{
	static const char *error_table[TEX2SDF_ERR_COUNT] = {
		[TEX2SDF_ERR_NONE] = "Success",
		[TEX2SDF_ERR_ALLOC_FAILURE] = "Failed to allocate memory",
		[TEX2SDF_ERR_PREALLOCATED_MEMORY_INCORRECT] = "The memory passed in is not the size needed, or is not allocated. (This is a harmless error if you're calling the function the first time to find out the required memory size)",
		[TEX2SDF_ERR_TRIED_TO_FREE_NON_OWNING_IMAGE] = "An image was passed to t2s_free_image() that did not own its data pointer. This can happen if trying to free the input image, or if trying to free an image from t2s_convert_noalloc(). For the latter, please free your allocation block inside of T2S_Allocation."
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
			const float pixel_value = (float)input->data[t2s_at(input, x, y, input_channel)] / 255.0f;
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
	if(channel->edge_buffer[t2s_channel_at(channel, x, y)]) {
		return;
	}

	float distance = channel->distance_buffer[t2s_channel_at(channel, x, y)];

    //read current and sign, then correct sign to work with +ve distance
    float current = distance;
    float sign = current < 0 ? -1.0f : 1.0f;
    current *= sign;

    //find the smallest of the 2 horizontal neighbours (correcting for sign)
    float horizontalmin = FLT_MAX;
    if (x > 0) horizontalmin = t2s_min(horizontalmin, sign * channel->distance_buffer[t2s_channel_at(channel, x - 1, y)]);
    if (x < channel->width - 1) horizontalmin = t2s_min(horizontalmin, sign * channel->distance_buffer[t2s_channel_at(channel, x + 1, y)]);

    //find the smallest of the 2 vertical neighbours
    float verticalmin = FLT_MAX;
    if (y > 0) verticalmin = t2s_min(verticalmin, sign * channel->distance_buffer[t2s_channel_at(channel, x, y - 1)]);
    if (y < channel->height - 1) verticalmin = t2s_min(verticalmin, sign * channel->distance_buffer[t2s_channel_at(channel, x, y + 1)]);

	//solve eikonal equation in 2D
    float eikonal = _t2s_solve_eikonal_equation(horizontalmin, verticalmin);

    //either keep the current distance, or take the eikonal solution if it is smaller
    distance = sign * t2s_min(current, eikonal);

    //write
    channel->distance_buffer[t2s_channel_at(channel, x, y)] = distance;
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
