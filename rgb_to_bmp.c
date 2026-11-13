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
	uint32_t  bi_width;
	int32_t  bi_height;
	uint16_t bi_planes;
	uint16_t bi_bit_count;
	uint32_t bi_compression;
	uint32_t bi_size_image;
	uint32_t  bi_x_pels_per_meter;
	uint32_t  bi_y_pels_per_meter;
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

	uint32_t row_stride = ((image_width * 24 + 31) / 32) * 4;
	uint32_t pixel_array_size = row_stride * image_height;
	uint32_t header_size = sizeof(bitmap_file_header) + sizeof(bitmap_info_header); // 14 + 40 = 54 bytes
	uint32_t total_file_size = header_size + pixel_array_size;

	printf("=> Calculated Row Stride: %d bytes\n", row_stride);
	printf("=> Total File Size:       %u bytes\n", total_file_size);

	// Initialize File Header
	bitmap_file_header file_header;
	file_header.bf_type = 0x4D42;           // "BM" signature
	file_header.bf_size = total_file_size;
	file_header.bf_reserved_1 = 0;
	file_header.bf_reserved_2 = 0;
	file_header.bf_off_bits = header_size; // Pixel data starts right after headers (byte 54)

	// Initialize DIB Info Header
	bitmap_info_header info_header;
	info_header.bi_size = sizeof(bitmap_info_header); // 40 bytes
	info_header.bi_width = image_width;
	info_header.bi_height = image_height; // Positive = bottom-up orientation
	info_header.bi_planes = 1;
	info_header.bi_bit_count = 24; 
	info_header.bi_compression = 0; // uncompressed
	info_header.bi_size_image = pixel_array_size;
	info_header.bi_x_pels_per_meter = 0;
	info_header.bi_y_pels_per_meter = 0;
	info_header.bi_clr_used = 0;
	info_header.bi_clr_important = 0;
	

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

	// Write headers to disk
	fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
	fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);

	printf("^_^ : Step 2 complete: Headers calculated and written successfully.\n");

	fclose(input_file);
	fclose(output_file);
	return 0;
}
