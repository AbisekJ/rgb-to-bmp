#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

	
	uint32_t row_stride = ((image_width * 24 + 31) / 32) * 4;
	uint32_t pixel_array_size = row_stride * image_height;
	uint32_t header_size = sizeof(bitmap_file_header) + sizeof(bitmap_info_header); // 54 bytes
	uint32_t total_file_size = header_size + pixel_array_size;

	printf("=> Calculated Row Stride: %u bytes\n", row_stride);
	printf("=> Total File Size:       %u bytes\n", total_file_size);

	bitmap_file_header file_header;
	file_header.bf_type = 0x4D42; // "BM"
	file_header.bf_size = total_file_size;
	file_header.bf_reserved_1 = 0;
	file_header.bf_reserved_2 = 0;
	file_header.bf_off_bits = header_size;

	bitmap_info_header info_header;
	info_header.bi_size = sizeof(bitmap_info_header);
	info_header.bi_width = image_width;
	info_header.bi_height = image_height; // Positive = bottom-up
	info_header.bi_planes = 1;
	info_header.bi_bit_count = 24;
	info_header.bi_compression = 0;
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


	size_t raw_input_size = (size_t)image_width * image_height * 3;
	unsigned char *raw_input_buffer = malloc(raw_input_size);
	if (!raw_input_buffer) {
		perror("Failed to allocate memory for raw input buffer");
		fclose(input_file);
		fclose(output_file);
		return 1;
	}

	size_t bytes_read = fread(raw_input_buffer, 1, raw_input_size, input_file);
	if (bytes_read != raw_input_size) {
		fprintf(stderr, "Warning: Expected %zu bytes from raw file, but read %zu bytes.\n", raw_input_size, bytes_read);
	}

	// Transform RGB to BGR, Pad Rows, and Handle Bottom-Up Order 
	// Allocate buffer for the final BMP pixel array (includes stride padding)
	unsigned char *bmp_pixel_buffer = calloc(1, pixel_array_size); 
	if (!bmp_pixel_buffer) {
		perror("Failed to allocate memory for BMP pixel buffer");
		free(raw_input_buffer);
		fclose(input_file);
		fclose(output_file);
		return 1;
	}

	for (int y = 0; y < image_height; y++) {
		// Map top-down input row to bottom-up BMP output row
		int target_row_index = image_height - 1 - y;

		unsigned char *input_row_ptr = raw_input_buffer + (y * image_width * 3);
		unsigned char *output_row_ptr = bmp_pixel_buffer + (target_row_index * row_stride);

		for (int x = 0; x < image_width; x++) {
			unsigned char red_channel   = input_row_ptr[x * 3 + 0];
			unsigned char green_channel = input_row_ptr[x * 3 + 1];
			unsigned char blue_channel  = input_row_ptr[x * 3 + 2];

			// Write in BGR order into the padded output row buffer
			output_row_ptr[x * 3 + 0] = blue_channel;
			output_row_ptr[x * 3 + 1] = green_channel;
			output_row_ptr[x * 3 + 2] = red_channel;
		}
		// Any remaining bytes in output_row_ptr up to row_stride are automatically zero-initialized by calloc().
	}

	printf("^_^ Step 4 complete: Pixels transformed (RGB->BGR), padded, and reordered.\n");
	
	// Clean up
	free(raw_input_buffer);
	free(bmp_pixel_buffer);
	fclose(input_file);
	fclose(output_file);
	return 0;
}
