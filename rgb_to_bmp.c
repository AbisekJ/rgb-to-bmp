#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bmp_converter.h"

int validate_file_extension(const char *filename, const char *expected_ext) {
	size_t filename_len = strlen(filename);
	size_t ext_len = strlen(expected_ext);
	
	if (filename_len < ext_len) {
		return 0;
	}

	const char *file_ext = filename + filename_len - ext_len;

	for (size_t i = 0; i < ext_len; i++) {
		char c1 = file_ext[i];
		char c2 = expected_ext[i];
		if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
		if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
		if (c1 != c2) {
			return 0;
		}
	}
	return 1;
}

void print_banner(void) {
	printf("====================================================================\n");
	printf("        Modular Image Processing & BMP Toolkit (v2)\n");
	printf("====================================================================\n");
}

void print_help(const char *program_name) {
	printf("Usage:\n");
	printf("  %s                              -> Launch interactive menu mode\n", program_name);
	printf("  %s --help                       -> Display this help message\n", program_name);
	printf("  %s --version                    -> Display version information\n\n", program_name);
	printf("Description:\n");
	printf("  An interactive low-level systems toolkit for raw image processing,\n");
	printf("  BMP header parsing, validation, and geometric transformations.\n");
}

void print_version(void) {
	printf("image_toolkit version 2\n");
}

int calculate_row_stride(int image_width) {
	int row_stride = ((image_width * 24 + 31) / 32) * 4;
	printf("STATUS : Calculated row stride: %d bytes (including 4-byte boundary padding)\n", row_stride);
	return row_stride;
}

void load_bmp_headers(int image_width, int image_height, uint32_t pixel_array_size, bitmap_file_header *file_header, bitmap_info_header *info_header) {
	uint32_t header_size = sizeof(bitmap_file_header) + sizeof(bitmap_info_header);
	uint32_t total_file_size = header_size + pixel_array_size;
	
	printf("STATUS : Loading values to BMP file header (14 bytes)...\n");
	file_header->bf_type = 0x4D42; // "BM"
	file_header->bf_size = total_file_size;
	file_header->bf_reserved_1 = 0;
	file_header->bf_reserved_2 = 0;
	file_header->bf_off_bits = header_size;
	
	printf("STATUS : Loading values to DIB information header (40 bytes)...\n");
	info_header->bi_size = sizeof(bitmap_info_header);
	info_header->bi_width = image_width;
	info_header->bi_height = image_height;
	info_header->bi_planes = 1;
	info_header->bi_bit_count = 24;
	info_header->bi_compression = 0;
	info_header->bi_size_image = pixel_array_size;
	info_header->bi_x_pels_per_meter = 2835;
	info_header->bi_y_pels_per_meter = 2835;
	info_header->bi_clr_used = 0;
	info_header->bi_clr_important = 0;
}

int convert_rgb888_to_bmp_buffer(const unsigned char *raw_input_buffer, int image_width, int image_height, int row_stride, unsigned char *bmp_pixel_buffer) {
	int abs_height = image_height < 0 ? -image_height : image_height;
	
	if (image_height < 0) {
		printf("STATUS : Mode detected: TOP-DOWN orientation (height is negative).\n");
	} else {
		printf("STATUS : Mode detected: BOTTOM-UP orientation (height is positive).\n");
	}
	printf("STATUS : Starting RGB888 to BGR transformation and respective orientation mapping...\n");
	
	for (int y = 0; y < abs_height; y++) {
		int target_row_index = (image_height < 0) ? y : (abs_height - 1 - y);

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
	
	printf("STATUS : Pixel color channel transformation and row padding completed successfully.\n");
	return 0;
}

// strict input validation

int read_strict_string(const char *prompt, char *out_str, size_t max_len, int attempts_left) {
	char buffer[512];
	char extra_token[256];

	while (attempts_left > 0) {
		printf("%s [%d attempts left]: ", prompt, attempts_left);
		
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			attempts_left--;
			continue;
		}

		if (!strchr(buffer, '\n')) {
			int c;
			while ((c = getchar()) != '\n' && c != EOF);
			printf("[ERROR] Input exceeds maximum character limit. Please try again.\n");
			attempts_left--;
			continue;
		}

		buffer[strcspn(buffer, "\r\n")] = '\0';

		if (strlen(buffer) == 0) {
			printf("[ERROR] Input cannot be empty.\n");
			attempts_left--;
			continue;
		}

		int parsed = sscanf(buffer, "%255s %s", out_str, extra_token);
		if (parsed == 1) {
			return 1;
		} else {
			printf("[ERROR] Invalid format. Trailing characters or multiple tokens detected.\n");
			attempts_left--;
		}
	}
	return 0;
}

