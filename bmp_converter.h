#ifndef BMP_CONVERTER_H
#define BMP_CONVERTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// BMP format signatures, header sizes, and pixel constants
#define BMP_MAGIC_SIGNATURE      0x4D42u   // "BM" magic signature in little endian
#define STANDARD_DIB_SIZE        40u       // Standard BITMAPINFOHEADER byte size
#define BMP_V2_INFO_HEADER_SIZE  52u       // BITMAPV2INFOHEADER byte size
#define BMP_V3_INFO_HEADER_SIZE  56u       // BITMAPV3INFOHEADER byte size
#define BMP_V4_HEADER_SIZE       108u      // BITMAPV4HEADER byte size
#define BMP_V5_HEADER_SIZE       124u      // BITMAPV5HEADER byte size
#define BMP_REQUIRED_PLANES      1u        // BMP color planes must always be 1
#define BMP_BI_RGB               0u        // Uncompressed RGB pixel array
#define BMP_BI_BITFIELDS         3u        // Bitfield-masked pixel array
#define TARGET_BIT_COUNT         24u       // 24 bits per pixel RGB888 output
#define RGB888_BYTES_PER_PIXEL   3u        // 3 bytes per pixel (R, G, B)
#define BMP_PIXEL_BYTES_24BPP    3u        // 3 bytes per pixel in 24-bit BMP rows

// Bitwise, alignment, extension, and prompt constants
#define BYTE_MASK                0xFFu     // Mask for isolating lowest 8 bits
#define BITS_PER_BYTE            8u        // Number of bits in one byte
#define STRIDE_ALIGN_BITS        32u       // BMP rows align to 32 bits (4 bytes)
#define STRIDE_ALIGN_BYTES       4u        // 4-byte row alignment boundary
#define ASCII_CASE_OFFSET        32        // Offset between uppercase and lowercase ASCII
#define INITIAL_INPUT_BUF_SIZE   128u      // Initial heap buffer size for dynamic input
#define MAX_DYNAMIC_INPUT_BYTES  (16u * 1024u * 1024u) // 16 MiB cap against runaway input
#define DEFAULT_MAX_RETRIES      3u        // Retry limit for strict prompts
#define MENU_MIN_CHOICE          0         // Minimum valid menu option
#define MENU_MAX_CHOICE          7         // Maximum valid menu option

#define EXT_BMP                  ".bmp"    // Bitmap file extension
#define EXT_RGB                  ".rgb"    // Raw RGB file extension

typedef enum {
        INT_CONSTRAINT_ANY = 0,
        INT_CONSTRAINT_POSITIVE,   // Must be > 0
        INT_CONSTRAINT_NONZERO     // Must be != 0
} int_constraint;

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
int read_strict_int_constrained(const char *prompt, int *out_val, int_constraint constraint, const char *abort_msg);
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
