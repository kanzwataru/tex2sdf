/*
 * This file is a minimal example of using this library, for the pure purpose of ensuring it compiles as C++
 */
#define TEX2SDF_IMPLEMENTATION
#include "../tex2sdf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../frontend/extern/stb_image.h"

int main(int argc, char **argv)
{
	const char *input_path = "testing/assets/rectangles.png";

	int w, h, channels;
    uint8_t *input_data = stbi_load(input_path, &w, &h, &channels, 0);

    if(!input_data) {
    	fprintf(stderr, "Could not load image %s\n", input_path);
    	return 1;
    }

    struct T2S_Image image = { input_data, w, h, channels };
    const struct T2S_Image sdf = t2s_convert(image, t2s_get_default_options());

    if(sdf.error) {
    	fprintf(stderr, "Could not convert SDF: %s\n", t2s_get_error_string(sdf.error));
    	return 1;
    }

	return 0;
}
