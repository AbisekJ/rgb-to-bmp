#include <stdio.h>
#include <stdlib.h>
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
                if (c1 >= 'A' && c1 <= 'Z') {
                        c1 += 32;
                }
                if (c2 >= 'A' && c2 <= 'Z') {
                        c2 += 32;
                }
                if (c1 != c2) {
                        return 0;
                }
        }
        return 1;
}

int calculate_row_stride(int image_width) {
        int row_stride = ((image_width * TARGET_BIT_COUNT + 31) / 32) * 4;
        printf("STATUS : Calculated row stride: %d bytes (including 4-byte boundary padding)\n", row_stride);
        return row_stride;
}

int read_strict_string(const char *prompt, char *out_str, int attempts_left) {
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
                        while ((c = getchar()) != '\n' && c != EOF) {
                                // Flush rest of line
                        }
                        printf("ERROR : Input exceeds maximum character limit. Please try again.\n");
                        attempts_left--;
                        continue;
                }

                buffer[strcspn(buffer, "\r\n")] = '\0';

                if (strlen(buffer) == 0) {
                        printf("ERROR : Input cannot be empty. Please try again.\n");
                        attempts_left--;
                        continue;
                }

                int parsed = sscanf(buffer, "%255s %s", out_str, extra_token);
                if (parsed == 1) {
                        return 1;
                } else {
                        printf("ERROR : Invalid format. Trailing characters or multiple tokens detected.\n");
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
                        while ((c = getchar()) != '\n' && c != EOF) {
                                // Flush rest of line
                        }
                        printf("ERROR : Input too long. Please try again.\n");
                        attempts_left--;
                        continue;
                }

                buffer[strcspn(buffer, "\r\n")] = '\0';

                if (strlen(buffer) == 0) {
                        printf("ERROR : Input cannot be empty. Please try again.\n");
                        attempts_left--;
                        continue;
                }

                int parsed = sscanf(buffer, "%d %s", out_val, extra_token);
                if (parsed == 1) {
                        return 1;
                } else {
                        printf("ERROR : Invalid integer format or trailing characters detected. Please enter a valid integer number.\n");
                        attempts_left--;
                }
        }
        return 0;
}

int get_strict_overwrite_confirmation(const char *filepath) {
        int attempts = DEFAULT_MAX_RETRIES;
        char buffer[256];
        char extra_token[256];

        while (attempts > 0) {
                printf("WARNING : Output file '%s' already exists. Overwrite? (y/n) [%d attempts left]: ", filepath, attempts);
                
                if (!fgets(buffer, sizeof(buffer), stdin)) {
                        attempts--;
                        continue;
                }

                if (!strchr(buffer, '\n')) {
                        int c;
                        while ((c = getchar()) != '\n' && c != EOF) {
                                // Flush rest of line
                        }
                        printf("ERROR : Input too long. Please enter 'y' or 'n'.\n");
                        attempts--;
                        continue;
                }

                buffer[strcspn(buffer, "\r\n")] = '\0';

                if (strlen(buffer) == 0) {
                        printf("ERROR : Input cannot be empty. Please enter 'y' or 'n'.\n");
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
                                printf("ERROR : Invalid response '%c'. Please enter 'y' or 'n' only.\n", choice);
                                attempts--;
                        }
                } else {
                        printf("ERROR : Invalid format or trailing characters. Enter 'y' or 'n' only.\n");
                        attempts--;
                }
        }
        return -1;
}

void print_banner(void) {
        printf("====================================================================\n");
        printf("        Modular Image Processing & BMP Toolkit (v2)\n");
        printf("====================================================================\n");
}

void print_help(const char *program_name) {
        printf("Usage:\n");
        printf("  %s                                -> Launch interactive menu mode\n", program_name);
        printf("  %s --help                         -> Display this help message\n", program_name);
        printf("  %s --version                      -> Display version information\n\n", program_name);
        printf("Description:\n");
        printf("  A basic level toolkit for raw and BMP image processing,\n");
        printf("  BMP header parsing, validation, and geometric transformations.\n");
}

void print_version(void) {
        printf("image_toolkit version 2\n");
}
