#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
	uint16_t bf_type;
	uint32_t bf_size;
	uint16_t bf_reserved_1;
	uint16_t bf_reserved_2;
	uint32_t bf_off_bits;
} bitmap_file_header;

typedef struct {
	uint32_t bi_size;
	int32_t  bi_width;
	int32_t  bi_height;
	uint16_t bi_planes;
	uint16_t bi_bit_count;
	uint32_t bi_compression;
	uint32_t bi_size_image;
	int32_t  bi_x_pels_per_meter;
	int32_t  bi_y_pels_per_meter;
	uint32_t bi_clr_used;
	uint32_t bi_clr_important;
} bitmap_info_header;
#pragma pack(pop)

int main(int argc, char *argv[]) {
	if (argc != 5) {
		fprintf(stderr, "Usage: %s <input.rgb> <width> <height> <output.bmp>\n", argv[0]);
		return 1;
	}

	char *input_path = argv[1];
	int image_width = atoi(argv[2]);
	int image_height = atoi(argv[3]);
	char *output_path = argv[4];

	printf("=> Target Image: %dx%d\n", image_width, image_height);
	printf("=> Input File:   %s\n", input_path);
	printf("=> Output File:  %s\n", output_path);

	FILE *input_file = fopen(input_path, "rb");
	if (!input_file) {
		perror("Failed to open raw input file");
		return 1;
	}

	FILE *output_file = fopen(output_path, "wb");
	if (!output_file) {
		perror("Failed to create BMP output file");
		fclose(input_file);
		return 1;
	}

	printf("[Status] Step 1 complete: Headers and file descriptors initialized successfully.\n");

	fclose(input_file);
	fclose(output_file);
	return 0;
}
