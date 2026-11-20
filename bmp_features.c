#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "bmp_converter.h"

typedef struct {
        bitmap_file_header file_header;
        bitmap_info_header info_header;
        unsigned char *extra_header;
        size_t extra_bytes;
        unsigned char *pixels;
        size_t pixel_bytes;
        size_t width;
        size_t height;
        size_t row_stride;
} bmp_image_buffer;

// Release all heap buffers owned by a bmp_image_buffer
static void free_bmp_image(bmp_image_buffer *img) {
        if (!img) {
                return;
        }
        free(img->extra_header);
        free(img->pixels);
        memset(img, 0, sizeof(*img));
}

// Populate standard 14-byte file header and 40-byte info header
static void load_bmp_headers(int image_width, int image_height, size_t pixel_array_size, bitmap_file_header *file_header, bitmap_info_header *info_header) {
        size_t header_size = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        size_t total_file_size = header_size + pixel_array_size;

        file_header->bf_type = (uint16_t)BMP_MAGIC_SIGNATURE;
        file_header->bf_size = (uint32_t)total_file_size;
        file_header->bf_reserved_1 = 0;
        file_header->bf_reserved_2 = 0;
        file_header->bf_off_bits = (uint32_t)header_size;

        info_header->bi_size = STANDARD_DIB_SIZE;
        info_header->bi_width = image_width;
        info_header->bi_height = image_height;
        info_header->bi_planes = (uint16_t)BMP_REQUIRED_PLANES;
        info_header->bi_bit_count = (uint16_t)TARGET_BIT_COUNT;
        info_header->bi_compression = BMP_BI_RGB;
        info_header->bi_size_image = (uint32_t)pixel_array_size;
        info_header->bi_x_pels_per_meter = 0;
        info_header->bi_y_pels_per_meter = 0;
        info_header->bi_clr_used = 0;
        info_header->bi_clr_important = 0;
}

// Prompt and validate an existing file path with expected extension
static int prompt_existing_file_path(const char *prompt, const char *expected_ext, const char *abort_msg, char **out_path) {
        char *path = NULL;
        size_t attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;

        while (attempts > 0 && !valid) {
                free(path);
                path = NULL;
                if (!read_strict_string(prompt, &path, attempts)) {
                        break;
                }
                attempts--;
                if (!validate_file_extension(path, expected_ext)) {
                        printf("ERROR : File must have a '%s' extension.\n", expected_ext);
                        continue;
                }
                FILE *test_file = fopen(path, "rb");
                if (!test_file) {
                        perror("ERROR : Failed to open file");
                        continue;
                }
                fclose(test_file);
                valid = 1;
        }

        if (!valid) {
                if (abort_msg) {
                        printf("%s\n", abort_msg);
                }
                free(path);
                *out_path = NULL;
                return 0;
        }

        *out_path = path;
        return 1;
}

// Prompt and validate an output .bmp path, handling overwrite confirmation if needed
static int prompt_output_bmp_path(const char *prompt, const char *abort_msg, char **out_path) {
        char *path = NULL;
        size_t attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;

        while (attempts > 0 && !valid) {
                free(path);
                path = NULL;
                if (!read_strict_string(prompt, &path, attempts)) {
                        break;
                }
                attempts--;
                if (!validate_file_extension(path, EXT_BMP)) {
                        printf("ERROR : Output file must have a '%s' extension.\n", EXT_BMP);
                        continue;
                }
                FILE *existing = fopen(path, "rb");
                if (existing) {
                        fclose(existing);
                        int confirm = get_strict_overwrite_confirmation(path);
                        if (confirm == 1) {
                                valid = 1;
                        } else if (confirm == 0) {
                                printf("INFO : Overwrite declined. Please provide a different path.\n");
                                continue;
                        } else {
                                free(path);
                                *out_path = NULL;
                                return 0;
                        }
                } else {
                        valid = 1;
                }
        }

        if (!valid) {
                if (abort_msg) {
                        printf("%s\n", abort_msg);
                }
                free(path);
                *out_path = NULL;
                return 0;
        }

        *out_path = path;
        return 1;
}

// Read file and info headers and validate supported 24-bit transform format
static int read_and_validate_transform_headers(FILE *file, bitmap_file_header *file_header, bitmap_info_header *info_header) {
        if (fread(file_header, sizeof(bitmap_file_header), 1, file) != 1 ||
            fread(info_header, sizeof(bitmap_info_header), 1, file) != 1) {
                fprintf(stderr, "ERROR : Failed to read BMP headers.\n");
                return 0;
        }

        if (file_header->bf_type != BMP_MAGIC_SIGNATURE ||
            info_header->bi_planes != BMP_REQUIRED_PLANES ||
            info_header->bi_width <= 0 ||
            info_header->bi_height == 0 ||
            info_header->bi_height == INT_MIN) {
                fprintf(stderr, "ERROR : Invalid BMP signature, plane count, or dimensions.\n");
                return 0;
        }

        if (info_header->bi_bit_count != TARGET_BIT_COUNT ||
            info_header->bi_compression != BMP_BI_RGB) {
                fprintf(stderr, "ERROR : Only uncompressed 24-bit (BI_RGB) BMP images are supported for transformations.\n");
                return 0;
        }

        return 1;
}

