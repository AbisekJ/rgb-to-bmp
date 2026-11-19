#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "bmp_converter.h"

// Check case-insensitive file extension match
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

// Compute 4-byte aligned row stride for a given bit depth
size_t calculate_row_stride(size_t image_width, uint16_t bit_count) {
        size_t bits_per_row = image_width * (size_t)bit_count;
        size_t row_stride = ((bits_per_row + 31u) / 32u) * 4u;
        printf("STATUS : Calculated row stride: %zu bytes (%u BPP)\n",
               row_stride, (unsigned int)bit_count);
        return row_stride;
}

// Check whether BMP bit depth is one of the common supported depths
int is_supported_bmp_bit_count(uint16_t bit_count) {
        return bit_count == 1u ||
               bit_count == 4u ||
               bit_count == 8u ||
               bit_count == 16u ||
               bit_count == 24u ||
               bit_count == 32u;
}

// Read dynamic input from stdin with no fixed length limit
char *read_dynamic_input(const char *prompt_message) {
        if (prompt_message != NULL) {
                printf("%s", prompt_message);
                fflush(stdout);
        }

        size_t buffer_size = 128;
        size_t current_length = 0;
        char *buffer = malloc(buffer_size);

        if (buffer == NULL) {
                perror("ERROR : Memory allocation failed for input");
                return NULL;
        }

        int character;
        while ((character = getchar()) != '\n' && character != EOF) {
                if (current_length + 1 >= buffer_size) {
                        size_t new_size = buffer_size * 2;
                        char *temp = realloc(buffer, new_size);
                        if (temp == NULL) {
                                perror("ERROR : Memory reallocation failed for input");
                                free(buffer);
                                return NULL;
                        }

                        buffer = temp;
                        buffer_size = new_size;
                }

                buffer[current_length++] = (char)character;
        }

        if (current_length == 0 && character == EOF) {
                free(buffer);
                return NULL;
        }

        if (current_length > 0 && buffer[current_length - 1] == '\r') {
                current_length--;
        }

        buffer[current_length] = '\0';
        return buffer;
}

// Skip leading spaces or tabs and return pointer to first non-space char
static const char *skip_horizontal_space(const char *ptr) {
        while (*ptr == ' ' || *ptr == '\t') {
                ptr++;
        }
        return ptr;
}

// Read single non-empty token dynamically into heap string
int read_strict_string(const char *prompt, char **out_str, size_t attempts_left) {
        while (attempts_left > 0) {
                printf("%s [%zu attempts left]: ", prompt, attempts_left);
                fflush(stdout);

                char *line = read_dynamic_input(NULL);
                if (line == NULL) {
                        printf("ERROR : End of input or read failure.\n");
                        attempts_left--;
                        continue;
                }

                const char *start = skip_horizontal_space(line);
                if (*start == '\0') {
                        printf("ERROR : Input cannot be empty. Please try again.\n");
                        free(line);
                        attempts_left--;
                        continue;
                }

                size_t token_len = 0;
                while (start[token_len] != '\0' && start[token_len] != ' ' && start[token_len] != '\t') {
                        token_len++;
                }

                const char *after_token = skip_horizontal_space(start + token_len);
                if (*after_token != '\0') {
                        printf("ERROR : Invalid format. Trailing characters or multiple tokens detected.\n");
                        free(line);
                        attempts_left--;
                        continue;
                }

                char *token = malloc(token_len + 1);
                if (token == NULL) {
                        perror("ERROR : Memory allocation failed for token");
                        free(line);
                        return 0;
                }

                memcpy(token, start, token_len);
                token[token_len] = '\0';
                free(line);

                *out_str = token;
                return 1;
        }
        return 0;
}

