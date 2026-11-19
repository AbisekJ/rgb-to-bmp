#ifndef BMP_CONVERTER_H
#define BMP_CONVERTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define BMP_MAGIC_SIGNATURE   0x4D42  // "BM" magic signature in little endian
#define STANDARD_DIB_SIZE     40      // Standard Windows BITMAPINFOHEADER byte size
#define TARGET_BIT_COUNT      24      // 24 bits per pixel RGB output
#define DEFAULT_MAX_RETRIES   3       // Retry limit for strict prompts

// Packed BMP header structures matching file layout
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

// Helper and input prototypes
int validate_file_extension(const char *filename, const char *expected_ext);
size_t calculate_row_stride(size_t image_width, uint16_t bit_count);
int is_supported_bmp_bit_count(uint16_t bit_count);
char *read_dynamic_input(const char *prompt_message);
int read_strict_string(const char *prompt, char **out_str, size_t attempts_left);
int read_strict_int(const char *prompt, int *out_val, size_t attempts_left);
int get_strict_overwrite_confirmation(const char *filepath);

// CLI and menu prototypes
void print_banner(void);
void print_help(const char *program_name);
void print_version(void);

// Image processing feature prototypes
void handle_rgb_to_bmp_conversion(void);
void handle_bmp_header_parsing(void);
void handle_bmp_validation(void);
void handle_vertical_flip(void);
void handle_mirror_image(void);
void handle_image_rotation(void);
void handle_image_cropping(void);

#endif
