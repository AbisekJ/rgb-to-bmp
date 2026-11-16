#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bmp_converter.h"

//  INTERNAL HELPER: Load Headers 
static void load_bmp_headers(int image_width, int image_height, uint32_t pixel_array_size, bitmap_file_header *file_header, bitmap_info_header *info_header) {
        uint32_t header_size = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
        uint32_t total_file_size = header_size + pixel_array_size;
        
        file_header->bf_type = BMP_MAGIC_SIGNATURE;
        file_header->bf_size = total_file_size;
        file_header->bf_reserved_1 = 0;
        file_header->bf_reserved_2 = 0;
        file_header->bf_off_bits = header_size;
        
        info_header->bi_size = STANDARD_DIB_SIZE;
        info_header->bi_width = image_width;
        info_header->bi_height = image_height;
        info_header->bi_planes = 1;
        info_header->bi_bit_count = TARGET_BIT_COUNT;
        info_header->bi_compression = 0;
        info_header->bi_size_image = pixel_array_size;
        info_header->bi_x_pels_per_meter = 2835;
        info_header->bi_y_pels_per_meter = 2835;
        info_header->bi_clr_used = 0;
        info_header->bi_clr_important = 0;
}

//  FEATURE 1: RGB888 to BMP
void handle_rgb_to_bmp_conversion(void) {
        char input_path[MAX_PATH_LENGTH];
        char output_path[MAX_PATH_LENGTH];
        int image_width = 0;
        int image_height = 0;

        printf("\n ========== Feature 1: Raw RGB888 to 24-bit BMP Conversion ========== \n");

        int valid_input = 0;
        int attempts = DEFAULT_MAX_RETRIES;
        while (attempts > 0 && !valid_input) {
                if (!read_strict_string("Enter input .rgb file path", input_path, attempts)) {
                        break;
                }
                attempts--;
                if (!validate_file_extension(input_path, ".rgb")) {
                        printf("ERROR : Input file must have a '.rgb' extension.\n");
                        continue;
                }
                FILE *test_file = fopen(input_path, "rb");
                if (!test_file) {
                        printf("ERROR : Failed to open input file '%s'. Check if file exists.\n", input_path);
                        continue;
                }
                fclose(test_file);
                valid_input = 1;
        }
        if (!valid_input) {
                printf("ABORT : Maximum retry attempts reached for input file. Returning to menu.\n");
                return;
        }

        int valid_width = 0;
        attempts = DEFAULT_MAX_RETRIES;
        while (attempts > 0 && !valid_width) {
                if (!read_strict_int("Enter image width in pixels", &image_width, attempts)) {
                        break;
                }
                attempts--;
                if (image_width <= 0) {
                        printf("ERROR : Width (%d) must be strictly positive (> 0).\n", image_width);
                        continue;
                }
                valid_width = 1;
        }
        if (!valid_width) {
                printf("ABORT : Maximum retry attempts reached for image width. Returning to menu.\n");
                return;
        }

        int valid_height = 0;
        attempts = DEFAULT_MAX_RETRIES;
        while (attempts > 0 && !valid_height) {
                if (!read_strict_int("Enter image height (+ bottom-up, - top-down)", &image_height, attempts)) {
                        break;
                }
                attempts--;
                if (image_height == 0) {
                        printf("ERROR : Image height cannot be zero.\n");
                        continue;
                }
                valid_height = 1;
        }
        if (!valid_height) {
                printf("ABORT : Maximum retry attempts reached for image height. Returning to menu.\n");
                return;
        }

        int valid_output = 0;
        attempts = DEFAULT_MAX_RETRIES;
        while (attempts > 0 && !valid_output) {
                if (!read_strict_string("Enter output .bmp file path", output_path, attempts)) {
                        break;
                }
                attempts--;
                if (!validate_file_extension(output_path, ".bmp")) {
                        printf("ERROR : Output file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *existing_file = fopen(output_path, "rb");
                if (existing_file) {
                        fclose(existing_file);
                        int confirm = get_strict_overwrite_confirmation(output_path);
                        if (confirm == 1) {
                                valid_output = 1;
                        } else if (confirm == 0) {
                                printf("INFO : Overwrite declined. Please provide a different output file path.\n");
                                continue;
                        } else {
                                return;
                        }
                } else {
                        valid_output = 1;
                }
        }
        if (!valid_output) {
                printf("ABORT : Maximum retry attempts reached for output file. Returning to menu.\n");
                return;
        }

        printf("STATUS : Opening raw input file '%s' in binary read mode...\n", input_path);
        int abs_height = image_height < 0 ? -image_height : image_height;
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("Failed to open raw input file");
                return;
        }
        printf("STATUS : Raw input file opened successfully.\n");

        printf("STATUS : Verifying input raw file size match...\n");
        fseek(input_file, 0, SEEK_END);
        long actual_file_size = ftell(input_file);
        rewind(input_file);

        size_t expected_raw_size = (size_t)image_width * abs_height * 3;
        if ((size_t)actual_file_size != expected_raw_size) {
                fprintf(stderr, "ERROR : Input file size mismatch! Expected %zu bytes, got %ld bytes.\n", expected_raw_size, actual_file_size);
                fclose(input_file);
                return;
        }
        printf("STATUS : Input file size verified successfully (%ld bytes matches dimensions).\n", actual_file_size);

        int row_stride = calculate_row_stride(image_width);
        uint32_t pixel_array_size = row_stride * abs_height;

        bitmap_file_header file_header;
        bitmap_info_header info_header;
        load_bmp_headers(image_width, image_height, pixel_array_size, &file_header, &info_header);

        printf("STATUS : Creating BMP output file '%s' in binary write mode...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("Failed to create BMP output file");
                fclose(input_file);
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
                perror("Memory allocation failed");
                free(raw_input_buffer);
                free(bmp_pixel_buffer);
                fclose(input_file);
                fclose(output_file);
                return;
        }
        printf("STATUS : Memory allocation completed successfully.\n");

        printf("STATUS : Reading entire raw input stream into memory buffer...\n");
        if (fread(raw_input_buffer, 1, expected_raw_size, input_file) != expected_raw_size) {
                fprintf(stderr, "ERROR : Failed to read complete raw input data.\n");
        }
        printf("STATUS : Raw input stream read finished successfully.\n");

        printf("STATUS : Converting RGB888 pixels to BGR stride-aligned pixel array...\n");
        for (int y = 0; y < abs_height; y++) {
                int target_row_index = (image_height < 0) ? y : (abs_height - 1 - y);
                const unsigned char *input_row_ptr = raw_input_buffer + (y * image_width * 3);
                unsigned char *output_row_ptr = bmp_pixel_buffer + (target_row_index * row_stride);

                for (int x = 0; x < image_width; x++) {
                        unsigned char r = input_row_ptr[x * 3 + 0];
                        unsigned char g = input_row_ptr[x * 3 + 1];
                        unsigned char b = input_row_ptr[x * 3 + 2];
                        
                        output_row_ptr[x * 3 + 0] = b;
                        output_row_ptr[x * 3 + 1] = g;
                        output_row_ptr[x * 3 + 2] = r;
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
        printf("STATUS : File streams and allocated buffers closed/freed successfully.\n");

        printf("\n^_^ Conversion successfully finished! Saved to: %s\n", output_path);
}

//  FEATURE 2: BMP Header Parser & Pretty Printer 
void handle_bmp_header_parsing(void) {
        char bmp_path[MAX_PATH_LENGTH];
        int attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;

        printf("\n ========== Feature 2: Parse and Print BMP Headers ========== \n");

        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter existing .bmp file path", bmp_path, attempts)) {
                        break;
                }
                attempts--;
                if (!validate_file_extension(bmp_path, ".bmp")) {
                        printf("ERROR : File must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *test_file = fopen(bmp_path, "rb");
                if (!test_file) {
                        printf("ERROR : Failed to open file '%s'. Check if file exists.\n", bmp_path);
                        continue;
                }
                fclose(test_file);
                valid = 1;
        }
        if (!valid) {
                printf("ABORT : File path validation failed.\n");
                return;
        }

        printf("STATUS : Opening BMP file '%s' in binary read mode...\n", bmp_path);
        FILE *bmp_file = fopen(bmp_path, "rb");
        if (!bmp_file) {
                perror("Failed to open BMP file");
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
                return;
        }
        printf("STATUS : Headers read successfully from file stream.\n");
        
        fclose(bmp_file);
        printf("STATUS : BMP file closed successfully.\n");

        printf("\n====================================================================\n");
        printf("                 BMP FILE METADATA INSPECTION REPORT                \n");
        printf("====================================================================\n");
        printf("[1] bitmap_file_header (14 Bytes):\n");
        printf("    - bf_type          : 0x%04X ('%c%c') %s\n", 
                file_header.bf_type, (char)(file_header.bf_type & 0xFF), (char)(file_header.bf_type >> 8),
                file_header.bf_type == BMP_MAGIC_SIGNATURE ? "VALID" : "INVALID");
        printf("    - bf_size          : %u bytes\n", file_header.bf_size);
        printf("    - bf_off_bits      : %u bytes\n", file_header.bf_off_bits);

        printf("\n[2] bitmap_info_header (40 Bytes):\n");
        printf("    - bi_width         : %u pixels\n", info_header.bi_width);
        printf("    - bi_height        : %d pixels (%s)\n", info_header.bi_height, info_header.bi_height < 0 ? "Top-Down" : "Bottom-Up");
        printf("    - bi_bit_count     : %u BPP\n", info_header.bi_bit_count);
        printf("    - bi_compression   : %u\n", info_header.bi_compression);
        printf("    - bi_size_image    : %u bytes\n", info_header.bi_size_image);
        printf("====================================================================\n");
        printf("STATUS : Header inspection completed successfully.\n");
}

//  FEATURE 3: Comprehensive BMP File Validator 
void handle_bmp_validation(void) {
        char bmp_path[MAX_PATH_LENGTH];
        int attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;

        printf("\n ========== Feature 3: BMP File Validator ========== \n");

        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter .bmp file path to validate", bmp_path, attempts)) {
                        break;
                }
                attempts--;
                if (!validate_file_extension(bmp_path, ".bmp")) {
                        printf("ERROR : File must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *test_file = fopen(bmp_path, "rb");
                if (!test_file) {
                        printf("ERROR : Failed to open file '%s'. Check if file exists.\n", bmp_path);
                        continue;
                }
                fclose(test_file);
                valid = 1;
        }
        if (!valid) {
                printf("ABORT : File path validation failed.\n");
                return;
        }

        printf("STATUS : Opening BMP file '%s' for structural validation...\n", bmp_path);
        FILE *bmp_file = fopen(bmp_path, "rb");
        if (!bmp_file) {
                perror("Failed to open BMP file");
                return;
        }
        printf("STATUS : BMP file opened successfully.\n");

        printf("STATUS : Determining total file size on disk...\n");
        if (fseek(bmp_file, 0, SEEK_END) != 0) {
                perror("Failed to seek BMP file");
                fclose(bmp_file);
                return;
        }
        long disk_file_size = ftell(bmp_file);
        rewind(bmp_file);
        printf("STATUS : File size determined successfully (%ld bytes).\n", disk_file_size);

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading headers for structural integrity checks...\n");
        if (fread(&file_header, sizeof(bitmap_file_header), 1, bmp_file) != 1 ||
            fread(&info_header, sizeof(bitmap_info_header), 1, bmp_file) != 1) {
                fprintf(stderr, "ERROR : File is too small to contain valid BMP headers.\n");
                fclose(bmp_file);
                return;
        }
        fclose(bmp_file);
        printf("STATUS : Headers read and file stream closed successfully.\n");

        int error_count = 0;
        printf("\n====================================================================\n");
        printf("                   BMP STRUCTURAL VALIDATION REPORT                 \n");
        printf("====================================================================\n");

        if (file_header.bf_type == BMP_MAGIC_SIGNATURE) {
                printf("PASS : Magic Signature (bf_type): 0x%04X ('BM')\n", file_header.bf_type);
        } else {
                printf("FAIL : Magic Signature (bf_type): 0x%04X (Expected 0x4D42)\n", file_header.bf_type);
                error_count++;
        }

        if (info_header.bi_size == 40 || info_header.bi_size == 108 || info_header.bi_size == 124 || info_header.bi_size == 52 || info_header.bi_size == 56) {
                printf("PASS : DIB Header Size (bi_size): %u bytes (Valid Header)\n", info_header.bi_size);
        } else {
                printf("FAIL : DIB Header Size (bi_size): %u bytes (Unsupported DIB header)\n", info_header.bi_size);
                error_count++;
        }

        if (info_header.bi_planes == 1) {
                printf("PASS : Color Planes (bi_planes): %u\n", info_header.bi_planes);
        } else {
                printf("FAIL : Color Planes (bi_planes): %u (Must be 1)\n", info_header.bi_planes);
                error_count++;
        }

        if (info_header.bi_bit_count == TARGET_BIT_COUNT) {
                printf("PASS : Color Depth (bi_bit_count): 24 BPP\n");
        } else {
                printf("FAIL : Color Depth (bi_bit_count): %u BPP (Toolkit expects 24 BPP)\n", info_header.bi_bit_count);
                error_count++;
        }

        if (info_header.bi_compression == 0) {
                printf("PASS : Compression Scheme: BI_RGB Uncompressed\n");
        } else {
                printf("FAIL : Compression Scheme: %u (Toolkit supports uncompressed only)\n", info_header.bi_compression);
                error_count++;
        }

        int width = info_header.bi_width;
        int height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;
        if (width > 0 && info_header.bi_height != 0) {
                printf("PASS : Dimensions Check: Width = %d, Height = %d\n", width, info_header.bi_height);
        } else {
                printf("FAIL : Dimensions Check: Width = %d, Height = %d (Invalid dimensions)\n", width, info_header.bi_height);
                error_count++;
        }

        uint32_t min_header_offset = sizeof(bitmap_file_header) + info_header.bi_size;
        if (file_header.bf_off_bits >= min_header_offset && (long)file_header.bf_off_bits < disk_file_size) {
                printf("PASS : Pixel Array Offset (bf_off_bits): %u bytes\n", file_header.bf_off_bits);
        } else {
                printf("FAIL : Pixel Array Offset (bf_off_bits): %u bytes (Invalid offset boundary)\n", file_header.bf_off_bits);
                error_count++;
        }

        if (width > 0 && height > 0) {
                long long row_stride = ((1LL * width * TARGET_BIT_COUNT + 31) / 32) * 4;
                long long expected_pixel_array_size = row_stride * height;
                long long expected_total_size = file_header.bf_off_bits + expected_pixel_array_size;

                if (disk_file_size >= expected_total_size) {
                        printf("PASS : File Size Consistency: Disk size (%ld bytes) matches/exceeds structural requirements\n", disk_file_size);
                } else {
                        printf("FAIL : File Size Consistency: Truncated file! Expected at least %lld bytes, found %ld bytes\n", expected_total_size, disk_file_size);
                        error_count++;
                }
        }

        printf("--------------------------------------------------------------------\n");
        if (error_count == 0) {
                printf("RESULT : SUCCESS - BMP file is fully valid and safe to process!\n");
        } else {
                printf("RESULT : FAILURE - BMP file contains %d critical structural error(s).\n", error_count);
        }
        printf("====================================================================\n");
        printf("STATUS : Validation routine completed successfully.\n");
}

//  FEATURE 4: Vertical Flip Transformation 
void handle_vertical_flip(void) {
        char input_path[MAX_PATH_LENGTH];
        char output_path[MAX_PATH_LENGTH];

        printf("\n ========== Feature 4: Vertical Flip Transformation ========== \n");

        int attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter input .bmp file path", input_path, attempts)) {
                        return;
                }
                attempts--;
                if (!validate_file_extension(input_path, ".bmp")) {
                        printf("ERROR : Input file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *test_file = fopen(input_path, "rb");
                if (!test_file) {
                        printf("ERROR : Failed to open input file '%s'. Check if file exists.\n", input_path);
                        continue;
                }
                fclose(test_file);
                valid = 1;
        }
        if (!valid) {
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter output .bmp file path", output_path, attempts)) {
                        return;
                }
                attempts--;
                if (!validate_file_extension(output_path, ".bmp")) {
                        printf("ERROR : Output file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *existing = fopen(output_path, "rb");
                if (existing) {
                        fclose(existing);
                        int confirm = get_strict_overwrite_confirmation(output_path);
                        if (confirm == 1) {
                                valid = 1;
                        } else if (confirm == 0) {
                                printf("INFO : Overwrite declined. Please provide a different path.\n");
                                continue;
                        } else {
                                return;
                        }
                } else {
                        valid = 1;
                }
        }
        if (!valid) {
                return;
        }

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("Failed to open input BMP");
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading BMP headers...\n");
        if (fread(&file_header, sizeof(bitmap_file_header), 1, input_file) != 1 ||
            fread(&info_header, sizeof(bitmap_info_header), 1, input_file) != 1) {
                fprintf(stderr, "ERROR : Failed to read BMP headers.\n");
                fclose(input_file);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        if (file_header.bf_type != BMP_MAGIC_SIGNATURE || info_header.bi_bit_count != TARGET_BIT_COUNT || info_header.bi_compression != 0) {
                fprintf(stderr, "ERROR : Unsupported or invalid 24-bit uncompressed BMP file.\n");
                fclose(input_file);
                return;
        }

        long extra_header_bytes_count = (long)file_header.bf_off_bits - 54;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes_count > 0) {
                printf("STATUS : Reading %ld extra header bytes...\n", extra_header_bytes_count);
                extra_header_buffer = malloc(extra_header_bytes_count);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes_count, input_file) != (size_t)extra_header_bytes_count) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                }
        }

        int width = info_header.bi_width;
        int height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;
        int row_stride = ((width * TARGET_BIT_COUNT + 31) / 32) * 4;
        uint32_t pixel_array_size = row_stride * height;

        printf("STATUS : Allocating memory for source and destination pixel arrays...\n");
        unsigned char *src_pixels = malloc(pixel_array_size);
        unsigned char *dst_pixels = calloc(1, pixel_array_size);
        if (!src_pixels || !dst_pixels) {
                perror("Memory allocation failed");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                fclose(input_file);
                return;
        }
        printf("STATUS : Pixel memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset (%u bytes)...\n", file_header.bf_off_bits);
        fseek(input_file, file_header.bf_off_bits, SEEK_SET);
        if (fread(src_pixels, 1, pixel_array_size, input_file) != pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Source pixel array read into memory successfully. Input file closed.\n");

        printf("STATUS : Executing vertical flip pixel row re-ordering...\n");
        for (int y = 0; y < height; y++) {
                unsigned char *src_row_ptr = src_pixels + (y * row_stride);
                unsigned char *dst_row_ptr = dst_pixels + ((height - 1 - y) * row_stride);
                for (int x = 0; x < width; x++) {
                        int p_offset = x * 3;
                        dst_row_ptr[p_offset + 0] = src_row_ptr[p_offset + 0];
                        dst_row_ptr[p_offset + 1] = src_row_ptr[p_offset + 1];
                        dst_row_ptr[p_offset + 2] = src_row_ptr[p_offset + 2];
                }
        }
        printf("STATUS : Vertical flip row re-ordering completed successfully.\n");

        info_header.bi_height = height;

        printf("STATUS : Creating output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("Failed to create output file");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                return;
        }
        printf("STATUS : Output BMP file created successfully.\n");

        printf("STATUS : Writing headers and flipped pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes_count > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes_count, output_file);
        }
        fwrite(dst_pixels, 1, pixel_array_size, output_file);
        printf("STATUS : All headers and pixel data written to disk successfully.\n");

        fclose(output_file);
        free(extra_header_buffer); 
        free(src_pixels); 
        free(dst_pixels);
        printf("STATUS : Output file closed and all buffers freed successfully.\n");

        printf("\n^_^ Vertical flip completed successfully! Saved to: %s\n", output_path);
}

//  FEATURE 5: Mirror (Horizontal Flip) Transformation 
void handle_mirror_image(void) {
        char input_path[MAX_PATH_LENGTH];
        char output_path[MAX_PATH_LENGTH];

        printf("\n ========== Feature 5: Mirror / Horizontal Flip Transformation ========== \n");

        int attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter input .bmp file path", input_path, attempts)) {
                        return;
                }
                attempts--;
                if (!validate_file_extension(input_path, ".bmp")) {
                        printf("ERROR : Input file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *test_file = fopen(input_path, "rb");
                if (!test_file) {
                        printf("ERROR : Failed to open input file '%s'. Check if file exists.\n", input_path);
                        continue;
                }
                fclose(test_file);
                valid = 1;
        }
        if (!valid) {
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter output .bmp file path", output_path, attempts)) {
                        return;
                }
                attempts--;
                if (!validate_file_extension(output_path, ".bmp")) {
                        printf("ERROR : Output file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *existing = fopen(output_path, "rb");
                if (existing) {
                        fclose(existing);
                        int confirm = get_strict_overwrite_confirmation(output_path);
                        if (confirm == 1) {
                                valid = 1;
                        } else if (confirm == 0) {
                                printf("INFO : Overwrite declined. Please provide a different path.\n");
                                continue;
                        } else {
                                return;
                        }
                } else {
                        valid = 1;
                }
        }
        if (!valid) {
                return;
        }

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("Failed to open input BMP");
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading BMP headers...\n");
        if (fread(&file_header, sizeof(bitmap_file_header), 1, input_file) != 1 ||
            fread(&info_header, sizeof(bitmap_info_header), 1, input_file) != 1) {
                fprintf(stderr, "ERROR : Failed to read BMP headers.\n");
                fclose(input_file);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        if (file_header.bf_type != BMP_MAGIC_SIGNATURE || info_header.bi_bit_count != TARGET_BIT_COUNT || info_header.bi_compression != 0) {
                fprintf(stderr, "ERROR : Unsupported or invalid 24-bit uncompressed BMP file.\n");
                fclose(input_file);
                return;
        }

        long extra_header_bytes_count = (long)file_header.bf_off_bits - 54;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes_count > 0) {
                printf("STATUS : Reading %ld extra header bytes...\n", extra_header_bytes_count);
                extra_header_buffer = malloc(extra_header_bytes_count);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes_count, input_file) != (size_t)extra_header_bytes_count) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                }
        }

        int width = info_header.bi_width;
        int height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;
        int row_stride = ((width * TARGET_BIT_COUNT + 31) / 32) * 4;
        uint32_t pixel_array_size = row_stride * height;

        printf("STATUS : Allocating memory for pixel buffers...\n");
        unsigned char *src_pixels = malloc(pixel_array_size);
        unsigned char *dst_pixels = calloc(1, pixel_array_size);
        if (!src_pixels || !dst_pixels) {
                perror("Memory allocation failed");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                fclose(input_file);
                return;
        }
        printf("STATUS : Pixel memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset...\n");
        fseek(input_file, file_header.bf_off_bits, SEEK_SET);
        if (fread(src_pixels, 1, pixel_array_size, input_file) != pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Pixel array read into memory successfully. Input file closed.\n");

        printf("STATUS : Executing mirror (horizontal flip) transformation...\n");
        for (int y = 0; y < height; y++) {
                unsigned char *src_row_ptr = src_pixels + (y * row_stride);
                unsigned char *dst_row_ptr = dst_pixels + (y * row_stride);
                for (int x = 0; x < width; x++) {
                        int src_p = x * 3;
                        int dst_p = (width - 1 - x) * 3;
                        dst_row_ptr[dst_p + 0] = src_row_ptr[src_p + 0];
                        dst_row_ptr[dst_p + 1] = src_row_ptr[src_p + 1];
                        dst_row_ptr[dst_p + 2] = src_row_ptr[src_p + 2];
                }
        }
        printf("STATUS : Mirror pixel transformation completed successfully.\n");

        printf("STATUS : Creating output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("Failed to create output file");
                free(extra_header_buffer);
                free(src_pixels);
                free(dst_pixels);
                return;
        }
        printf("STATUS : Output BMP file created successfully.\n");

        printf("STATUS : Writing headers and mirrored pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes_count > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes_count, output_file);
        }
        fwrite(dst_pixels, 1, pixel_array_size, output_file);
        printf("STATUS : Headers and mirrored pixel buffer written to disk successfully.\n");

        fclose(output_file);
        free(extra_header_buffer); 
        free(src_pixels); 
        free(dst_pixels);
        printf("STATUS : Output file closed and allocated buffers freed successfully.\n");

        printf("\n^_^ Mirror completed successfully! Saved to: %s\n", output_path);
}

//  FEATURE 6: Rotation 
void handle_image_rotation(void) {
        char input_path[MAX_PATH_LENGTH];
        char output_path[MAX_PATH_LENGTH];
        int angle = 0;
        char direction_choice[16];
        char dir_flag = 'c';

        printf("\n ========== Feature 6: Image Rotation (Clockwise / Anticlockwise) ========== \n");

        int attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter input .bmp file path", input_path, attempts)) {
                        return;
                }
                attempts--;
                if (!validate_file_extension(input_path, ".bmp")) {
                        printf("ERROR : Input file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *test_file = fopen(input_path, "rb");
                if (!test_file) {
                        printf("ERROR : Failed to open input file '%s'. Check if file exists.\n", input_path);
                        continue;
                }
                fclose(test_file);
                valid = 1;
        }
        if (!valid) {
                return;
        }

        // Preview image details before prompting rotation angle
        printf("STATUS : Inspecting source image dimensions for preview guidance...\n");
        FILE *preview_file = fopen(input_path, "rb");
        if (preview_file) {
                bitmap_info_header p_info;
                fseek(preview_file, sizeof(bitmap_file_header), SEEK_SET);
                if (fread(&p_info, sizeof(bitmap_info_header), 1, preview_file) == 1) {
                        int p_h = p_info.bi_height < 0 ? -p_info.bi_height : p_info.bi_height;
                        printf("INFO : Source Image Details -> Width: %d pixels, Height: %d pixels\n", p_info.bi_width, p_h);
                }
                fclose(preview_file);
                printf("STATUS : Source image inspection completed successfully.\n");
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int("Enter rotation angle (90, 180, or 270)", &angle, attempts)) {
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
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter direction - Clockwise (c) or Anticlockwise (a)", direction_choice, attempts)) {
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
        if (!valid) {
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter output .bmp file path", output_path, attempts)) {
                        return;
                }
                attempts--;
                if (!validate_file_extension(output_path, ".bmp")) {
                        printf("ERROR : Output file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *existing = fopen(output_path, "rb");
                if (existing) {
                        fclose(existing);
                        int confirm = get_strict_overwrite_confirmation(output_path);
                        if (confirm == 1) {
                                valid = 1;
                        } else if (confirm == 0) {
                                printf("INFO : Overwrite declined. Please provide a different path.\n");
                                continue;
                        } else {
                                return;
                        }
                } else {
                        valid = 1;
                }
        }
        if (!valid) {
                return;
        }

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("Failed to open input BMP");
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;

        printf("STATUS : Reading BMP headers...\n");
        if (fread(&file_header, sizeof(bitmap_file_header), 1, input_file) != 1 ||
            fread(&info_header, sizeof(bitmap_info_header), 1, input_file) != 1) {
                fprintf(stderr, "ERROR : Failed to read BMP headers.\n");
                fclose(input_file);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        if (file_header.bf_type != BMP_MAGIC_SIGNATURE || info_header.bi_bit_count != TARGET_BIT_COUNT || info_header.bi_compression != 0) {
                fprintf(stderr, "ERROR : Unsupported or invalid 24-bit uncompressed BMP file.\n");
                fclose(input_file);
                return;
        }

        long extra_header_bytes_count = (long)file_header.bf_off_bits - 54;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes_count > 0) {
                printf("STATUS : Reading %ld extra header bytes...\n", extra_header_bytes_count);
                extra_header_buffer = malloc(extra_header_bytes_count);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes_count, input_file) != (size_t)extra_header_bytes_count) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                }
        }

        int src_width = info_header.bi_width;
        int src_height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;
        int src_row_stride = ((src_width * TARGET_BIT_COUNT + 31) / 32) * 4;
        uint32_t src_pixel_array_size = src_row_stride * src_height;

        printf("STATUS : Allocating memory for source pixel array...\n");
        unsigned char *src_pixels = malloc(src_pixel_array_size);
        if (!src_pixels) {
                perror("Memory allocation failed");
                free(extra_header_buffer);
                fclose(input_file);
                return;
        }
        printf("STATUS : Source memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset...\n");
        fseek(input_file, file_header.bf_off_bits, SEEK_SET);
        if (fread(src_pixels, 1, src_pixel_array_size, input_file) != src_pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Source pixel array read successfully. Input file closed.\n");

        int dst_width = (angle == 180) ? src_width : src_height;
        int dst_height = (angle == 180) ? src_height : src_width;
        int dst_row_stride = ((dst_width * TARGET_BIT_COUNT + 31) / 32) * 4;
        uint32_t dst_pixel_array_size = dst_row_stride * dst_height;

        printf("STATUS : Allocating memory for rotated destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, dst_pixel_array_size);
        if (!dst_pixels) {
                perror("Destination memory allocation failed");
                free(src_pixels);
                free(extra_header_buffer);
                return;
        }
        printf("STATUS : Destination memory allocated successfully.\n");

        printf("STATUS : Executing %d-degree rotation (%s)...\n", angle, (dir_flag == 'c') ? "Clockwise" : "Anticlockwise");
        for (int y = 0; y < src_height; y++) {
                unsigned char *src_row_ptr = src_pixels + (y * src_row_stride);
                for (int x = 0; x < src_width; x++) {
                        unsigned char *src_pixel = src_row_ptr + (x * 3);
                        int dst_x = x, dst_y = y;

                        if (angle == 90) {
                                if (dir_flag == 'a') {
                                        dst_x = src_height - 1 - y;
                                        dst_y = x;
                                } else {
                                        dst_x = y;
                                        dst_y = src_width - 1 - x;
                                }
                        } else if (angle == 180) {
                                dst_x = src_width - 1 - x;
                                dst_y = src_height - 1 - y;
                        } else if (angle == 270) {
                                if (dir_flag == 'a') {
                                        dst_x = y;
                                        dst_y = src_width - 1 - x;
                                } else {
                                        dst_x = src_height - 1 - y;
                                        dst_y = x;
                                }
                        }

                        unsigned char *dst_pixel = dst_pixels + (dst_y * dst_row_stride) + (dst_x * 3);
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                }
        }
        printf("STATUS : Rotation matrix mapping completed successfully.\n");

        info_header.bi_width = dst_width;
        info_header.bi_height = dst_height;
        info_header.bi_size_image = dst_pixel_array_size;
        file_header.bf_size = file_header.bf_off_bits + dst_pixel_array_size;

        printf("STATUS : Creating rotated output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("Failed to create output file");
                free(src_pixels);
                free(dst_pixels);
                free(extra_header_buffer);
                return;
        }
        printf("STATUS : Output file created successfully.\n");

        printf("STATUS : Writing rotated headers and pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes_count > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes_count, output_file);
        }
        fwrite(dst_pixels, 1, dst_pixel_array_size, output_file);
        printf("STATUS : Rotated headers and pixels written to disk successfully.\n");

        fclose(output_file);
        free(src_pixels); 
        free(dst_pixels); 
        free(extra_header_buffer);
        printf("STATUS : Output file closed and allocated buffers freed successfully.\n");

        printf("\n^_^ Rotation completed successfully! Saved to: %s\n", output_path);
}

//  FEATURE 7: Cropping 
void handle_image_cropping(void) {
        char input_path[MAX_PATH_LENGTH];
        char output_path[MAX_PATH_LENGTH];
        int start_x = 0, start_y = 0, crop_width = 0, crop_height = 0;

        printf("\n ========== Feature 7: Bounding-Box Cropping ========== \n");

        int attempts = DEFAULT_MAX_RETRIES;
        int valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter input .bmp file path", input_path, attempts)) {
                        return;
                }
                attempts--;
                if (!validate_file_extension(input_path, ".bmp")) {
                        printf("ERROR : Input file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *test_file = fopen(input_path, "rb");
                if (!test_file) {
                        printf("ERROR : Failed to open input file '%s'. Check if file exists.\n", input_path);
                        continue;
                }
                fclose(test_file);
                valid = 1;
        }
        if (!valid) {
                return;
        }

        printf("STATUS : Opening input BMP file '%s'...\n", input_path);
        FILE *input_file = fopen(input_path, "rb");
        if (!input_file) {
                perror("Failed to open input BMP");
                return;
        }
        printf("STATUS : Input BMP file opened successfully.\n");

        bitmap_file_header file_header;
        bitmap_info_header info_header;
        printf("STATUS : Reading BMP headers for crop coordinate guidance...\n");
        if (fread(&file_header, sizeof(bitmap_file_header), 1, input_file) != 1 ||
            fread(&info_header, sizeof(bitmap_info_header), 1, input_file) != 1) {
                fprintf(stderr, "ERROR : Failed to read BMP headers.\n");
                fclose(input_file);
                return;
        }
        printf("STATUS : BMP headers read successfully.\n");

        if (file_header.bf_type != BMP_MAGIC_SIGNATURE || info_header.bi_bit_count != TARGET_BIT_COUNT || info_header.bi_compression != 0) {
                fprintf(stderr, "ERROR : Unsupported or invalid 24-bit uncompressed BMP file.\n");
                fclose(input_file);
                return;
        }

        int src_width = info_header.bi_width;
        int src_height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;

        // Display image details prior to coordinate input to guide the user experience
        printf("\nINFO : Source Image Loaded Successfully:\n");
        printf("INFO :   - Total Width  : %d pixels (Valid X range: 0 to %d)\n", src_width, src_width - 1);
        printf("INFO :   - Total Height : %d pixels (Valid Y range: 0 to %d)\n\n", src_height, src_height - 1);

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int("Enter starting X coordinate", &start_x, attempts)) {
                        fclose(input_file);
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
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int("Enter starting Y coordinate", &start_y, attempts)) {
                        fclose(input_file);
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
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int("Enter crop width", &crop_width, attempts)) {
                        fclose(input_file);
                        return;
                }
                attempts--;
                if (crop_width <= 0 || (start_x + crop_width) > src_width) {
                        printf("ERROR : Invalid width. Must be > 0 and start_x + width <= %d (Max available: %d).\n", src_width, src_width - start_x);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                fclose(input_file);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_int("Enter crop height", &crop_height, attempts)) {
                        fclose(input_file);
                        return;
                }
                attempts--;
                if (crop_height <= 0 || (start_y + crop_height) > src_height) {
                        printf("ERROR : Invalid height. Must be > 0 and start_y + height <= %d (Max available: %d).\n", src_height, src_height - start_y);
                        continue;
                }
                valid = 1;
        }
        if (!valid) {
                fclose(input_file);
                return;
        }

        attempts = DEFAULT_MAX_RETRIES;
        valid = 0;
        while (attempts > 0 && !valid) {
                if (!read_strict_string("Enter output .bmp file path", output_path, attempts)) {
                        fclose(input_file);
                        return;
                }
                attempts--;
                if (!validate_file_extension(output_path, ".bmp")) {
                        printf("ERROR : Output file must have a '.bmp' extension.\n");
                        continue;
                }
                FILE *existing = fopen(output_path, "rb");
                if (existing) {
                        fclose(existing);
                        int confirm = get_strict_overwrite_confirmation(output_path);
                        if (confirm == 1) {
                                valid = 1;
                        } else if (confirm == 0) {
                                printf("INFO : Overwrite declined. Please provide a different path.\n");
                                continue;
                        } else {
                                fclose(input_file);
                                return;
                        }
                } else {
                        valid = 1;
                }
        }
        if (!valid) {
                fclose(input_file);
                return;
        }

        long extra_header_bytes_count = (long)file_header.bf_off_bits - 54;
        unsigned char *extra_header_buffer = NULL;
        if (extra_header_bytes_count > 0) {
                printf("STATUS : Reading %ld extra header bytes...\n", extra_header_bytes_count);
                extra_header_buffer = malloc(extra_header_bytes_count);
                if (extra_header_buffer) {
                        if (fread(extra_header_buffer, 1, extra_header_bytes_count, input_file) != (size_t)extra_header_bytes_count) {
                                fprintf(stderr, "WARNING : Failed to read complete extra header bytes.\n");
                        } else {
                                printf("STATUS : Extra header bytes read successfully.\n");
                        }
                }
        }

        int src_row_stride = ((src_width * TARGET_BIT_COUNT + 31) / 32) * 4;
        uint32_t src_pixel_array_size = src_row_stride * src_height;
        
        printf("STATUS : Allocating memory for source pixel array...\n");
        unsigned char *src_pixels = malloc(src_pixel_array_size);
        if (!src_pixels) {
                perror("Memory allocation failed");
                free(extra_header_buffer);
                fclose(input_file);
                return;
        }
        printf("STATUS : Source memory allocated successfully.\n");

        printf("STATUS : Seeking to pixel data offset...\n");
        fseek(input_file, file_header.bf_off_bits, SEEK_SET);
        if (fread(src_pixels, 1, src_pixel_array_size, input_file) != src_pixel_array_size) {
                fprintf(stderr, "WARNING : Pixel array read size mismatch.\n");
        }
        fclose(input_file);
        printf("STATUS : Source pixel array read successfully. Input file closed.\n");

        int dst_row_stride = ((crop_width * TARGET_BIT_COUNT + 31) / 32) * 4;
        uint32_t dst_pixel_array_size = dst_row_stride * crop_height;
        
        printf("STATUS : Allocating memory for cropped destination pixel array...\n");
        unsigned char *dst_pixels = calloc(1, dst_pixel_array_size);
        if (!dst_pixels) {
                perror("Destination memory allocation failed");
                free(src_pixels);
                free(extra_header_buffer);
                return;
        }
        printf("STATUS : Destination memory allocated successfully.\n");

        printf("STATUS : Extracting bounding-box region pixels (%d,%d to %d,%d)...\n", start_x, start_y, start_x + crop_width, start_y + crop_height);
        for (int y = 0; y < crop_height; y++) {
                int src_y = start_y + y;
                unsigned char *src_row_ptr = src_pixels + (src_y * src_row_stride);
                unsigned char *dst_row_ptr = dst_pixels + (y * dst_row_stride);
                for (int x = 0; x < crop_width; x++) {
                        int src_x = start_x + x;
                        unsigned char *src_pixel = src_row_ptr + (src_x * 3);
                        unsigned char *dst_pixel = dst_row_ptr + (x * 3);
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                }
        }
        printf("STATUS : Bounding-box pixel extraction completed successfully.\n");

        info_header.bi_width = crop_width;
        info_header.bi_height = crop_height;
        info_header.bi_size_image = dst_pixel_array_size;
        file_header.bf_size = file_header.bf_off_bits + dst_pixel_array_size;

        printf("STATUS : Creating cropped output BMP file '%s'...\n", output_path);
        FILE *output_file = fopen(output_path, "wb");
        if (!output_file) {
                perror("Failed to create output file");
                free(src_pixels);
                free(dst_pixels);
                free(extra_header_buffer);
                return;
        }
        printf("STATUS : Output file created successfully.\n");

        printf("STATUS : Writing updated headers and cropped pixel buffer to disk...\n");
        fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
        fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
        if (extra_header_bytes_count > 0 && extra_header_buffer) {
                fwrite(extra_header_buffer, 1, extra_header_bytes_count, output_file);
        }
        fwrite(dst_pixels, 1, dst_pixel_array_size, output_file);
        printf("STATUS : Cropped headers and pixel buffer written to disk successfully.\n");

        fclose(output_file);
        free(src_pixels);
        free(dst_pixels);
        free(extra_header_buffer);
        printf("STATUS : Output file closed and allocated buffers freed successfully.\n");

        printf("\n^_^ Cropping completed successfully! Saved to: %s\n", output_path);
}