// Read single strict integer dynamically without sscanf
int read_strict_int(const char *prompt, int *out_val, size_t attempts_left) {
        while (attempts_left > 0) {
                printf("%s [%zu attempts left]: ", prompt, attempts_left);
                fflush(stdout);

                char *line = read_dynamic_input(NULL);
                if (line == NULL) {
                        printf("ERROR : End of input or read failure.\n");
                        attempts_left--;
                        continue;
                }

                const char *ptr = skip_horizontal_space(line);
                if (*ptr == '\0') {
                        printf("ERROR : Input cannot be empty. Please try again.\n");
                        free(line);
                        attempts_left--;
                        continue;
                }

                int sign = 1;
                if (*ptr == '+' || *ptr == '-') {
                        if (*ptr == '-') {
                                sign = -1;
                        }
                        ptr++;
                }

                if (*ptr < '0' || *ptr > '9') {
                        printf("ERROR : Invalid integer format. Please enter a valid integer number.\n");
                        free(line);
                        attempts_left--;
                        continue;
                }

                size_t value = 0;
                int overflow = 0;
                size_t max_allowed = (sign == 1) ? (size_t)INT_MAX : ((size_t)INT_MAX + 1u);
                while (*ptr >= '0' && *ptr <= '9') {
                        size_t digit = (size_t)(*ptr - '0');
                        if (value > (max_allowed - digit) / 10u) {
                                overflow = 1;
                                break;
                        }
                        value = value * 10u + digit;
                        ptr++;
                }

                if (overflow) {
                        printf("ERROR : Integer value out of range.\n");
                        free(line);
                        attempts_left--;
                        continue;
                }

                const char *rest = skip_horizontal_space(ptr);
                if (*rest != '\0') {
                        printf("ERROR : Invalid integer format or trailing characters detected.\n");
                        free(line);
                        attempts_left--;
                        continue;
                }

                if (sign == -1 && value == (size_t)INT_MAX + 1u) {
                        *out_val = INT_MIN;
                } else {
                        *out_val = sign * (int)value;
                }
                free(line);
                return 1;
        }
        return 0;
}

// Prompt user strictly for y or n when overwriting an existing file
int get_strict_overwrite_confirmation(const char *filepath) {
        size_t attempts = DEFAULT_MAX_RETRIES;

        while (attempts > 0) {
                printf("WARNING : Output file '%s' already exists. Overwrite? (y/n) [%zu attempts left]: ",
                       filepath, attempts);
                fflush(stdout);

                char *line = read_dynamic_input(NULL);
                if (line == NULL) {
                        printf("ERROR : End of input or read failure.\n");
                        attempts--;
                        continue;
                }

                const char *ptr = skip_horizontal_space(line);
                if (*ptr == '\0') {
                        printf("ERROR : Input cannot be empty. Please enter 'y' or 'n'.\n");
                        free(line);
                        attempts--;
                        continue;
                }

                char choice = *ptr;
                const char *rest = skip_horizontal_space(ptr + 1);
                free(line);

                if (*rest != '\0') {
                        printf("ERROR : Invalid format or trailing characters. Enter 'y' or 'n' only.\n");
                        attempts--;
                        continue;
                }

                if (choice == 'y' || choice == 'Y') {
                        return 1;
                }
                if (choice == 'n' || choice == 'N') {
                        return 0;
                }

                printf("ERROR : Invalid response '%c'. Please enter 'y' or 'n' only.\n", choice);
                attempts--;
        }
        return -1;
}

// Print application banner
void print_banner(void) {
        printf("====================================================================\n");
        printf("        Modular Image Processing & BMP Toolkit (v2)\n");
        printf("====================================================================\n");
}

// Print command-line help text
void print_help(const char *program_name) {
        printf("Usage:\n");
        printf("  %s                                -> Launch interactive menu mode\n", program_name);
        printf("  %s --help                         -> Display this help message\n", program_name);
        printf("  %s --version                      -> Display version information\n\n", program_name);
        printf("Description:\n");
        printf("  A basic level toolkit for raw and BMP image processing,\n");
        printf("  BMP header parsing, validation, and geometric transformations.\n");
}

// Print application version
void print_version(void) {
        printf("image_toolkit version 2\n");
}
