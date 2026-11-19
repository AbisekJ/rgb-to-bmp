#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "bmp_converter.h"

// Populate standard 14-byte file header and 40-byte info header
static void load_bmp_headers(int image_width, int image_height, size_t pixel_array_size,
                             bitmap_file_header *file_header, bitmap_info_header *info_header) {
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
static int prompt_existing_file_path(const char *prompt, const char *expected_ext,
                                    const char *abort_msg, char **out_path) {
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

// Read file and info headers and validate supported BMP format parameters
static int read_and_validate_bmp_headers(FILE *file, bitmap_file_header *file_header,
                                          bitmap_info_header *info_header) {
        if (fread(file_header, sizeof(bitmap_file_header), 1, file) != 1 ||
            fread(info_header, sizeof(bitmap_info_header), 1, file) != 1) {
                fprintf(stderr, "ERROR : Failed to read BMP headers.\n");
                return 0;
        }

        if (file_header->bf_type != BMP_MAGIC_SIGNATURE ||
            !is_supported_bmp_bit_count(info_header->bi_bit_count) ||
            (info_header->bi_compression != BMP_BI_RGB && info_header->bi_compression != BMP_BI_BITFIELDS)) {
                fprintf(stderr, "ERROR : Unsupported or invalid BMP file.\n");
                return 0;
        }

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

        if (!prompt_output_bmp_path("Enter output .bmp file path",
                                    "ABORT : Maximum retry attempts reached for output file. Returning to menu.",
                                    &output_path)) {
                free(input_path);
                return;
        }

        printf("STATUS : Opening raw input file '%s' in binary read mode...\n", input_path);
        size_t width_sz = (size_t)image_width;
        size_t abs_height_sz = (image_height == INT_MIN)
                               ? ((size_t)INT_MAX + 1u)
                               : (size_t)(image_height < 0 ? -image_height : image_height);

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

        bitmap_file_header file_header;
        bitmap_info_header info_header;
        load_bmp_headers(image_width, image_height, pixel_array_size, &file_header, &info_header);

        printf("STATUS : Creating BMP output file '%s' in binary write mode...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("ERROR : Failed to create BMP output file");
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : BMP output file created successfully.\n");

        printf("STATUS : Writing 54-byte headers (File Header + Info Header)...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        printf("STATUS : Headers written successfully.\n");

        printf("STATUS : Allocating memory for raw input and stride-aligned pixel buffers...\n");
        unsigned char *raw_input_buffer = malloc(expected_raw_size);
        unsigned char *bmp_pixel_buffer = calloc(1, pixel_array_size);
        if (!raw_input_buffer || !bmp_pixel_buffer) {
                perror("ERROR : Memory allocation failed");
                free(raw_input_buffer);
                free(bmp_pixel_buffer);
                fclose(input_file);
                fclose(output_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Memory allocation completed successfully.\n");

        printf("STATUS : Reading entire raw input stream into memory buffer...\n");
        if (fread(raw_input_buffer, 1, expected_raw_size, input_file) != expected_raw_size) {
                fprintf(stderr, "ERROR : Failed to read complete raw input data.\n");
        }
        printf("STATUS : Raw input stream read finished successfully.\n");

        printf("STATUS : Converting RGB888 pixels to BGR stride-aligned pixel array...\n");
        for (size_t y = 0; y < abs_height_sz; y++) {
                size_t target_row_index = (image_height < 0) ? y : (abs_height_sz - 1u - y);
                const unsigned char *input_row_ptr = raw_input_buffer + (y * width_sz * RGB888_BYTES_PER_PIXEL);
                unsigned char *output_row_ptr = bmp_pixel_buffer + (target_row_index * row_stride);

                for (size_t x = 0; x < width_sz; x++) {
                        size_t src_idx = x * RGB888_BYTES_PER_PIXEL;
                        size_t dst_idx = x * BMP_PIXEL_BYTES_24BPP;
                        unsigned char r = input_row_ptr[src_idx + 0u];
                        unsigned char g = input_row_ptr[src_idx + 1u];
                        unsigned char b = input_row_ptr[src_idx + 2u];

                        output_row_ptr[dst_idx + 0u] = b;
                        output_row_ptr[dst_idx + 1u] = g;
                        output_row_ptr[dst_idx + 2u] = r;
                }
        }
        printf("STATUS : Pixel conversion and stride alignment completed successfully.\n");

        printf("STATUS : Writing padded pixel array to output BMP file...\n");
        fwrite(bmp_pixel_buffer, 1, pixel_array_size, output_file);
        printf("STATUS : Pixel array written to disk successfully.\n");

        free(raw_input_buffer);
        free(bmp_pixel_buffer);
        fclose(input_file);
        fclose(output_file);
        free(input_path);
        free(output_path);
        printf("STATUS : File streams and allocated buffers closed/freed successfully.\n");

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
        int height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;
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

        if (width > 0 && height > 0 && is_supported_bmp_bit_count(info_header.bi_bit_count)) {
                size_t row_stride = calculate_row_stride((size_t)width, info_header.bi_bit_count);
                size_t expected_pixel_array_size = row_stride * (size_t)height;
                size_t expected_total_size = pixel_offset + expected_pixel_array_size;

                if (disk_file_size >= expected_total_size) {
                        printf("PASS : File Size Consistency: Disk size (%zu bytes) matches/exceeds structural requirements\n",
                               disk_file_size);
                } else {
                        printf("FAIL : File Size Consistency: Truncated file! Expected at least %zu bytes, found %zu bytes\n",
                               expected_total_size, disk_file_size);
                        error_count++;
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

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("ERROR : Failed to open input BMP");
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading BMP headers...\n");
        if (!read_and_validate_bmp_headers(input_file, &file_header, &info_header)) {
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        size_t base_header_bytes = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        size_t extra_header_bytes = ((size_t)file_header.bf_off_bits > base_header_bytes)
                                      ? ((size_t)file_header.bf_off_bits - base_header_bytes)
                                      : 0;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes > 0) {
                printf("STATUS : Reading %zu extra header bytes...\n", extra_header_bytes);
                extra_header_buffer = malloc(extra_header_bytes);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes, input_file) != extra_header_bytes) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                } else {
                        perror("ERROR : Failed to allocate extra header buffer");
                }
        }

        size_t width = (size_t)info_header.bi_width;
        size_t height = (size_t)(info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height);
        size_t row_stride = calculate_row_stride(width, info_header.bi_bit_count);
        size_t pixel_array_size = row_stride * height;

        printf("STATUS : Allocating memory for source and destination pixel arrays...\n");
        unsigned char *src_pixels = malloc(pixel_array_size);
        unsigned char *dst_pixels = calloc(1, pixel_array_size);
        if (!src_pixels || !dst_pixels) {
                perror("ERROR : Memory allocation failed");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Pixel memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset (%u bytes)...\n", file_header.bf_off_bits);
        if (fseek(input_file, (long)file_header.bf_off_bits, SEEK_SET) != 0) {
                perror("ERROR : Failed to seek pixel array offset");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        if (fread(src_pixels, 1, pixel_array_size, input_file) != pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Source pixel array read into memory successfully. Input file closed.\n");

        printf("STATUS : Executing vertical flip pixel row re-ordering...\n");
        for (size_t y = 0; y < height; y++) {
                unsigned char *src_row_ptr = src_pixels + (y * row_stride);
                unsigned char *dst_row_ptr = dst_pixels + ((height - 1u - y) * row_stride);
                for (size_t x = 0; x < width; x++) {
                        size_t p_offset = x * BMP_PIXEL_BYTES_24BPP;
                        dst_row_ptr[p_offset + 0u] = src_row_ptr[p_offset + 0u];
                        dst_row_ptr[p_offset + 1u] = src_row_ptr[p_offset + 1u];
                        dst_row_ptr[p_offset + 2u] = src_row_ptr[p_offset + 2u];
                }
        }
        printf("STATUS : Vertical flip row re-ordering completed successfully.\n");

        info_header.bi_height = (int32_t)height;

        printf("STATUS : Creating output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("ERROR : Failed to create output file");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Output BMP file created successfully.\n");

        printf("STATUS : Writing headers and flipped pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes, output_file);
        }
        fwrite(dst_pixels, 1, pixel_array_size, output_file);
        printf("STATUS : All headers and pixel data written to disk successfully.\n");

        fclose(output_file);
        free(extra_header_buffer);
        free(src_pixels);
        free(dst_pixels);
        free(input_path);
        free(output_path);
        printf("STATUS : Output file closed and all buffers freed successfully.\n");

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

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("ERROR : Failed to open input BMP");
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading BMP headers...\n");
        if (!read_and_validate_bmp_headers(input_file, &file_header, &info_header)) {
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        size_t base_header_bytes = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        size_t extra_header_bytes = ((size_t)file_header.bf_off_bits > base_header_bytes)
                                      ? ((size_t)file_header.bf_off_bits - base_header_bytes)
                                      : 0;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes > 0) {
                printf("STATUS : Reading %zu extra header bytes...\n", extra_header_bytes);
                extra_header_buffer = malloc(extra_header_bytes);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes, input_file) != extra_header_bytes) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                } else {
                        perror("ERROR : Failed to allocate extra header buffer");
                }
        }

        size_t width = (size_t)info_header.bi_width;
        size_t height = (size_t)(info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height);
        size_t row_stride = calculate_row_stride(width, info_header.bi_bit_count);
        size_t pixel_array_size = row_stride * height;

        printf("STATUS : Allocating memory for pixel buffers...\n");
        unsigned char *src_pixels = malloc(pixel_array_size);
        unsigned char *dst_pixels = calloc(1, pixel_array_size);
        if (!src_pixels || !dst_pixels) {
                perror("ERROR : Memory allocation failed");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Pixel memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset...\n");
        if (fseek(input_file, (long)file_header.bf_off_bits, SEEK_SET) != 0) {
                perror("ERROR : Failed to seek pixel array offset");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        if (fread(src_pixels, 1, pixel_array_size, input_file) != pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Pixel array read into memory successfully. Input file closed.\n");

        printf("STATUS : Executing mirror (horizontal flip) transformation...\n");
        for (size_t y = 0; y < height; y++) {
                unsigned char *src_row_ptr = src_pixels + (y * row_stride);
                unsigned char *dst_row_ptr = dst_pixels + (y * row_stride);
                for (size_t x = 0; x < width; x++) {
                        size_t src_p = x * BMP_PIXEL_BYTES_24BPP;
                        size_t dst_p = (width - 1u - x) * BMP_PIXEL_BYTES_24BPP;
                        dst_row_ptr[dst_p + 0u] = src_row_ptr[src_p + 0u];
                        dst_row_ptr[dst_p + 1u] = src_row_ptr[src_p + 1u];
                        dst_row_ptr[dst_p + 2u] = src_row_ptr[src_p + 2u];
                }
        }
        printf("STATUS : Mirror pixel transformation completed successfully.\n");

        printf("STATUS : Creating output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("ERROR : Failed to create output file");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Output BMP file created successfully.\n");

        printf("STATUS : Writing headers and mirrored pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes, output_file);
        }
        fwrite(dst_pixels, 1, pixel_array_size, output_file);
        printf("STATUS : Headers and mirrored pixel buffer written to disk successfully.\n");

        fclose(output_file);
        free(extra_header_buffer);
        free(src_pixels);
        free(dst_pixels);
        free(input_path);
        free(output_path);
        printf("STATUS : Output file closed and allocated buffers freed successfully.\n");

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

        // Preview source dimensions before prompting rotation angle
        printf("STATUS : Inspecting source image dimensions for preview guidance...\n");
        FILE *preview_file = fopen(input_path, "rb");
        if (preview_file) {
                bitmap_info_header p_info;
                fseek(preview_file, (long)sizeof(bitmap_file_header), SEEK_SET);
                if (fread(&p_info, sizeof(bitmap_info_header), 1, preview_file) == 1) {
                        int p_h = p_info.bi_height < 0 ? -p_info.bi_height : p_info.bi_height;
                        printf("INFO : Source Image Details -> Width: %d pixels, Height: %d pixels\n",
                               p_info.bi_width, p_h);
                }
                fclose(preview_file);
                printf("STATUS : Source image inspection completed successfully.\n");
        }

        size_t attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter rotation angle (90, 180, or 270)", &angle,
                                                INT_CONSTRAINT_POSITIVE, NULL)) {
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
                free(input_path);
                return;
        }

        if (!prompt_output_bmp_path("Enter output .bmp file path", NULL, &output_path)) {
                free(input_path);
                return;
        }

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("ERROR : Failed to open input BMP");
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading BMP headers...\n");
        if (!read_and_validate_bmp_headers(input_file, &file_header, &info_header)) {
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        size_t base_header_bytes = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        size_t extra_header_bytes = ((size_t)file_header.bf_off_bits > base_header_bytes)
                                      ? ((size_t)file_header.bf_off_bits - base_header_bytes)
                                      : 0;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes > 0) {
                printf("STATUS : Reading %zu extra header bytes...\n", extra_header_bytes);
                extra_header_buffer = malloc(extra_header_bytes);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes, input_file) != extra_header_bytes) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                } else {
                        perror("ERROR : Failed to allocate extra header buffer");
                }
        }

        size_t src_width = (size_t)info_header.bi_width;
        size_t src_height = (size_t)(info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height);
        size_t src_row_stride = calculate_row_stride(src_width, info_header.bi_bit_count);
        size_t src_pixel_array_size = src_row_stride * src_height;

        printf("STATUS : Allocating memory for source pixel array...\n");
        unsigned char *src_pixels = malloc(src_pixel_array_size);
        if (!src_pixels) {
                perror("ERROR : Memory allocation failed");
                free(extra_header_buffer);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Source memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset...\n");
        if (fseek(input_file, (long)file_header.bf_off_bits, SEEK_SET) != 0) {
                perror("ERROR : Failed to seek pixel array offset");
                free(src_pixels);
                free(extra_header_buffer);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        if (fread(src_pixels, 1, src_pixel_array_size, input_file) != src_pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Source pixel array read successfully. Input file closed.\n");

        size_t dst_width = (angle == 180) ? src_width : src_height;
        size_t dst_height = (angle == 180) ? src_height : src_width;
        size_t dst_row_stride = calculate_row_stride(dst_width, info_header.bi_bit_count);
        size_t dst_pixel_array_size = dst_row_stride * dst_height;

        printf("STATUS : Allocating memory for rotated destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, dst_pixel_array_size);
        if (!dst_pixels) {
                perror("ERROR : Destination memory allocation failed");
                free(src_pixels);
                free(extra_header_buffer);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Destination memory allocated successfully.\n");

        printf("STATUS : Executing %d-degree rotation (%s)...\n",
               angle, (dir_flag == 'c') ? "Clockwise" : "Anticlockwise");
        for (size_t y = 0; y < src_height; y++) {
                unsigned char *src_row_ptr = src_pixels + (y * src_row_stride);
                for (size_t x = 0; x < src_width; x++) {
                        unsigned char *src_pixel = src_row_ptr + (x * BMP_PIXEL_BYTES_24BPP);
                        size_t dst_x = x;
                        size_t dst_y = y;

                        if (angle == 90) {
                                if (dir_flag == 'a') {
                                        dst_x = src_height - 1u - y;
                                        dst_y = x;
                                } else {
                                        dst_x = y;
                                        dst_y = src_width - 1u - x;
                                }
                        } else if (angle == 180) {
                                dst_x = src_width - 1u - x;
                                dst_y = src_height - 1u - y;
                        } else if (angle == 270) {
                                if (dir_flag == 'a') {
                                        dst_x = y;
                                        dst_y = src_width - 1u - x;
                                } else {
                                        dst_x = src_height - 1u - y;
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

        info_header.bi_width = (int32_t)dst_width;
        info_header.bi_height = (int32_t)dst_height;
        info_header.bi_size_image = (uint32_t)dst_pixel_array_size;
        file_header.bf_size = (uint32_t)((size_t)file_header.bf_off_bits + dst_pixel_array_size);

        printf("STATUS : Creating rotated output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("ERROR : Failed to create output file");
                free(src_pixels);
                free(dst_pixels);
                free(extra_header_buffer);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Output file created successfully.\n");

        printf("STATUS : Writing rotated headers and pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes, output_file);
        }
        fwrite(dst_pixels, 1, dst_pixel_array_size, output_file);
        printf("STATUS : Rotated headers and pixels written to disk successfully.\n");

        fclose(output_file);
        free(src_pixels);
        free(dst_pixels);
        free(extra_header_buffer);
        free(input_path);
        free(output_path);
        printf("STATUS : Output file closed and allocated buffers freed successfully.\n");

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

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("ERROR : Failed to open input BMP");
                free(input_path);
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;
        printf("STATUS : Reading BMP headers for crop coordinate guidance...\n");
        if (!read_and_validate_bmp_headers(input_file, &file_header, &info_header)) {
                fclose(input_file);
                free(input_path);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        int src_width = info_header.bi_width;
        int src_height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;

        // Show valid coordinate and dimension ranges before asking for crop values
        printf("\nINFO : Source Image Loaded Successfully:\n");
        printf("INFO :   - Total Width  : %d pixels (Valid X range: 0 to %d)\n", src_width, src_width - 1);
        printf("INFO :   - Total Height : %d pixels (Valid Y range: 0 to %d)\n\n", src_height, src_height - 1);

        size_t attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter starting X coordinate", &start_x,
                                                INT_CONSTRAINT_ANY, NULL)) {
                        fclose(input_file);
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
                fclose(input_file);
                free(input_path);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter starting Y coordinate", &start_y,
                                                INT_CONSTRAINT_ANY, NULL)) {
                        fclose(input_file);
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
                fclose(input_file);
                free(input_path);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter crop width", &crop_width,
                                                INT_CONSTRAINT_POSITIVE, NULL)) {
                        fclose(input_file);
                        free(input_path);
                        return;
                }
                attempts--;
                if ((start_x + crop_width) > src_width) {
                        printf("ERROR : Invalid width. Must be > 0 and start_x + width <= %d (Max available: %d).\n",
                               src_width, src_width - start_x);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                fclose(input_file);
                free(input_path);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int_constrained("Enter crop height", &crop_height,
                                                INT_CONSTRAINT_POSITIVE, NULL)) {
                        fclose(input_file);
                        free(input_path);
                        return;
                }
                attempts--;
                if ((start_y + crop_height) > src_height) {
                        printf("ERROR : Invalid height. Must be > 0 and start_y + height <= %d (Max available: %d).\n",
                               src_height, src_height - start_y);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                fclose(input_file);
                free(input_path);
                return;
        }

        if (!prompt_output_bmp_path("Enter output .bmp file path", NULL, &output_path)) {
                fclose(input_file);
                free(input_path);
                return;
        }

        size_t base_header_bytes = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        size_t extra_header_bytes = ((size_t)file_header.bf_off_bits > base_header_bytes)
                                      ? ((size_t)file_header.bf_off_bits - base_header_bytes)
                                      : 0;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes > 0) {
                printf("STATUS : Reading %zu extra header bytes...\n", extra_header_bytes);
                extra_header_buffer = malloc(extra_header_bytes);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes, input_file) != extra_header_bytes) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                } else {
                        perror("ERROR : Failed to allocate extra header buffer");
                }
        }

        size_t src_w_sz = (size_t)src_width;
        size_t src_h_sz = (size_t)src_height;
        size_t src_row_stride = calculate_row_stride(src_w_sz, info_header.bi_bit_count);
        size_t src_pixel_array_size = src_row_stride * src_h_sz;

        printf("STATUS : Allocating memory for source pixel array...\n");
        unsigned char *src_pixels = malloc(src_pixel_array_size);
        if (!src_pixels) {
                perror("ERROR : Memory allocation failed");
                free(extra_header_buffer);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Source memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset...\n");
        if (fseek(input_file, (long)file_header.bf_off_bits, SEEK_SET) != 0) {
                perror("ERROR : Failed to seek pixel array offset");
                free(src_pixels);
                free(extra_header_buffer);
                fclose(input_file);
                free(input_path);
                free(output_path);
                return;
        }
        if (fread(src_pixels, 1, src_pixel_array_size, input_file) != src_pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Source pixel array read successfully. Input file closed.\n");

        size_t crop_w_sz = (size_t)crop_width;
        size_t crop_h_sz = (size_t)crop_height;
        size_t dst_row_stride = calculate_row_stride(crop_w_sz, info_header.bi_bit_count);
        size_t dst_pixel_array_size = dst_row_stride * crop_h_sz;

        printf("STATUS : Allocating memory for cropped destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, dst_pixel_array_size);
        if (!dst_pixels) {
                perror("ERROR : Destination memory allocation failed");
                free(src_pixels);
                free(extra_header_buffer);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Destination memory allocated successfully.\n");

        printf("STATUS : Extracting bounding-box region pixels (%d,%d to %d,%d)...\n",
               start_x, start_y, start_x + crop_width, start_y + crop_height);
        for (size_t y = 0; y < crop_h_sz; y++) {
                size_t src_y = (size_t)start_y + y;
                unsigned char *src_row_ptr = src_pixels + (src_y * src_row_stride);
                unsigned char *dst_row_ptr = dst_pixels + (y * dst_row_stride);
                for (size_t x = 0; x < crop_w_sz; x++) {
                        size_t src_x = (size_t)start_x + x;
                        unsigned char *src_pixel = src_row_ptr + (src_x * BMP_PIXEL_BYTES_24BPP);
                        unsigned char *dst_pixel = dst_row_ptr + (x * BMP_PIXEL_BYTES_24BPP);
                        dst_pixel[0u] = src_pixel[0u];
                        dst_pixel[1u] = src_pixel[1u];
                        dst_pixel[2u] = src_pixel[2u];
                }
        }
        printf("STATUS : Bounding-box pixel extraction completed successfully.\n");

        info_header.bi_width = crop_width;
        info_header.bi_height = crop_height;
        info_header.bi_size_image = (uint32_t)dst_pixel_array_size;
        file_header.bf_size = (uint32_t)((size_t)file_header.bf_off_bits + dst_pixel_array_size);

        printf("STATUS : Creating cropped output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("ERROR : Failed to create output file");
                free(src_pixels);
                free(dst_pixels);
                free(extra_header_buffer);
                free(input_path);
                free(output_path);
                return;
        }
        printf("STATUS : Output file created successfully.\n");

        printf("STATUS : Writing updated headers and cropped pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes, output_file);
        }
        fwrite(dst_pixels, 1, dst_pixel_array_size, output_file);
        printf("STATUS : Cropped headers and pixel buffer written to disk successfully.\n");

        fclose(output_file);
        free(src_pixels);
        free(dst_pixels);
        free(extra_header_buffer);
        free(input_path);
        free(output_path);
        printf("STATUS : Output file closed and allocated buffers freed successfully.\n");

        printf("\n^_^ Cropping completed successfully!\n");
}
