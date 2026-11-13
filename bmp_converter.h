#ifndef BMP_CONVERTER_H
#define BMP_CONVERTER_H

#include <stdint.h>
#include <stdio.h>

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
	uint32_t bi_width;
	int32_t  bi_height;
	uint16_t bi_planes;
	uint16_t bi_bit_count;
	uint32_t bi_compression;
	uint32_t bi_size_image;
	uint32_t bi_x_pels_per_meter;
	uint32_t bi_y_pels_per_meter;
	uint32_t bi_clr_used;
	uint32_t bi_clr_important;
} bitmap_info_header;
#pragma pack(pop)

// Function prototypes for modular separation
int calculate_row_stride(int image_width);
void load_bmp_headers(int image_width, int image_height, uint32_t pixel_array_size, bitmap_file_header *file_header, bitmap_info_header *info_header);
int convert_rgb888_to_bmp_buffer(const unsigned char *raw_input_buffer, int image_width, int image_height, int row_stride, unsigned char *bmp_pixel_buffer);

#endif 