// Load headers, optional extended DIB/palette bytes, and pixel array from disk
static int load_bmp_image(const char *path, bmp_image_buffer *img) {
        memset(img, 0, sizeof(*img));

        printf("STATUS : Opening input BMP file '%s'...\n", path);
        FILE *input_file = fopen(path, "rb");
        if (!input_file) {
                perror("ERROR : Failed to open input BMP");
                return 0;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        printf("STATUS : Reading and validating BMP headers...\n");
        if (!read_and_validate_transform_headers(input_file, &img->file_header, &img->info_header)) {
                fclose(input_file);
                return 0;
        }
        printf("STATUS : BMP headers read successfully.\n");

        size_t base_header_bytes = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        img->extra_bytes = (size_t)img->file_header.bf_off_bits - base_header_bytes;
        if (img->extra_bytes > 0) {
                printf("STATUS : Reading %zu extra header bytes...\n", img->extra_bytes);
                img->extra_header = malloc(img->extra_bytes);
                if (!img->extra_header) {
                        perror("ERROR : Failed to allocate extra header buffer");
                        fclose(input_file);
                        return 0;
                }
                if (fread(img->extra_header, 1, img->extra_bytes, input_file) != img->extra_bytes) {
                        fprintf(stderr, "ERROR : Failed to read complete extra header bytes.\n");
                        fclose(input_file);
                        free_bmp_image(img);
                        return 0;
                }
                printf("STATUS : Extra header bytes read successfully.\n");
        }

        img->width = (size_t)img->info_header.bi_width;
        img->height = (size_t)(img->info_header.bi_height < 0 ? -img->info_header.bi_height : img->info_header.bi_height);
        img->row_stride = calculate_row_stride(img->width, img->info_header.bi_bit_count);
        if (img->row_stride == 0 || img->height > SIZE_MAX / img->row_stride) {
                fprintf(stderr, "ERROR : BMP pixel buffer dimensions overflow size_t.\n");
                fclose(input_file);
                free_bmp_image(img);
                return 0;
        }
        img->pixel_bytes = img->row_stride * img->height;

        printf("STATUS : Allocating memory for source pixel array (%zu bytes)...\n", img->pixel_bytes);
        img->pixels = malloc(img->pixel_bytes);
        if (!img->pixels) {
                perror("ERROR : Memory allocation failed for source pixels");
                fclose(input_file);
                free_bmp_image(img);
                return 0;
        }
        printf("STATUS : Source pixel memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset (%u bytes)...\n", img->file_header.bf_off_bits);
        if (fseek(input_file, (long)img->file_header.bf_off_bits, SEEK_SET) != 0) {
                perror("ERROR : Failed to seek pixel array offset");
                fclose(input_file);
                free_bmp_image(img);
                return 0;
        }

        if (fread(img->pixels, 1, img->pixel_bytes, input_file) != img->pixel_bytes) {
                fprintf(stderr, "ERROR : Truncated BMP pixel array on disk.\n");
                fclose(input_file);
                free_bmp_image(img);
                return 0;
        }

        fclose(input_file);
        printf("STATUS : Source pixel array read successfully. Input file closed.\n");
        return 1;
}

// Write updated headers, optional extra header bytes, and transformed pixels to disk
static int write_bmp_image(const char *path, const bmp_image_buffer *img, const unsigned char *out_pixels, size_t out_pixel_bytes) {
        if (out_pixel_bytes > (size_t)UINT32_MAX || (size_t)img->file_header.bf_off_bits > (size_t)UINT32_MAX - out_pixel_bytes) {
                fprintf(stderr, "ERROR : Output BMP file size exceeds 4 GiB BMP format limit.\n");
                return 0;
        }

        printf("STATUS : Creating output BMP file '%s'...\n", path);
        FILE *output_file = fopen(path, "wb");
        if (!output_file) {
                perror("ERROR : Failed to create output BMP file");
                return 0;
        }
        printf("STATUS : Output BMP file created successfully.\n");

        printf("STATUS : Writing headers and pixel buffer to disk...\n");
        int write_ok = 1;
        if (fwrite(&img->file_header, sizeof(bitmap_file_header), 1, output_file) != 1 ||
            fwrite(&img->info_header, sizeof(bitmap_info_header), 1, output_file) != 1) {
                write_ok = 0;
        }
        if (write_ok && img->extra_bytes > 0 && img->extra_header != NULL) {
                if (fwrite(img->extra_header, 1, img->extra_bytes, output_file) != img->extra_bytes) {
                        write_ok = 0;
                }
        }
        if (write_ok && out_pixel_bytes > 0) {
                if (fwrite(out_pixels, 1, out_pixel_bytes, output_file) != out_pixel_bytes) {
                        write_ok = 0;
                }
        }

        if (!write_ok) {
                perror("ERROR : Failed while writing BMP data to disk");
                fclose(output_file);
                return 0;
        }

        if (fclose(output_file) != 0) {
                perror("ERROR : Failed to flush and close output BMP file");
                return 0;
        }

        printf("STATUS : Headers and pixel data written and flushed to disk successfully.\n");
        return 1;
}

// Feature 1: Convert raw RGB888 file into 24-bit BMP
void handle_rgb_to_bmp_conversion(void) {
        char *input_path = NULL;
        char *output_path = NULL;
        int image_width = 0;
        int image_height = 0;

        printf("\n ========== Feature 1: Raw RGB888 to 24-bit BMP Conversion ========== \n");

        if (!prompt_existing_file_path("Enter input .rgb file path", EXT_RGB,
                                     "ABORT : Maximum retry attempts reached for input file. Returning to menu.",
                                     &input_path)) {
                return;
        }

        if (!read_strict_int_constrained("Enter image width in pixels", &image_width,
                                         INT_CONSTRAINT_POSITIVE,
                                         "ABORT : Maximum retry attempts reached for image width. Returning to menu.")) {
                free(input_path);
                return;
        }

        if (!read_strict_int_constrained("Enter image height (+ bottom-up, - top-down)", &image_height,
                                         INT_CONSTRAINT_NONZERO,
                                         "ABORT : Maximum retry attempts reached for image height. Returning to menu.")) {
                free(input_path);
                return;
        }

        if (image_height == INT_MIN) {
                fprintf(stderr, "ERROR : Image height out of supported range.\n");
                free(input_path);
                return;
        }

        if (!prompt_output_bmp_path("Enter output .bmp file path",
                                    "ABORT : Maximum retry attempts reached for output file. Returning to menu.",
                                    &output_path)) {
                free(input_path);
                return;
        }

        size_t width_sz = (size_t)image_width;
        size_t abs_height_sz = (size_t)(image_height < 0 ? -image_height : image_height);

        if (width_sz > 0 && abs_height_sz > SIZE_MAX / width_sz) {
                fprintf(stderr, "ERROR : Image dimensions overflow size_t.\n");
                free(input_path);
                free(output_path);
                return;
        }
        size_t pixel_count = width_sz * abs_height_sz;
        if (pixel_count > SIZE_MAX / RGB888_BYTES_PER_PIXEL) {
                fprintf(stderr, "ERROR : Raw RGB888 buffer size overflows size_t.\n");
                free(input_path);
                free(output_path);
                return;
        }

        printf("STATUS : Opening raw input file '%s' in binary read mode...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("ERROR : Failed to open raw input file");
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Raw input file opened successfully.\n");

        printf("STATUS : Verifying input raw file size match...\n");
        if (fseek(input_file, 0, SEEK_END) != 0) {
                perror("ERROR : Failed to seek end of raw input file");
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        long raw_size_signed = ftell(input_file);
        rewind(input_file);
        if (raw_size_signed < 0) {
                perror("ERROR : Failed to determine raw input file size");
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }

        size_t actual_file_size = (size_t)raw_size_signed;
        size_t expected_raw_size = pixel_count * RGB888_BYTES_PER_PIXEL;
        if (actual_file_size != expected_raw_size) {
                fprintf(stderr, "ERROR : Input file size mismatch! Expected %zu bytes, got %zu bytes.\n",
                        expected_raw_size, actual_file_size);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Input file size verified successfully (%zu bytes matches dimensions).\n", actual_file_size);

        size_t row_stride = calculate_row_stride(width_sz, (uint16_t)TARGET_BIT_COUNT);
        if (row_stride == 0 || (abs_height_sz > 0 && row_stride > SIZE_MAX / abs_height_sz)) {
                fprintf(stderr, "ERROR : Row stride or pixel buffer size calculation overflowed.\n");
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        size_t pixel_array_size = row_stride * abs_height_sz;
        size_t header_size = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        if (pixel_array_size > (size_t)UINT32_MAX - header_size) {
                fprintf(stderr, "ERROR : Output BMP size exceeds 4 GiB BMP format limit.\n");
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }

        printf("STATUS : Allocating memory for raw input and stride-aligned pixel buffers...\n");
        unsigned char *raw_input_buffer = malloc(expected_raw_size);
        unsigned char *bmp_pixel_buffer = calloc(1, pixel_array_size);
        if (!raw_input_buffer || !bmp_pixel_buffer) {
                perror("ERROR : Memory allocation failed");
                free(raw_input_buffer);
                free(bmp_pixel_buffer);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Memory allocation completed successfully.\n");

        printf("STATUS : Reading entire raw input stream into memory buffer...\n");
        if (fread(raw_input_buffer, 1, expected_raw_size, input_file) != expected_raw_size) {
                fprintf(stderr, "ERROR : Failed to read complete raw input data.\n");
                free(raw_input_buffer);
                free(bmp_pixel_buffer);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        fclose(input_file);
        printf("STATUS : Raw input stream read finished successfully. Input file closed.\n");

        printf("STATUS : Converting RGB888 pixels to BGR stride-aligned pixel array...\n");
        for (size_t y = 0; y < abs_height_sz; y++) {
                size_t target_row_index = (image_height < 0) ? y : (abs_height_sz - 1u - y);
                const unsigned char *input_row_ptr = raw_input_buffer + (y * width_sz * RGB888_BYTES_PER_PIXEL);
                unsigned char *output_row_ptr = bmp_pixel_buffer + (target_row_index * row_stride);

                for (size_t x = 0; x < width_sz; x++) {
                        size_t src_idx = x * RGB888_BYTES_PER_PIXEL;
                        size_t dst_idx = x * BMP_PIXEL_BYTES_24BPP;
                        output_row_ptr[dst_idx + 0u] = input_row_ptr[src_idx + 2u];
                        output_row_ptr[dst_idx + 1u] = input_row_ptr[src_idx + 1u];
                        output_row_ptr[dst_idx + 2u] = input_row_ptr[src_idx + 0u];
                }
        }
        free(raw_input_buffer);
        printf("STATUS : Pixel conversion and stride alignment completed successfully.\n");

        bmp_image_buffer out_img;
        memset(&out_img, 0, sizeof(out_img));
        load_bmp_headers(image_width, image_height, pixel_array_size,
                         &out_img.file_header, &out_img.info_header);

        int written = write_bmp_image(output_path, &out_img, bmp_pixel_buffer, pixel_array_size);
        free(bmp_pixel_buffer);
        free(input_path);
        free(output_path);

        if (!written) {
                return;
        }
        printf("STATUS : Allocated buffers freed successfully.\n");
        printf("\n^_^ Conversion successfully finished!\n");
}

// Feature 2: Read and print BMP file and info headers
void handle_bmp_header_parsing(void) {
        char *bmp_path = NULL;

        printf("\n ========== Feature 2: Parse and Print BMP Headers ========== \n");

        if (!prompt_existing_file_path("Enter existing .bmp file path", EXT_BMP,
                                     "ABORT : File path validation failed.", &bmp_path)) {
                return;
        }

        printf("STATUS : Opening BMP file '%s' in binary read mode...\n", bmp_path);
        FILE *bmp_file = fopen(bmp_path, "rb");
        if (!bmp_file) {
                perror("ERROR : Failed to open BMP file");
                free(bmp_path);
                return;
        }
        printf("STATUS : BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading 14-byte File Header and 40-byte Info Header...\n");
        if (fread(&file_header, sizeof(bitmap_file_header), 1, bmp_file) != 1 ||
            fread(&info_header, sizeof(bitmap_info_header), 1, bmp_file) != 1) {
                fprintf(stderr, "ERROR : Failed to read BMP headers.\n");
                fclose(bmp_file);
                free(bmp_path);
                return;
        }
        printf("STATUS : Headers read successfully from file stream.\n");

        fclose(bmp_file);
        free(bmp_path);
        printf("STATUS : BMP file closed successfully.\n");

        printf("\n====================================================================\n");
        printf("                 BMP FILE METADATA INSPECTION REPORT                \n");
        printf("====================================================================\n");
        printf("[1] bitmap_file_header (14 Bytes):\n");
        printf("    - bf_type          : 0x%04X ('%c%c') %s\n",
               file_header.bf_type,
               (char)(file_header.bf_type & BYTE_MASK),
               (char)(file_header.bf_type >> BITS_PER_BYTE),
               file_header.bf_type == BMP_MAGIC_SIGNATURE ? "VALID" : "INVALID");
        printf("    - bf_size          : %u bytes\n", file_header.bf_size);
        printf("    - bf_off_bits      : %u bytes\n", file_header.bf_off_bits);

        printf("\n[2] bitmap_info_header (40 Bytes):\n");
        printf("    - bi_width         : %d pixels\n", info_header.bi_width);
        printf("    - bi_height        : %d pixels (%s)\n",
               info_header.bi_height, info_header.bi_height < 0 ? "Top-Down" : "Bottom-Up");
        printf("    - bi_bit_count     : %u BPP\n", info_header.bi_bit_count);
        printf("    - bi_compression   : %u\n", info_header.bi_compression);
        printf("    - bi_size_image    : %u bytes\n", info_header.bi_size_image);
        printf("====================================================================\n");
        printf("STATUS : Header inspection completed successfully.\n");
}

// Feature 3: Validate BMP header fields, offsets, and file size across bit depths
void handle_bmp_validation(void) {
        char *bmp_path = NULL;

        printf("\n ========== Feature 3: BMP File Validator ========== \n");

        if (!prompt_existing_file_path("Enter .bmp file path to validate", EXT_BMP,
                                     "ABORT : File path validation failed.", &bmp_path)) {
                return;
        }

        printf("STATUS : Opening BMP file '%s' for structural validation...\n", bmp_path);
        FILE *bmp_file = fopen(bmp_path, "rb");
        if (!bmp_file) {
                perror("ERROR : Failed to open BMP file");
                free(bmp_path);
                return;
        }
        printf("STATUS : BMP file opened successfully.\n");

        printf("STATUS : Determining total file size on disk...\n");
        if (fseek(bmp_file, 0, SEEK_END) != 0) {
                perror("ERROR : Failed to seek BMP file");
                fclose(bmp_file);
                free(bmp_path);
                return;
        }
        long disk_size_signed = ftell(bmp_file);
        rewind(bmp_file);
        if (disk_size_signed < 0) {
                perror("ERROR : Failed to determine BMP file size");
                fclose(bmp_file);
                free(bmp_path);
                return;
        }
        size_t disk_file_size = (size_t)disk_size_signed;
        printf("STATUS : File size determined successfully (%zu bytes).\n", disk_file_size);

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading headers for structural integrity checks...\n");
        if (fread(&file_header, sizeof(bitmap_file_header), 1, bmp_file) != 1 ||
            fread(&info_header, sizeof(bitmap_info_header), 1, bmp_file) != 1) {
                fprintf(stderr, "ERROR : File is too small to contain valid BMP headers.\n");
                fclose(bmp_file);
                free(bmp_path);
                return;
        }
        fclose(bmp_file);
        free(bmp_path);
        printf("STATUS : Headers read and file stream closed successfully.\n");

        size_t error_count = 0;
        printf("\n====================================================================\n");
        printf("                   BMP STRUCTURAL VALIDATION REPORT                 \n");
        printf("====================================================================\n");

        if (file_header.bf_type == BMP_MAGIC_SIGNATURE) {
                printf("PASS : Magic Signature (bf_type): 0x%04X ('BM')\n", file_header.bf_type);
        } else {
                printf("FAIL : Magic Signature (bf_type): 0x%04X (Expected 0x4D42)\n", file_header.bf_type);
                error_count++;
        }

        if (info_header.bi_size == STANDARD_DIB_SIZE ||
            info_header.bi_size == BMP_V2_INFO_HEADER_SIZE ||
            info_header.bi_size == BMP_V3_INFO_HEADER_SIZE ||
            info_header.bi_size == BMP_V4_HEADER_SIZE ||
            info_header.bi_size == BMP_V5_HEADER_SIZE) {
                printf("PASS : DIB Header Size (bi_size): %u bytes (Valid Header)\n", info_header.bi_size);
        } else {
                printf("FAIL : DIB Header Size (bi_size): %u bytes (Unsupported DIB header)\n", info_header.bi_size);
                error_count++;
        }

        if (info_header.bi_planes == BMP_REQUIRED_PLANES) {
                printf("PASS : Color Planes (bi_planes): %u\n", info_header.bi_planes);
        } else {
                printf("FAIL : Color Planes (bi_planes): %u (Must be %u)\n",
                       info_header.bi_planes, BMP_REQUIRED_PLANES);
                error_count++;
        }

        if (is_supported_bmp_bit_count(info_header.bi_bit_count)) {
                printf("PASS : Color Depth (bi_bit_count): %u BPP\n", info_header.bi_bit_count);
        } else {
                printf("FAIL : Color Depth (bi_bit_count): %u BPP (Supported: 1, 4, 8, 16, 24, 32 BPP)\n",
                       info_header.bi_bit_count);
                error_count++;
        }

        if (info_header.bi_compression == BMP_BI_RGB || info_header.bi_compression == BMP_BI_BITFIELDS) {
                printf("PASS : Compression Scheme: %u (%s)\n",
                       info_header.bi_compression,
                       info_header.bi_compression == BMP_BI_RGB ? "BI_RGB Uncompressed" : "BI_BITFIELDS");
        } else {
                printf("FAIL : Compression Scheme: %u (Unsupported compression)\n", info_header.bi_compression);
                error_count++;
        }

        int width = info_header.bi_width;
        size_t abs_height = (info_header.bi_height == INT_MIN)
                            ? ((size_t)INT_MAX + 1u)
                            : (size_t)(info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height);
        if (width > 0 && info_header.bi_height != 0) {
                printf("PASS : Dimensions Check: Width = %d, Height = %d\n", width, info_header.bi_height);
        } else {
                printf("FAIL : Dimensions Check: Width = %d, Height = %d (Invalid dimensions)\n",
                       width, info_header.bi_height);
                error_count++;
        }

        size_t min_header_offset = sizeof(bitmap_file_header) + (size_t)info_header.bi_size;
        size_t pixel_offset = (size_t)file_header.bf_off_bits;
        if (pixel_offset >= min_header_offset && pixel_offset < disk_file_size) {
                printf("PASS : Pixel Array Offset (bf_off_bits): %u bytes\n", file_header.bf_off_bits);
        } else {
                printf("FAIL : Pixel Array Offset (bf_off_bits): %u bytes (Invalid offset boundary)\n",
                       file_header.bf_off_bits);
                error_count++;
        }

        if (width > 0 && abs_height > 0 && is_supported_bmp_bit_count(info_header.bi_bit_count)) {
                size_t row_stride = calculate_row_stride((size_t)width, info_header.bi_bit_count);
                if (row_stride == 0 || abs_height > SIZE_MAX / row_stride) {
                        printf("FAIL : File Size Consistency: Image dimensions exceed addressable size\n");
                        error_count++;
                } else {
                        size_t expected_pixel_array_size = row_stride * abs_height;
                        if (pixel_offset > SIZE_MAX - expected_pixel_array_size ||
                            disk_file_size < pixel_offset + expected_pixel_array_size) {
                                printf("FAIL : File Size Consistency: Truncated file! Expected at least %zu bytes, found %zu bytes\n",
                                       pixel_offset + expected_pixel_array_size, disk_file_size);
                                error_count++;
                        } else {
                                printf("PASS : File Size Consistency: Disk size (%zu bytes) matches/exceeds structural requirements\n",
                                       disk_file_size);
                        }
                }
        }

        printf("--------------------------------------------------------------------\n");
        if (error_count == 0) {
                printf("RESULT : SUCCESS - BMP file is fully valid and safe to process!\n");
        } else {
                printf("RESULT : FAILURE - BMP file contains %zu critical structural error(s).\n", error_count);
        }
        printf("====================================================================\n");
        printf("STATUS : Validation routine completed successfully.\n");
}

// Feature 4: Flip BMP image vertically
void handle_vertical_flip(void) {
        char *input_path = NULL;
        char *output_path = NULL;

        printf("\n ========== Feature 4: Vertical Flip Transformation ========== \n");

        if (!prompt_existing_file_path("Enter input .bmp file path", EXT_BMP, NULL, &input_path)) {
                return;
        }
        if (!prompt_output_bmp_path("Enter output .bmp file path", NULL, &output_path)) {
                free(input_path);
                return;
        }

        bmp_image_buffer img;
        if (!load_bmp_image(input_path, &img)) {
                free(input_path);
                free(output_path);
                return;
        }

        printf("STATUS : Allocating memory for flipped destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, img.pixel_bytes);
        if (!dst_pixels) {
                perror("ERROR : Memory allocation failed");
                free_bmp_image(&img);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Destination pixel memory allocated successfully.\n");

        printf("STATUS : Executing vertical flip pixel row re-ordering...\n");
        for (size_t y = 0; y < img.height; y++) {
                const unsigned char *src_row_ptr = img.pixels + (y * img.row_stride);
                unsigned char *dst_row_ptr = dst_pixels + ((img.height - 1u - y) * img.row_stride);
                memcpy(dst_row_ptr, src_row_ptr, img.width * BMP_PIXEL_BYTES_24BPP);
        }
        printf("STATUS : Vertical flip row re-ordering completed successfully.\n");

        img.info_header.bi_height = (int32_t)img.height;

        int written = write_bmp_image(output_path, &img, dst_pixels, img.pixel_bytes);
        free(dst_pixels);
        free_bmp_image(&img);
        free(input_path);
        free(output_path);

        if (!written) {
                return;
        }
        printf("STATUS : All buffers freed successfully.\n");
        printf("\n^_^ Vertical flip completed successfully!\n");
}

// Feature 5: Mirror BMP image horizontally
void handle_mirror_image(void) {
        char *input_path = NULL;
        char *output_path = NULL;

        printf("\n ========== Feature 5: Mirror / Horizontal Flip Transformation ========== \n");

        if (!prompt_existing_file_path("Enter input .bmp file path", EXT_BMP, NULL, &input_path)) {
                return;
        }
        if (!prompt_output_bmp_path("Enter output .bmp file path", NULL, &output_path)) {
                free(input_path);
                return;
        }

        bmp_image_buffer img;
        if (!load_bmp_image(input_path, &img)) {
                free(input_path);
                free(output_path);
                return;
        }

        printf("STATUS : Allocating memory for mirrored destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, img.pixel_bytes);
        if (!dst_pixels) {
                perror("ERROR : Memory allocation failed");
                free_bmp_image(&img);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Destination pixel memory allocated successfully.\n");

        printf("STATUS : Executing mirror (horizontal flip) transformation...\n");
        for (size_t y = 0; y < img.height; y++) {
                const unsigned char *src_row_ptr = img.pixels + (y * img.row_stride);
                unsigned char *dst_row_ptr = dst_pixels + (y * img.row_stride);
                for (size_t x = 0; x < img.width; x++) {
                        size_t src_p = x * BMP_PIXEL_BYTES_24BPP;
                        size_t dst_p = (img.width - 1u - x) * BMP_PIXEL_BYTES_24BPP;
                        dst_row_ptr[dst_p + 0u] = src_row_ptr[src_p + 0u];
                        dst_row_ptr[dst_p + 1u] = src_row_ptr[src_p + 1u];
                        dst_row_ptr[dst_p + 2u] = src_row_ptr[src_p + 2u];
                }
        }
        printf("STATUS : Mirror pixel transformation completed successfully.\n");

        int written = write_bmp_image(output_path, &img, dst_pixels, img.pixel_bytes);
        free(dst_pixels);
        free_bmp_image(&img);
        free(input_path);
        free(output_path);

        if (!written) {
                return;
        }
        printf("STATUS : All buffers freed successfully.\n");
        printf("\n^_^ Mirror completed successfully!\n");
}

// Feature 6: Rotate BMP image by 90, 180, or 270 degrees
void handle_image_rotation(void) {
        char *input_path = NULL;
        char *output_path = NULL;
        int angle = 0;
        char *direction_choice = NULL;
        char dir_flag = 'c';

        printf("\n ========== Feature 6: Image Rotation (Clockwise / Anticlockwise) ========== \n");

        if (!prompt_existing_file_path("Enter input .bmp file path", EXT_BMP, NULL, &input_path)) {
                return;
        }

        bmp_image_buffer img;
        if (!load_bmp_image(input_path, &img)) {
                free(input_path);
                return;
        }

        printf("INFO : Source Image Details -> Width: %zu pixels, Height: %zu pixels\n",
               img.width, img.height);

        size_t attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter rotation angle (90, 180, or 270)", &angle,
                                                INT_CONSTRAINT_POSITIVE, NULL)) {
                        free_bmp_image(&img);
                        free(input_path);
                        return;
                }
                attempts--;
                if (angle != 90 && angle != 180 && angle != 270) {
                        printf("ERROR : Invalid angle. Rotation must be strictly 90, 180, or 270 degrees.\n");
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                free(direction_choice);
                direction_choice = NULL;
                if (!read_strict_string("Enter direction - Clockwise (c) or Anticlockwise (a)",
                                       &direction_choice, attempts)) {
                        free_bmp_image(&img);
                        free(input_path);
                        return;
                }
                attempts--;
                char d = direction_choice[0];
                if ((d == 'c' || d == 'C' || d == 'a' || d == 'A') && direction_choice[1] == '\0') {
                        dir_flag = (d == 'C' || d == 'c') ? 'c' : 'a';
                        valid = 1;
                } else {
                        printf("ERROR : Invalid direction. Please enter strictly 'c' or 'a'.\n");
                }
        }
        free(direction_choice);
        if (!valid) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        if (!prompt_output_bmp_path("Enter output .bmp file path", NULL, &output_path)) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        size_t dst_width = (angle == 180) ? img.width : img.height;
        size_t dst_height = (angle == 180) ? img.height : img.width;
        size_t dst_row_stride = calculate_row_stride(dst_width, img.info_header.bi_bit_count);
        if (dst_row_stride == 0 || dst_height > SIZE_MAX / dst_row_stride) {
                fprintf(stderr, "ERROR : Rotated destination buffer size overflows size_t.\n");
                free_bmp_image(&img);
                free(input_path);
                free(output_path);
                return;
        }
        size_t dst_pixel_array_size = dst_row_stride * dst_height;

        printf("STATUS : Allocating memory for rotated destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, dst_pixel_array_size);
        if (!dst_pixels) {
                perror("ERROR : Destination memory allocation failed");
                free_bmp_image(&img);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Destination memory allocated successfully.\n");

        printf("STATUS : Executing %d-degree rotation (%s)...\n",
               angle, (dir_flag == 'c') ? "Clockwise" : "Anticlockwise");
        for (size_t y = 0; y < img.height; y++) {
                const unsigned char *src_row_ptr = img.pixels + (y * img.row_stride);
                for (size_t x = 0; x < img.width; x++) {
                        const unsigned char *src_pixel = src_row_ptr + (x * BMP_PIXEL_BYTES_24BPP);
                        size_t dst_x = x;
                        size_t dst_y = y;

                        if (angle == 90) {
                                if (dir_flag == 'a') {
                                        dst_x = img.height - 1u - y;
                                        dst_y = x;
                                } else {
                                        dst_x = y;
                                        dst_y = img.width - 1u - x;
                                }
                        } else if (angle == 180) {
                                dst_x = img.width - 1u - x;
                                dst_y = img.height - 1u - y;
                        } else if (angle == 270) {
                                if (dir_flag == 'a') {
                                        dst_x = y;
                                        dst_y = img.width - 1u - x;
                                } else {
                                        dst_x = img.height - 1u - y;
                                        dst_y = x;
                                }
                        }

                        unsigned char *dst_pixel = dst_pixels + (dst_y * dst_row_stride) + (dst_x * BMP_PIXEL_BYTES_24BPP);
                        dst_pixel[0u] = src_pixel[0u];
                        dst_pixel[1u] = src_pixel[1u];
                        dst_pixel[2u] = src_pixel[2u];
                }
        }
        printf("STATUS : Rotation matrix mapping completed successfully.\n");

        img.info_header.bi_width = (int32_t)dst_width;
        img.info_header.bi_height = (int32_t)dst_height;
        img.info_header.bi_size_image = (uint32_t)dst_pixel_array_size;
        img.file_header.bf_size = (uint32_t)((size_t)img.file_header.bf_off_bits + dst_pixel_array_size);

        int written = write_bmp_image(output_path, &img, dst_pixels, dst_pixel_array_size);
        free(dst_pixels);
        free_bmp_image(&img);
        free(input_path);
        free(output_path);

        if (!written) {
                return;
        }
        printf("STATUS : All buffers freed successfully.\n");
        printf("\n^_^ Rotation completed successfully!\n");
}

// Feature 7: Crop rectangular sub-region from BMP image
void handle_image_cropping(void) {
        char *input_path = NULL;
        char *output_path = NULL;
        int start_x = 0;
        int start_y = 0;
        int crop_width = 0;
        int crop_height = 0;

        printf("\n ========== Feature 7: Bounding-Box Cropping ========== \n");

        if (!prompt_existing_file_path("Enter input .bmp file path", EXT_BMP, NULL, &input_path)) {
                return;
        }

        bmp_image_buffer img;
        if (!load_bmp_image(input_path, &img)) {
                free(input_path);
                return;
        }

        int src_width = (int)img.width;
        int src_height = (int)img.height;

        printf("\nINFO : Source Image Loaded Successfully:\n");
        printf("INFO :   - Total Width  : %d pixels (Valid X range: 0 to %d)\n", src_width, src_width - 1);
        printf("INFO :   - Total Height : %d pixels (Valid Y range: 0 to %d)\n\n", src_height, src_height - 1);

        size_t attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter starting X coordinate", &start_x,
                                                INT_CONSTRAINT_ANY, NULL)) {
                        free_bmp_image(&img);
                        free(input_path);
                        return;
                }
                attempts--;
                if (start_x < 0 || start_x >= src_width) {
                        printf("ERROR : Start X (%d) is out of bounds [0, %d].\n", start_x, src_width - 1);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter starting Y coordinate", &start_y,
                                                INT_CONSTRAINT_ANY, NULL)) {
                        free_bmp_image(&img);
                        free(input_path);
                        return;
                }
                attempts--;
                if (start_y < 0 || start_y >= src_height) {
                        printf("ERROR : Start Y (%d) is out of bounds [0, %d].\n", start_y, src_height - 1);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter crop width", &crop_width,
                                                INT_CONSTRAINT_POSITIVE, NULL)) {
                        free_bmp_image(&img);
                        free(input_path);
                        return;
                }
                attempts--;
                if (crop_width > src_width - start_x) {
                        printf("ERROR : Invalid width. Must be > 0 and start_x + width <= %d (Max available: %d).\n",
                               src_width, src_width - start_x);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter crop height", &crop_height,
                                                INT_CONSTRAINT_POSITIVE, NULL)) {
                        free_bmp_image(&img);
                        free(input_path);
                        return;
                }
                attempts--;
                if (crop_height > src_height - start_y) {
                        printf("ERROR : Invalid height. Must be > 0 and start_y + height <= %d (Max available: %d).\n",
                               src_height, src_height - start_y);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        if (!prompt_output_bmp_path("Enter output .bmp file path", NULL, &output_path)) {
                free_bmp_image(&img);
                free(input_path);
                return;
        }

        size_t crop_w_sz = (size_t)crop_width;
        size_t crop_h_sz = (size_t)crop_height;
        size_t dst_row_stride = calculate_row_stride(crop_w_sz, img.info_header.bi_bit_count);
        if (dst_row_stride == 0 || crop_h_sz > SIZE_MAX / dst_row_stride) {
                fprintf(stderr, "ERROR : Cropped destination buffer size overflows size_t.\n");
                free_bmp_image(&img);
                free(input_path);
                free(output_path);
                return;
        }
        size_t dst_pixel_array_size = dst_row_stride * crop_h_sz;

        printf("STATUS : Allocating memory for cropped destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, dst_pixel_array_size);
        if (!dst_pixels) {
                perror("ERROR : Destination memory allocation failed");
                free_bmp_image(&img);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Destination memory allocated successfully.\n");

        printf("STATUS : Extracting bounding-box region pixels (%d,%d to %d,%d)...\n",
               start_x, start_y, start_x + crop_width, start_y + crop_height);
        for (size_t y = 0; y < crop_h_sz; y++) {
                size_t src_y = (size_t)start_y + y;
                const unsigned char *src_row_ptr = img.pixels + (src_y * img.row_stride) + ((size_t)start_x * BMP_PIXEL_BYTES_24BPP);
                unsigned char *dst_row_ptr = dst_pixels + (y * dst_row_stride);
                memcpy(dst_row_ptr, src_row_ptr, crop_w_sz * BMP_PIXEL_BYTES_24BPP);
        }
        printf("STATUS : Bounding-box pixel extraction completed successfully.\n");

        img.info_header.bi_width = crop_width;
        img.info_header.bi_height = crop_height;
        img.info_header.bi_size_image = (uint32_t)dst_pixel_array_size;
        img.file_header.bf_size = (uint32_t)((size_t)img.file_header.bf_off_bits + dst_pixel_array_size);

        int written = write_bmp_image(output_path, &img, dst_pixels, dst_pixel_array_size);
        free(dst_pixels);
        free_bmp_image(&img);
        free(input_path);
        free(output_path);

        if (!written) {
                return;
        }
        printf("STATUS : All buffers freed successfully.\n");
        printf("\n^_^ Cropping completed successfully!\n");
}
