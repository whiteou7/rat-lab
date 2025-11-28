#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#ifdef _WIN32
    #include <windows.h>
#else
    typedef unsigned char BYTE;
#endif

BYTE* SaveScreenshot(size_t* outSize);

#endif