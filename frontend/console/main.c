#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../extern/stb_image_write.h"

#define TEX2SDF_IMPLEMENTATION
#include "../../tex2sdf.h"

int main(int argc, char **argv)
{
	// 1. Parse arguments
	if(argc != 3) {
		fprintf(stderr, "tex2sdf <input> <output.tga>\n");
		return 1;
	}

	const char *input_path = argv[1];
	const char *output_path = argv[2];

	// 2. Load data
	int w, h, channels;
    uint8_t *input_data = stbi_load(input_path, &w, &h, &channels, 0);

    if(!input_data) {
    	fprintf(stderr, "Could not load image %s\n", input_path);
    	return 1;
    }

    // 3. Convert to SDF
    const struct T2S_Image sdf = t2s_convert((struct T2S_Image){input_data, w, h, channels});

    if(sdf.error) {
    	fprintf(stderr, "Could not convert SDF: %s\n", t2s_get_error_string(sdf.error));
    	return 1;
    }

    // 4. Output data
    int write_result = stbi_write_tga(output_path, sdf.width, sdf.height, sdf.channels, sdf.data);
    if(!write_result)
    {
    	fprintf(stderr, "Failed to write output image to: %s\n", output_path);
    }

    // NOTE: We leak all memory, because this is a single-run console app.
	return 0;
}
