#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "client/screenshot.h"
#include <windows.h>

BYTE* SaveScreenshot(size_t* outSize)
{
    int width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top    = GetSystemMetrics(SM_YVIRTUALSCREEN);

    if (width <= 0 || height <= 0) {
        printf("[ERR] Invalid screen dimensions\n");
        return NULL;
    }

    // Create device context
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);

    SelectObject(hMemoryDC, hBitmap);

    // Take screenshot
    if (!BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, left, top, SRCCOPY | CAPTUREBLT)) {
        printf("[ERR] BitBlt failed\n");
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        return NULL;
    }

    // Set up bitmap info & file header
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; 
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    // malloc for binary data
    DWORD pixelDataSize = width * height * 4;
    BYTE* pixels = (BYTE*)malloc(pixelDataSize);

    BITMAPFILEHEADER bfh = {0};
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + pixelDataSize;

    // Convert pixels into binary data
    if (!GetDIBits(hMemoryDC, hBitmap, 0, height, pixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        printf("[ERR] GetDIBits failed\n");
        free(pixels);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        return NULL;
    }

    // malloc for final binary bmp file
    size_t totalSize = bfh.bfSize;
    BYTE* buffer = (BYTE*)malloc(totalSize);

    memcpy(buffer, &bfh, sizeof(bfh));
    memcpy(buffer + sizeof(bfh), &bi, sizeof(bi));
    memcpy(buffer + sizeof(bfh) + sizeof(bi), pixels, pixelDataSize);

    // Clean up
    free(pixels);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    if (outSize)
        *outSize = totalSize;

    return buffer;
}