int read_strict_int(const char *prompt, int *out_val, int attempts_left) {
	char buffer[256];
	char extra_token[256];

	while (attempts_left > 0) {
		printf("%s [%d attempts left]: ", prompt, attempts_left);
		
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			attempts_left--;
			continue;
		}

		if (!strchr(buffer, '\n')) {
			int c;
			while ((c = getchar()) != '\n' && c != EOF);
			printf("[ERROR] Input too long. Please try again.\n");
			attempts_left--;
			continue;
		}

		buffer[strcspn(buffer, "\r\n")] = '\0';

		if (strlen(buffer) == 0) {
			printf("[ERROR] Input cannot be empty.\n");
			attempts_left--;
			continue;
		}

		int parsed = sscanf(buffer, "%d %s", out_val, extra_token);
		if (parsed == 1) {
			return 1;
		} else {
			printf("[ERROR] Invalid integer format or trailing characters detected.\n");
			attempts_left--;
		}
	}
	return 0;
}

int get_strict_overwrite_confirmation(const char *filepath) {
	int attempts = 3;
	char buffer[256];
	char extra_token[256];

	while (attempts > 0) {
		printf("[WARNING] Output file '%s' already exists. Overwrite? (y/n) [%d attempts left]: ", filepath, attempts);
		
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			attempts--;
			continue;
		}

		if (!strchr(buffer, '\n')) {
			int c;
			while ((c = getchar()) != '\n' && c != EOF);
			printf("[ERROR] Input too long. Please enter 'y' or 'n'.\n");
			attempts--;
			continue;
		}

		buffer[strcspn(buffer, "\r\n")] = '\0';

		if (strlen(buffer) == 0) {
			printf("[ERROR] Input cannot be empty. Please enter 'y' or 'n'.\n");
			attempts--;
			continue;
		}

		char choice;
		int parsed = sscanf(buffer, " %c %s", &choice, extra_token);
		if (parsed == 1) {
			if (choice == 'y' || choice == 'Y') {
				return 1;
			} else if (choice == 'n' || choice == 'N') {
				return 0;
			} else {
				printf("[ERROR] Invalid response '%c'. Please enter 'y' or 'n' only.\n", choice);
				attempts--;
			}
		} else {
			printf("[ERROR] Invalid format or trailing characters. Enter 'y' or 'n' only.\n");
			attempts--;
		}
	}
	return -1;
}

