#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bmp_converter.h"




void print_banner(void) {
	printf("====================================================================\n");
	printf("  RGB888 to 24-bit Uncompressed BMP Converter (v1)\n");
	printf("====================================================================\n");
}

void print_help(const char *program_name) {
	printf("Usage:\n");
	printf("  %s <input.rgb> <width> <height> <output.bmp>\n", program_name);
	printf("  %s --help\n", program_name);
	printf("  %s --version\n\n", program_name);
	printf("Description:\n");
	printf("  Converts a raw, uncompressed RGB888 byte stream into a standard\n");
	printf("  24-bit Device-Independent Bitmap (BMP). Automatically handles\n");
	printf("  RGB-to-BGR channel transformation, 4-byte row stride padding, and\n");
	printf("  bottom-up row orientation mapping.\n");
}

void print_version(void) {
	printf("rgb_to_bmp version 1\n");
}

// Stride Calculation Logic ---
int calculate_row_stride(int image_width) {
	return ((image_width * 24 + 31) / 32) * 4;
}

// Loading header values
void load_bmp_headers(int image_width, int image_height, uint32_t pixel_array_size, bitmap_file_header *file_header, bitmap_info_header *info_header) {

	uint32_t header_size = sizeof(bitmap_file_header) + sizeof(bitmap_info_header); // 54 bytes
	uint32_t total_file_size = header_size + pixel_array_size;

	file_header->bf_type = 0x4D42; // "BM"
	file_header->bf_size = total_file_size;
	file_header->bf_reserved_1 = 0;
	file_header->bf_reserved_2 = 0;
	file_header->bf_off_bits = header_size;
	
	info_header->bi_size = sizeof(bitmap_info_header);
	info_header->bi_width = image_width;
	info_header->bi_height = image_height; // Positive = bottom-up
	info_header->bi_planes = 1;
	info_header->bi_bit_count = 24;
	info_header->bi_compression = 0; // uncompressed
	info_header->bi_size_image = pixel_array_size;
	info_header->bi_x_pels_per_meter = 0;
	info_header->bi_y_pels_per_meter = 0;
	info_header->bi_clr_used = 0;
	info_header->bi_clr_important = 0;
}

// Pixel Transformation & Stride Padding Logic 
int convert_rgb888_to_bmp_buffer(const unsigned char *raw_input_buffer, int image_width, int image_height, int row_stride, unsigned char *bmp_pixel_buffer) {
	for (int y = 0; y < image_height; y++) {
		int target_row_index = image_height - 1 - y;

		const unsigned char *input_row_ptr = raw_input_buffer + (y * image_width * 3);
		unsigned char *output_row_ptr = bmp_pixel_buffer + (target_row_index * row_stride);

		for (int x = 0; x < image_width; x++) {
			unsigned char red_channel   = input_row_ptr[x * 3 + 0];
			unsigned char green_channel = input_row_ptr[x * 3 + 1];
			unsigned char blue_channel  = input_row_ptr[x * 3 + 2];

			
			output_row_ptr[x * 3 + 0] = blue_channel;
			output_row_ptr[x * 3 + 1] = green_channel;
			output_row_ptr[x * 3 + 2] = red_channel;
		}
	}
	return 0;
}


int main(int argc, char *argv[]) {
        // Check for help or version flags before strict argument count validation
	if (argc == 2) {
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
			print_banner();
			print_help(argv[0]);
			return 0;
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
			print_banner();
			print_version();
			return 0;
		}
	}

	if (argc != 5) {
		fprintf(stderr, "Error: Invalid argument count.\n");
		fprintf(stderr, "Try '%s --help' for usage instructions.\n", argv[0]);
		return 1;
	}

	print_banner();
	
	char *input_path = argv[1];
	int image_width = atoi(argv[2]);
	int image_height = atoi(argv[3]);
	char *output_path = argv[4];

	// input validation
	if (image_width <= 0 || image_height <= 0) {
		fprintf(stderr, "Error: Image width (%d) and height (%d) must be strictly positive integers.\n", 
			image_width, image_height);
		return 1;
	}

	printf("=> Target Image: %dx%d\n", image_width, image_height);

	FILE *input_file = fopen(input_path, "rb");
	if (!input_file) {
		perror("Failed to open raw input file");
		return 1;
	}
	
	
	if (fseek(input_file, 0, SEEK_END) != 0) {
		perror("Failed to seek input file");
		fclose(input_file);
		return 1;
	}
 
	long actual_file_size = ftell(input_file);
	if (actual_file_size < 0) {
		perror("Failed to get input file size");
		fclose(input_file);
		return 1;
	}
	rewind(input_file);


	size_t expected_raw_size = (size_t)image_width * image_height * 3;
	if ((size_t)actual_file_size != expected_raw_size) {
		fprintf(stderr, "Error: Input file size mismatch!\n");
		fprintf(stderr, "       Expected: %zu bytes (%dx%d pixels * 3 channels)\n", 
			expected_raw_size, image_width, image_height);
		fprintf(stderr, "       Actual:   %ld bytes on disk\n", actual_file_size);
		fclose(input_file);
		return 1;
	}

	
	uint32_t row_stride = calculate_row_stride(image_width);
	uint32_t pixel_array_size = row_stride * image_height;

	printf("=> Calculated Row Stride: %d bytes\n", row_stride);

	bitmap_file_header file_header;
	bitmap_info_header info_header;
	
	load_bmp_headers(image_width, image_height, pixel_array_size, &file_header, &info_header);

	FILE *output_file = fopen(output_path, "wb");
	if (!output_file) {
		perror("Failed to create BMP output file");
		fclose(input_file);
		return 1;
	}

	// Write headers to disk
	fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
	fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);

	// Allocate memory and read raw input file
	unsigned char *raw_input_buffer = malloc(expected_raw_size);
	if (!raw_input_buffer) {
		perror("Failed to allocate memory for raw input buffer");
		fclose(input_file);
		fclose(output_file);
		return 1;
	}

	size_t bytes_read = fread(raw_input_buffer, 1, expected_raw_size, input_file);
	if (bytes_read != expected_raw_size) {
		fprintf(stderr, "Error: Read %zu bytes, but expected %zu bytes.\n", 
			bytes_read, expected_raw_size);
		free(raw_input_buffer);
		fclose(input_file);
		fclose(output_file);
		return 1;
	}

	// Allocate BMP pixel buffer
	unsigned char *bmp_pixel_buffer = calloc(1, pixel_array_size); 
	if (!bmp_pixel_buffer) {
		perror("Failed to allocate memory for BMP pixel buffer");
		free(raw_input_buffer);
		fclose(input_file);
		fclose(output_file);
		return 1;
	}

	convert_rgb888_to_bmp_buffer(raw_input_buffer, image_width, image_height, row_stride, bmp_pixel_buffer);

	// Write Pixel Data to Disk
	size_t pixels_written = fwrite(bmp_pixel_buffer, 1, pixel_array_size, output_file);
	if (pixels_written != pixel_array_size) {
		fprintf(stderr, "Error: Wrote %zu bytes, but expected %u bytes.\n", pixels_written, pixel_array_size);
	} else {
		printf("^_^ : Modular conversion complete: Successfully written to %s (%u bytes).\n", output_path, pixel_array_size);
	}

	// Clean up resources
	free(raw_input_buffer);
	free(bmp_pixel_buffer);
	fclose(input_file);
	fclose(output_file);

	return 0;
}
