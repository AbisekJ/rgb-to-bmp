#include <stdio.h>
#include <string.h>
#include "bmp_converter.h"

void run_interactive_menu(void) {
        int choice;
        do {
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
                printf("\n >>>>> Enter your choice [0-7]: ");
                
                char buffer[256];
                char extra[256];
                if (!fgets(buffer, sizeof(buffer), stdin)) {
                        break;
                }
                buffer[strcspn(buffer, "\r\n")] = '\0';

                if (sscanf(buffer, "%d %s", &choice, extra) != 1) {
                        printf("ERROR : Invalid choice format or trailing characters detected. Try again.\n");
                        printf("\nPress Enter to continue...");
                        char pause_buf[256];
                        if (!fgets(pause_buf, sizeof(pause_buf), stdin)) {
                                // Handle EOF
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
                                printf("ERROR : Invalid choice. Please select between 0 and 7.\n");
                }
                
                if (choice != 0) {
                        printf("\nPress Enter to continue...");
                        char pause_buf[256];
                        if (!fgets(pause_buf, sizeof(pause_buf), stdin)) {
                                // Handle EOF
                        }
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