// feature 1 - rgb to bmp
void handle_rgb_to_bmp_conversion(void) {
	char input_path[256];
	char output_path[256];
	int image_width = 0;
	int image_height = 0;

	printf("\n--- Feature 1: Raw RGB888 to 24-bit BMP Conversion ---\n");

	int valid_input = 0;
	int attempts = 3;
	while (attempts > 0 && !valid_input) {
		if (!read_strict_string("Enter input .rgb file path", input_path, sizeof(input_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(input_path, ".rgb")) {
			printf("[ERROR] Input file must have a '.rgb' extension.\n");
			continue;
		}

		FILE *test_file = fopen(input_path, "rb");
		if (!test_file) {
			printf("[ERROR] Failed to open input file '%s'. Check if file exists.\n", input_path);
			continue;
		}
		fclose(test_file);
		valid_input = 1;
	}
	if (!valid_input) {
		printf("[ABORT] Maximum retry attempts reached for input file. Returning to menu.\n");
		return;
	}

	int valid_width = 0;
	attempts = 3;
	while (attempts > 0 && !valid_width) {
		if (!read_strict_int("Enter image width in pixels (must be > 0)", &image_width, attempts)) {
			break;
		}
		attempts--;

		if (image_width <= 0) {
			printf("[ERROR] Width (%d) must be strictly positive.\n", image_width);
			continue;
		}
		valid_width = 1;
	}
	if (!valid_width) {
		printf("[ABORT] Maximum retry attempts reached for image width. Returning to menu.\n");
		return;
	}

	int valid_height = 0;
	attempts = 3;
	while (attempts > 0 && !valid_height) {
		if (!read_strict_int("Enter image height (+ for bottom-up, - for top-down, != 0)", &image_height, attempts)) {
			break;
		}
		attempts--;

		if (image_height == 0) {
			printf("[ERROR] Height cannot be zero.\n");
			continue;
		}
		valid_height = 1;
	}
	if (!valid_height) {
		printf("[ABORT] Maximum retry attempts reached for image height. Returning to menu.\n");
		return;
	}

	int valid_output = 0;
	attempts = 3;
	while (attempts > 0 && !valid_output) {
		if (!read_strict_string("Enter output .bmp file path", output_path, sizeof(output_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(output_path, ".bmp")) {
			printf("[ERROR] Output file must have a '.bmp' extension.\n");
			continue;
		}

		FILE *existing_file = fopen(output_path, "rb");
		if (existing_file) {
			fclose(existing_file);
			int confirm = get_strict_overwrite_confirmation(output_path);
			if (confirm == 1) {
				valid_output = 1;
			} else if (confirm == 0) {
				printf("[INFO] Overwrite declined. Please provide a different output file path.\n");
				continue;
			} else {
				printf("[ABORT] Maximum retry attempts reached for overwrite confirmation. Returning to menu.\n");
				return;
			}
		} else {
			valid_output = 1;
		}
	}
	if (!valid_output) {
		printf("[ABORT] Maximum retry attempts reached for output file. Returning to menu.\n");
		return;
	}

	int abs_height = image_height < 0 ? -image_height : image_height;
	printf("STATUS : Validating image dimension parameters...\n");
	printf("STATUS : Dimensions validated: Width = %d, Height = %d (Absolute = %d).\n", 
		image_width, image_height, abs_height);

	printf("\n[*] Execution Configuration:\n");
	printf("    - Input File:  %s\n", input_path);
	printf("    - Output File: %s\n\n", output_path);

	printf("STATUS : Opening raw input file '%s' in binary read mode...\n", input_path);
	FILE *input_file = fopen(input_path, "rb");
	if (!input_file) {
		perror("Failed to open raw input file");
		return;
	}
	printf("STATUS : Input file opened successfully!\n");

	printf("STATUS : Verifying input file size on disk...\n");
	if (fseek(input_file, 0, SEEK_END) != 0) {
		perror("Failed to seek input file");
		fclose(input_file);
		return;
	}

	long actual_file_size = ftell(input_file);
	if (actual_file_size < 0) {
		perror("Failed to get input file size");
		fclose(input_file);
		return;
	}
	rewind(input_file);

	size_t expected_raw_size = (size_t)image_width * abs_height * 3;
	if ((size_t)actual_file_size != expected_raw_size) {
		fprintf(stderr, "Error: Input file size mismatch!\n");
		fprintf(stderr, "       Expected: %zu bytes (%dx%d pixels * 3 channels)\n", 
			expected_raw_size, image_width, abs_height);
		fprintf(stderr, "       Actual:   %ld bytes on disk\n", actual_file_size);
		fclose(input_file);
		return;
	}
	printf("STATUS : File size verified: %ld bytes matches expected RGB888 payload.\n", actual_file_size);

	uint32_t row_stride = calculate_row_stride(image_width);
	uint32_t pixel_array_size = row_stride * abs_height;

	bitmap_file_header file_header;
	bitmap_info_header info_header;
	
	load_bmp_headers(image_width, image_height, pixel_array_size, &file_header, &info_header);

	printf("STATUS : Creating output BMP file '%s' in binary write mode...\n", output_path);
	FILE *output_file = fopen(output_path, "wb");
	if (!output_file) {
		perror("Failed to create BMP output file");
		fclose(input_file);
		return;
	}
	printf("STATUS : Output file stream established.\n");

	printf("STATUS : Writing headers (file header + DIB info header) to disk (54 bytes total)...\n");
	fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
	fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);
	printf("STATUS : Headers written successfully!\n");

	printf("STATUS : Allocating heap memory for raw input buffer (%zu bytes)...\n", expected_raw_size);
	unsigned char *raw_input_buffer = malloc(expected_raw_size);
	if (!raw_input_buffer) {
		perror("Failed to allocate memory for raw input buffer");
		fclose(input_file);
		fclose(output_file);
		return;
	}

	printf("STATUS : Reading raw pixel stream from disk into memory buffer...\n");
	size_t bytes_read = fread(raw_input_buffer, 1, expected_raw_size, input_file);
	if (bytes_read != expected_raw_size) {
		fprintf(stderr, "Error: Read %zu bytes, but expected %zu bytes.\n", bytes_read, expected_raw_size);
		free(raw_input_buffer);
		fclose(input_file);
		fclose(output_file);
		return;
	}
	printf("STATUS : Read operation complete (%zu bytes loaded).\n", bytes_read);

	printf("STATUS : Allocating heap memory for destination BMP pixel array (%u bytes)...\n", pixel_array_size);
	unsigned char *bmp_pixel_buffer = calloc(1, pixel_array_size); 
	if (!bmp_pixel_buffer) {
		perror("Failed to allocate memory for BMP pixel buffer");
		free(raw_input_buffer);
		fclose(input_file);
		fclose(output_file);
		return;
	}
	printf("STATUS : Destination pixel buffer allocated and zero-initialized.\n");

	convert_rgb888_to_bmp_buffer(raw_input_buffer, image_width, image_height, row_stride, bmp_pixel_buffer);

	printf("STATUS : Writing finalized BMP pixel array to disk (%u bytes)...\n", pixel_array_size);
	size_t pixels_written = fwrite(bmp_pixel_buffer, 1, pixel_array_size, output_file);
	if (pixels_written != pixel_array_size) {
		fprintf(stderr, "Error: Wrote %zu bytes, but expected %u bytes.\n", pixels_written, pixel_array_size);
	} else {
		printf("STATUS : Pixel array successfully written to disk ^_^ !!!\n");
	}

	printf("STATUS : Releasing allocated heap memory and closing file streams...\n");
	free(raw_input_buffer);
	free(bmp_pixel_buffer);
	fclose(input_file);
	fclose(output_file);
	printf("STATUS : Cleanup completed successfully.\n");

	printf("\n^_^ Conversion successfully finished! Output saved to: %s ^_^\n", output_path);
}

// feature 2 - print bmp header
void handle_bmp_header_parsing(void) {
	char bmp_path[256];
	int attempts = 3;
	int valid = 0;

	printf("\n--- Feature 2: Parse and Pretty Print BMP Headers ---\n");

	while (attempts > 0 && !valid) {
		if (!read_strict_string("Enter existing .bmp file path", bmp_path, sizeof(bmp_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(bmp_path, ".bmp")) {
			printf("[ERROR] File must have a '.bmp' extension.\n");
			continue;
		}

		FILE *test_file = fopen(bmp_path, "rb");
		if (!test_file) {
			printf("[ERROR] Failed to open file '%s'. Check if file exists.\n", bmp_path);
			continue;
		}
		fclose(test_file);
		valid = 1;
	}

	if (!valid) {
		printf("[ABORT] Maximum retry attempts reached. Returning to menu.\n");
		return;
	}

	printf("STATUS : Opening BMP file '%s' in binary read mode...\n", bmp_path);
	FILE *bmp_file = fopen(bmp_path, "rb");
	if (!bmp_file) {
		perror("Failed to open BMP file");
		return;
	}

	bitmap_file_header file_header;
	bitmap_info_header info_header;

	printf("STATUS : Reading 14-byte bitmap_file_header from disk...\n");
	if (fread(&file_header, sizeof(bitmap_file_header), 1, bmp_file) != 1) {
		perror("Failed to read bitmap_file_header");
		fclose(bmp_file);
		return;
	}

	printf("STATUS : Reading 40-byte bitmap_info_header from disk...\n");
	if (fread(&info_header, sizeof(bitmap_info_header), 1, bmp_file) != 1) {
		perror("Failed to read bitmap_info_header");
		fclose(bmp_file);
		return;
	}

	fclose(bmp_file);
	printf("STATUS : Header structures extracted successfully.\n");

	printf("\n====================================================================\n");
	printf("                 BMP FILE METADATA INSPECTION REPORT                \n");
	printf("====================================================================\n");
	
	printf("\n[1] bitmap_file_header (14 Bytes):\n");
	printf("    - bf_type        : 0x%04X ('%c%c') [Magic Signature] %s\n", 
		file_header.bf_type, 
		(char)(file_header.bf_type & 0xFF), 
		(char)(file_header.bf_type >> 8),
		file_header.bf_type == 0x4D42 ? "[VALID]" : "[INVALID]");
	printf("    - bf_size        : %u bytes (%u KB) [Total File Size]\n", file_header.bf_size, file_header.bf_size / 1024);
	printf("    - bf_reserved_1  : %u [Reserved Field 1]\n", file_header.bf_reserved_1);
	printf("    - bf_reserved_2  : %u [Reserved Field 2]\n", file_header.bf_reserved_2);
	printf("    - bf_off_bits    : %u bytes [Pixel Data Offset]\n", file_header.bf_off_bits);

	printf("\n[2] bitmap_info_header (40 Bytes - DIB Header):\n");
	printf("    - bi_size          : %u bytes [Header Struct Size]\n", info_header.bi_size);
	printf("    - bi_width         : %u pixels [Image Width]\n", info_header.bi_width);
	printf("    - bi_height        : %d pixels [Image Height & Orientation] (%s)\n", 
		info_header.bi_height, 
		info_header.bi_height < 0 ? "Top-Down" : "Bottom-Up");
	printf("    - bi_planes        : %u [Color Planes] (Must be 1)\n", info_header.bi_planes);
	printf("    - bi_bit_count     : %u BPP [Bits Per Pixel / Color Depth]\n", info_header.bi_bit_count);
	printf("    - bi_compression   : %u [Compression Scheme] (%s)\n", 
		info_header.bi_compression, 
		info_header.bi_compression == 0 ? "BI_RGB (Uncompressed)" : "Compressed/Bitfields");
	printf("    - bi_size_image    : %u bytes [Pixel Array Size]\n", info_header.bi_size_image);
	printf("    - bi_x_pels_per_meter: %u [Horizontal Print Resolution] (~%d DPI)\n", 
		info_header.bi_x_pels_per_meter, (int)(info_header.bi_x_pels_per_meter * 0.0254 + 0.5));
	printf("    - bi_y_pels_per_meter: %u [Vertical Print Resolution] (~%d DPI)\n", 
		info_header.bi_y_pels_per_meter, (int)(info_header.bi_y_pels_per_meter * 0.0254 + 0.5));
	printf("    - bi_clr_used      : %u [Palette Colors Used]\n", info_header.bi_clr_used);
	printf("    - bi_clr_important : %u [Important Colors Count]\n", info_header.bi_clr_important);
	printf("====================================================================\n");
}

// feature 3 - validate bmp header
void handle_bmp_validation(void) {
	char bmp_path[256];
	int attempts = 3;
	int valid = 0;

	printf("\n--- Feature 3: Comprehensive BMP File Validator ---\n");

	while (attempts > 0 && !valid) {
		if (!read_strict_string("Enter .bmp file path to validate", bmp_path, sizeof(bmp_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(bmp_path, ".bmp")) {
			printf("[ERROR] File must have a '.bmp' extension.\n");
			continue;
		}

		FILE *test_file = fopen(bmp_path, "rb");
		if (!test_file) {
			printf("[ERROR] Failed to open file '%s'. Check if file exists.\n", bmp_path);
			continue;
		}
		fclose(test_file);
		valid = 1;
	}

	if (!valid) {
		printf("[ABORT] Maximum retry attempts reached. Returning to menu.\n");
		return;
	}

	printf("STATUS : Opening BMP file '%s' for validation checks...\n", bmp_path);
	FILE *bmp_file = fopen(bmp_path, "rb");
	if (!bmp_file) {
		perror("Failed to open BMP file");
		return;
	}

	// Get actual file size on disk
	if (fseek(bmp_file, 0, SEEK_END) != 0) {
		perror("Failed to seek file");
		fclose(bmp_file);
		return;
	}
	long disk_file_size = ftell(bmp_file);
	rewind(bmp_file);

	bitmap_file_header file_header;
	bitmap_info_header info_header;

	if (fread(&file_header, sizeof(bitmap_file_header), 1, bmp_file) != 1 ||
	    fread(&info_header, sizeof(bitmap_info_header), 1, bmp_file) != 1) {
		fprintf(stderr, "[ERROR] File is too small to contain valid BMP headers (needs at least 54 bytes).\n");
		fclose(bmp_file);
		return;
	}

	fclose(bmp_file);
	printf("STATUS : Running rigorous structural and payload integrity checks...\n");

	int error_count = 0;
	int warning_count = 0;

	printf("\n====================================================================\n");
	printf("                  BMP STRUCTURAL VALIDATION REPORT                  \n");
	printf("====================================================================\n");

	// Check 1: Magic Signature ('BM')
	if (file_header.bf_type == 0x4D42) {
		printf("[PASS] Magic Signature (bf_type): 0x%04X ('BM')\n", file_header.bf_type);
	} else {
		printf("[FAIL] Magic Signature (bf_type): 0x%04X (Expected 0x%04X / 'BM')\n", file_header.bf_type, 0x4D42);
		error_count++;
	}

	// Check 2: DIB Header Size (Must be 40 for Windows BITMAPINFOHEADER)
	if (info_header.bi_size == 40) {
		printf("[PASS] DIB Header Size (bi_size): %u bytes (Standard Windows DIB)\n", info_header.bi_size);
	} else {
		printf("[FAIL] DIB Header Size (bi_size): %u bytes (Expected 40 bytes)\n", info_header.bi_size);
		error_count++;
	}

	// Check 3: Color Depth (Must be 24 BPP for our True Color pipeline)
	if (info_header.bi_bit_count == 24) {
		printf("[PASS] Color Depth (bi_bit_count): 24 BPP (True Color RGB)\n");
	} else {
		printf("[FAIL] Color Depth (bi_bit_count): %u BPP (Toolkit expects 24 BPP)\n", info_header.bi_bit_count);
		error_count++;
	}

	// Check 4: Compression Type (Must be 0 / BI_RGB)
	if (info_header.bi_compression == 0) {
		printf("[PASS] Compression Scheme (bi_compression): 0 (BI_RGB Uncompressed)\n");
	} else {
		printf("[FAIL] Compression Scheme (bi_compression): %u (Toolkit expects 0 / BI_RGB)\n", info_header.bi_compression);
		error_count++;
	}

	// Check 5: Color Planes (Must be 1)
	if (info_header.bi_planes == 1) {
		printf("[PASS] Color Planes (bi_planes): 1\n");
	} else {
		printf("[FAIL] Color Planes (bi_planes): %u (Expected 1)\n", info_header.bi_planes);
		error_count++;
	}

	// Check 6: Pixel Data Offset (Must be 54 for uncompressed 24-bit BMP without palette)
	if (file_header.bf_off_bits == 54) {
		printf("[PASS] Pixel Data Offset (bf_off_bits): 54 bytes\n");
	} else {
		printf("[WARN] Pixel Data Offset (bf_off_bits): %u bytes (Expected 54 bytes)\n", file_header.bf_off_bits);
		warning_count++;
	}

	// Check 7: Image Dimensions Validation
	int abs_height = info_header.bi_height < 0 ? -info_header.bi_height : info_header.bi_height;
	if (info_header.bi_width > 0 && info_header.bi_height != 0) {
		printf("[PASS] Dimensions Validation: Width = %u, Height = %d (Absolute = %d)\n", 
			info_header.bi_width, info_header.bi_height, abs_height);
	} else {
		printf("[FAIL] Dimensions Validation: Width or Height are invalid (%u x %d)\n", 
			info_header.bi_width, info_header.bi_height);
		error_count++;
	}

	// Check 8: File Size & Payload Consistency Calculation
	int row_stride = ((info_header.bi_width * 24 + 31) / 32) * 4;
	uint32_t expected_pixel_array_size = row_stride * abs_height;
	uint32_t expected_total_size = 54 + expected_pixel_array_size;

	printf("    * Calculated Stride      : %d bytes/row\n", row_stride);
	printf("    * Expected Pixel Array   : %u bytes\n", expected_pixel_array_size);
	printf("    * Declared File Size     : %u bytes (bf_size)\n", file_header.bf_size);
	printf("    * Actual Size on Disk    : %ld bytes\n", disk_file_size);

	if (file_header.bf_size == expected_total_size) {
		printf("[PASS] Header File Size Consistency Check passed!\n");
	} else {
		printf("[FAIL] Header File Size Mismatch! Declared bf_size (%u) != Expected (%u)\n", 
			file_header.bf_size, expected_total_size);
		error_count++;
	}

	if ((long)file_header.bf_size == disk_file_size) {
		printf("[PASS] Disk File Size Consistency Check passed!\n");
	} else {
		printf("[FAIL] Disk File Size Mismatch! Declared size (%u) != Actual disk bytes (%ld)\n", 
			file_header.bf_size, disk_file_size);
		error_count++;
	}

	printf("--------------------------------------------------------------------\n");
	if (error_count == 0) {
		printf("RESULT: [SUCCESS] BMP file is fully valid and ready for processing!\n");
	} else {
		printf("RESULT: [FAILURE] BMP file contains %d critical error(s) and is corrupted/invalid.\n", error_count);
	}
	printf("====================================================================\n");
}

// feature 4 - vertical flip
void handle_vertical_flip(void) {
	char input_path[256];
	char output_path[256];

	printf("\n--- Feature 4: Vertical Flip Transformation ---\n");

	// 1. Input Path Validation
	int attempts = 3;
	int valid = 0;
	while (attempts > 0 && !valid) {
		if (!read_strict_string("Enter input .bmp file path", input_path, sizeof(input_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(input_path, ".bmp")) {
			printf("[ERROR] Input file must have a '.bmp' extension.\n");
			continue;
		}

		FILE *test_file = fopen(input_path, "rb");
		if (!test_file) {
			printf("[ERROR] Failed to open input file '%s'.\n", input_path);
			continue;
		}
		fclose(test_file);
		valid = 1;
	}
	if (!valid) return;

	// 2. Output Path & Overwrite Validation
	attempts = 3;
	valid = 0;
	while (attempts > 0 && !valid) {
		if (!read_strict_string("Enter output .bmp file path", output_path, sizeof(output_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(output_path, ".bmp")) {
			printf("[ERROR] Output file must have a '.bmp' extension.\n");
			continue;
		}

		FILE *existing = fopen(output_path, "rb");
		if (existing) {
			fclose(existing);
			int confirm = get_strict_overwrite_confirmation(output_path);
			if (confirm == 1) {
				valid = 1;
			} else if (confirm == 0) {
				continue;
			} else {
				return;
			}
		} else {
			valid = 1;
		}
	}
	if (!valid) return;

	printf("STATUS : Opening input BMP file '%s' in binary read mode...\n", input_path);
	FILE *input_file = fopen(input_path, "rb");
	if (!input_file) {
		perror("Failed to open input BMP");
		return;
	}

	// Get exact file size on disk
	fseek(input_file, 0, SEEK_END);
	long disk_file_size = ftell(input_file);
	rewind(input_file);

	bitmap_file_header file_header;
	bitmap_info_header info_header;

	printf("STATUS : Reading standard 54-byte headers...\n");
	fread(&file_header, sizeof(bitmap_file_header), 1, input_file);
	fread(&info_header, sizeof(bitmap_info_header), 1, input_file);

	if (file_header.bf_type != 0x4D42 || info_header.bi_bit_count != 24 || info_header.bi_compression != 0) {
		fprintf(stderr, "[ERROR] Unsupported or invalid 24-bit uncompressed BMP file.\n");
		fclose(input_file);
		return;
	}

	// Calculate extra header bytes between 54 bytes and bf_off_bits (e.g., bitmasks, profiles)
	long extra_header_bytes_count = (long)file_header.bf_off_bits - 54;
	unsigned char *extra_header_buffer = NULL;

	if (extra_header_bytes_count > 0) {
		printf("STATUS : Detected %ld extra header bytes before pixel array (bf_off_bits = %u). Reading...\n", 
			extra_header_bytes_count, file_header.bf_off_bits);
		extra_header_buffer = malloc(extra_header_bytes_count);
		if (!extra_header_buffer) {
			perror("Extra header memory allocation failed");
			fclose(input_file);
			return;
		}
		fread(extra_header_buffer, 1, extra_header_bytes_count, input_file);
	}

	int width = info_header.bi_width;
	int raw_height = info_header.bi_height;
	int height = raw_height < 0 ? -raw_height : raw_height;
	
	int row_stride = ((width * 24 + 31) / 32) * 4;
	uint32_t pixel_array_size = row_stride * height;

	printf("STATUS : Stride: %d bytes/row (Width: %d, Height: %d)\n", row_stride, width, height);

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

	// Seek to actual pixel offset defined by bf_off_bits
	fseek(input_file, file_header.bf_off_bits, SEEK_SET);
	fread(src_pixels, 1, pixel_array_size, input_file);
	fclose(input_file);

	printf("STATUS : Performing precise vertical flip with stride packing/unpacking...\n");
	for (int y = 0; y < height; y++) {
		int src_row = y;
		int dst_row = height - 1 - y;

		unsigned char *src_row_ptr = src_pixels + (src_row * row_stride);
		unsigned char *dst_row_ptr = dst_pixels + (dst_row * row_stride);

		for (int x = 0; x < width; x++) {
			int pixel_offset = x * 3;
			dst_row_ptr[pixel_offset + 0] = src_row_ptr[pixel_offset + 0]; // Blue
			dst_row_ptr[pixel_offset + 1] = src_row_ptr[pixel_offset + 1]; // Green
			dst_row_ptr[pixel_offset + 2] = src_row_ptr[pixel_offset + 2]; // Red
		}
	}

	info_header.bi_height = height; // Normalize orientation

	printf("STATUS : Creating output BMP file '%s'...\n", output_path);
	FILE *output_file = fopen(output_path, "wb");
	if (!output_file) {
		perror("Failed to create output file");
		free(extra_header_buffer);
		free(src_pixels);
		free(dst_pixels);
		return;
	}

	// Write base headers
	fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
	fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);

	// Write preserved extra header bytes so bf_off_bits alignment matches perfectly
	if (extra_header_bytes_count > 0 && extra_header_buffer != NULL) {
		fwrite(extra_header_buffer, 1, extra_header_bytes_count, output_file);
	}

	// Write pixel array
	fwrite(dst_pixels, 1, pixel_array_size, output_file);

	fclose(output_file);
	free(extra_header_buffer);
	free(src_pixels);
	free(dst_pixels);
	printf("STATUS : Cleanup completed successfully.\n");

	printf("\n^_^ Vertical flip completed successfully! Saved to: %s\n ^_^", output_path);
}

// feature 5 - horizontal flip
void handle_mirror_image(void) {
	char input_path[256];
	char output_path[256];

	printf("\n--- Feature 5: Mirror / Horizontal Flip Transformation ---\n");

	// 1. Input Path Validation
	int attempts = 3;
	int valid = 0;
	while (attempts > 0 && !valid) {
		if (!read_strict_string("Enter input .bmp file path", input_path, sizeof(input_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(input_path, ".bmp")) {
			printf("[ERROR] Input file must have a '.bmp' extension.\n");
			continue;
		}

		FILE *test_file = fopen(input_path, "rb");
		if (!test_file) {
			printf("[ERROR] Failed to open input file '%s'.\n", input_path);
			continue;
		}
		fclose(test_file);
		valid = 1;
	}
	if (!valid) return;

	// 2. Output Path & Overwrite Validation
	attempts = 3;
	valid = 0;
	while (attempts > 0 && !valid) {
		if (!read_strict_string("Enter output .bmp file path", output_path, sizeof(output_path), attempts)) {
			break;
		}
		attempts--;

		if (!validate_file_extension(output_path, ".bmp")) {
			printf("[ERROR] Output file must have a '.bmp' extension.\n");
			continue;
		}

		FILE *existing = fopen(output_path, "rb");
		if (existing) {
			fclose(existing);
			int confirm = get_strict_overwrite_confirmation(output_path);
			if (confirm == 1) {
				valid = 1;
			} else if (confirm == 0) {
				continue;
			} else {
				return;
			}
		} else {
			valid = 1;
		}
	}
	if (!valid) return;

	printf("STATUS : Opening input BMP file '%s' in binary read mode...\n", input_path);
	FILE *input_file = fopen(input_path, "rb");
	if (!input_file) {
		perror("Failed to open input BMP");
		return;
	}

	// Get exact file size on disk
	fseek(input_file, 0, SEEK_END);
	long disk_file_size = ftell(input_file);
	rewind(input_file);

	bitmap_file_header file_header;
	bitmap_info_header info_header;

	printf("STATUS : Reading standard 54-byte headers...\n");
	fread(&file_header, sizeof(bitmap_file_header), 1, input_file);
	fread(&info_header, sizeof(bitmap_info_header), 1, input_file);

	if (file_header.bf_type != 0x4D42 || info_header.bi_bit_count != 24 || info_header.bi_compression != 0) {
		fprintf(stderr, "[ERROR] Unsupported or invalid 24-bit uncompressed BMP file.\n");
		fclose(input_file);
		return;
	}

	// Preserve extra header bytes (e.g., bitmasks/profiles if bf_off_bits > 54)
	long extra_header_bytes_count = (long)file_header.bf_off_bits - 54;
	unsigned char *extra_header_buffer = NULL;

	if (extra_header_bytes_count > 0) {
		printf("STATUS : Detected %ld extra header bytes before pixel array. Reading...\n", extra_header_bytes_count);
		extra_header_buffer = malloc(extra_header_bytes_count);
		if (!extra_header_buffer) {
			perror("Extra header memory allocation failed");
			fclose(input_file);
			return;
		}
		fread(extra_header_buffer, 1, extra_header_bytes_count, input_file);
	}

	int width = info_header.bi_width;
	int raw_height = info_header.bi_height;
	int height = raw_height < 0 ? -raw_height : raw_height;
	
	int row_stride = ((width * 24 + 31) / 32) * 4;
	uint32_t pixel_array_size = row_stride * height;

	printf("STATUS : Stride: %d bytes/row (Width: %d, Height: %d)\n", row_stride, width, height);

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

	fseek(input_file, file_header.bf_off_bits, SEEK_SET);
	fread(src_pixels, 1, pixel_array_size, input_file);
	fclose(input_file);

	printf("STATUS : Performing precise horizontal pixel mirroring across %d scanlines...\n", height);
	for (int y = 0; y < height; y++) {
		unsigned char *src_row_ptr = src_pixels + (y * row_stride);
		unsigned char *dst_row_ptr = dst_pixels + (y * row_stride);

		for (int x = 0; x < width; x++) {
			int src_pixel_offset = x * 3;
			int dst_pixel_offset = (width - 1 - x) * 3;

			dst_row_ptr[dst_pixel_offset + 0] = src_row_ptr[src_pixel_offset + 0]; // Blue
			dst_row_ptr[dst_pixel_offset + 1] = src_row_ptr[src_pixel_offset + 1]; // Green
			dst_row_ptr[dst_pixel_offset + 2] = src_row_ptr[src_pixel_offset + 2]; // Red
		}
	}

	printf("STATUS : Creating output BMP file '%s'...\n", output_path);
	FILE *output_file = fopen(output_path, "wb");
	if (!output_file) {
		perror("Failed to create output file");
		free(extra_header_buffer);
		free(src_pixels);
		free(dst_pixels);
		return;
	}

	// Write base headers
	fwrite(&file_header, sizeof(bitmap_file_header), 1, output_file);
	fwrite(&info_header, sizeof(bitmap_info_header), 1, output_file);

	// Write preserved extra header bytes
	if (extra_header_bytes_count > 0 && extra_header_buffer != NULL) {
		fwrite(extra_header_buffer, 1, extra_header_bytes_count, output_file);
	}

	// Write mirrored pixel array
	fwrite(dst_pixels, 1, pixel_array_size, output_file);

	fclose(output_file);
	free(extra_header_buffer);
	free(src_pixels);
	free(dst_pixels);
	printf("STATUS : Cleanup completed successfully.\n");

	printf("\n^_^ Mirror (horizontal flip) completed successfully! Saved to: %s ^_^\n", output_path);
}


// menu 
void run_interactive_menu(void) {
	int choice;
	do {
		print_banner();
		printf("\nPlease select an operation:\n");
		printf("  [1] Convert Raw RGB888 to 24-bit BMP\n");
		printf("  [2] Parse and Print BMP Headers\n");
		printf("  [3] Validate BMP File \n");
		printf("  [4] Vertical Flip \n");
		printf("  [5] Mirror Image \n");
		printf("  [6] Rotate / Crop Image (Coming in Stage 5)\n");
		printf("  [0] Exit Toolkit\n");
		printf("\nEnter your choice [0-5]: ");
		
		char buffer[256];
		char extra[256];
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			break;
		}
		buffer[strcspn(buffer, "\r\n")] = '\0';

		if (sscanf(buffer, "%d %s", &choice, extra) != 1) {
			printf("[ERROR] Invalid choice format or trailing characters detected. Try again.\n");
			printf("\nPress Enter to continue...");
			char pause_buf[256];
			fgets(pause_buf, sizeof(pause_buf), stdin);
			continue;
		}

		switch (choice) {
			case 1:
				handle_rgb_to_bmp_conversion();
				break;
			case 2:
				handle_bmp_header_parsing();
				break;
			case 3:
				handle_bmp_validation();
				break;
			case 4:
				handle_vertical_flip(); 
				break;
			case 5:
				handle_mirror_image();
				break;
			case 0:
				printf("\nExiting toolkit. Goodbye!\n");
				break;
			default:
				printf("[ERROR] Invalid choice. Please select between 0 and 5.\n");
		}
		
		if (choice != 0) {
			printf("\nPress Enter to continue...");
			char pause_buf[256];
			fgets(pause_buf, sizeof(pause_buf), stdin);
		}
	} while (choice != 0);
}

int main(int argc, char *argv[]) {
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

	run_interactive_menu();
	return 0;
}
