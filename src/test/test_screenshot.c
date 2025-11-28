#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include "client/screenshot.h"

int main() {
    printf("Press 1 to take and save a screenshot.\n");
    printf("Press Q to quit.\n\n");

    int screenshotCount = 0;

    while (true) {
        if (GetAsyncKeyState('1') & 0x8000) {
            size_t size = 0;
            BYTE* buffer = SaveScreenshot(&size);

            if (buffer && size > 0) {
                char fileName[64];
                snprintf(fileName, sizeof(fileName), "screenshot_%d.bmp", screenshotCount++);

                FILE* f = fopen(fileName, "wb");
                if (f) {
                    fwrite(buffer, 1, size, f);
                    fclose(f);
                    printf("[OK] Screenshot saved as %s (%zu bytes)\n", fileName, size);
                } else {
                    printf("[ERR] Failed to write file.\n");
                }

                free(buffer);
            }
            else {
                printf("[ERR] Screenshot capture failed.\n");
            }

            // Prevent multiple triggers from one key press
            Sleep(300);
        }
        if (GetAsyncKeyState('Q') & 0x8000) {
            printf("Exiting...\n");
            break;
        }
        Sleep(20);
    }

    return 0;
}
