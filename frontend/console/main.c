#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../extern/stb_image_write.h"

#define TEX2SDF_IMPLEMENTATION
#include "../../tex2sdf.h"

static void show_help(void)
{
	fprintf(stderr, "tex2sdf <input> <output.tga> [flags]\n\n");
	fprintf(stderr, "\t--sdf_range [number]\n");
}

struct Argument_Parser {
	int top;
	int argc;
	const char **argv;
};

static bool is_flag(const char *s)
{
	return s[0] != 0 && s[0] == '-' &&
		   s[1] != 0 && s[1] == '-' &&
		   s[2] != 0 && isalpha(s[2]);
}

static const char **pop_arguments_raw(struct Argument_Parser *parser, int argument_count)
{
	if(parser->top + argument_count > parser->argc) {
		printf("Not enough arguments\n\n");
		show_help();
		exit(1);
		return NULL;
	}

	const char **arguments = parser->argv + parser->top;
	parser->top += argument_count;

	return arguments;
}

static const char **pop_arguments(struct Argument_Parser *parser, int argument_count)
{
	const char **arguments = pop_arguments_raw(parser, argument_count);

	for(int i = 0; i < argument_count; ++i) {
		if(is_flag(arguments[i])) {
			printf("Expected %d arguments but got a flag: %s\n", argument_count, arguments[i]);
			show_help();
			exit(1);
			return NULL;
		}
	}

	return arguments;
}

static const char *pop_one_argument(struct Argument_Parser *parser)
{
	const char **arguments = pop_arguments(parser, 1);
	return arguments[0];
}

static const char *pop_flag(struct Argument_Parser *parser)
{
	const char *flag = pop_arguments_raw(parser, 1)[0];

	if(!is_flag(flag)) {
		printf("Expected a flag, but got an invalid argument: %s\n", flag);
		show_help();
		exit(1);
		return NULL;
	}

	return flag;
}

static bool string_matches(const char *a, const char *b)
{
	return 0 == strcmp(a, b);
}

int main(int argc, char **argv)
{
	// 1. Parse arguments
	if(argc < 3) {
		show_help();
		return 1;
	}

	struct T2S_Options options = t2s_get_default_options();

	struct Argument_Parser parser = {
		.top = 1,
		.argc = argc,
		.argv = (const char **)argv
	};

	const char *input_path = pop_one_argument(&parser);;
	const char *output_path = pop_one_argument(&parser);;

	while(parser.top < parser.argc) {
		const char *flag = pop_flag(&parser);

		if(string_matches(flag, "--sdf_range")) {
			options.sdf_range = atof(pop_one_argument(&parser));
		}
		else {
			printf("Unknown flag %s\n", flag);
			show_help();
			return 1;
		}
	}

	// 2. Load data
	int w, h, channels;
    uint8_t *input_data = stbi_load(input_path, &w, &h, &channels, 0);

    if(!input_data) {
    	fprintf(stderr, "Could not load image %s\n", input_path);
    	return 1;
    }

    // 3. Convert to SDF
    struct T2S_Image image = { input_data, w, h, channels };
    const struct T2S_Image sdf = t2s_convert(image, options);

    if(sdf.error) {
    	fprintf(stderr, "Could not convert SDF: %s\n", t2s_get_error_string(sdf.error));
    	return 1;
    }

    // 4. Output data
    int write_result = stbi_write_tga(output_path, sdf.width, sdf.height, sdf.channels, sdf.data);
    if(!write_result) {
    	fprintf(stderr, "Failed to write output image to: %s\n", output_path);
    	return 1;
    }

    // NOTE: We leak all memory, because this is a single-run console app.
	return 0;
}
