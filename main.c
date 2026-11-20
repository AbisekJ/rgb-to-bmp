#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "bmp_converter.h"

static int parse_menu_choice(const char *line, int *out_choice) {
        const char *ptr = line;
        while (*ptr == ' ' || *ptr == '\t') {
                ptr++;
        }

        if (*ptr == '\0') {
                return 0;
        }

        if (*ptr < '0' || *ptr > '9') {
                return 0;
        }

        int value = 0;
        while (*ptr >= '0' && *ptr <= '9') {
                int digit = *ptr - '0';
                if (value > (INT_MAX - digit) / 10) {
                        return 0;
                }
                value = value * 10 + digit;
                ptr++;
        }

        while (*ptr == ' ' || *ptr == '\t') {
                ptr++;
        }

        if (*ptr != '\0') {
                return 0;
        }

        *out_choice = value;
        return 1;
}

static int pause_for_enter(void) {
        char *pause_line = read_dynamic_input("\nPress Enter to continue...");
        if (pause_line == NULL) {
                printf("\nINFO : End of input reached. Exiting toolkit.\n");
                return 0;
        }
        free(pause_line);
        return 1;
}

// Run interactive operation selection loop
void run_interactive_menu(void) {
        int choice = -1;
        char prompt_buf[128];

        snprintf(prompt_buf, sizeof(prompt_buf), "\n >>>>> Enter your choice [%d-%d]: ",
                 MENU_MIN_CHOICE, MENU_MAX_CHOICE);

        do {
                choice = -1;
                print_banner();
                printf("\n ========== Please select an operation: ========== \n");
                printf("       < 1 > Convert Raw RGB888 to 24-bit BMP\n");
                printf("       < 2 > Parse and Print BMP Headers\n");
                printf("       < 3 > Validate BMP File\n");
                printf("       < 4 > Vertical Flip Transformation\n");
                printf("       < 5 > Mirror / Horizontal Flip Transformation\n");
                printf("       < 6 > Image Rotation (90/180/270 degrees)\n");
                printf("       < 7 > Bounding-Box Cropping\n");
                printf("       < 0 > Exit Toolkit\n");

                char *line = read_dynamic_input(prompt_buf);
                if (line == NULL) {
                        printf("\nINFO : End of input reached. Exiting toolkit.\n");
                        break;
                }

                int parsed = parse_menu_choice(line, &choice);
                free(line);

                if (!parsed) {
                        printf("ERROR : Invalid choice format or trailing characters detected. Try again.\n");
                        if (!pause_for_enter()) {
                                break;
                        }
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
                        case 6:
                                handle_image_rotation();
                                break;
                        case 7:
                                handle_image_cropping();
                                break;
                        case 0:
                                printf("\nExiting toolkit. Goodbye!\n");
                                break;
                        default:
                                printf("ERROR : Invalid choice. Please select between %d and %d.\n",
                                       MENU_MIN_CHOICE, MENU_MAX_CHOICE);
                }

                if (choice != 0 && !pause_for_enter()) {
                        break;
                }
        } while (choice != 0);
}

// Validate load-time arguments and launch menu
int main(int argc, char *argv[]) {
        if (argc > 2) {
                fprintf(stderr, "ERROR : Too many arguments provided.\n");
                fprintf(stderr, "INFO  : Use '%s --help' for usage instructions.\n", argv[0]);
                return 1;
        }

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

                fprintf(stderr, "ERROR : Unrecognized argument '%s'.\n", argv[1]);
                fprintf(stderr, "INFO  : Use '%s --help' for usage instructions.\n", argv[0]);
                return 1;
        }

        run_interactive_menu();
        return 0;
}
