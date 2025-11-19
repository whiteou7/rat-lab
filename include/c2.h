#ifndef C2_H
#define C2_H

#define C2_PORT         4444
#define C2_BUF_SIZE     16384
#define C2_EOF_MARK     "<<<EOF>>>"
#define C2_EOF_MARK_LEN 9

// Cross-platform socket abstraction helpers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET sock_t;
    #define CLOSE_SOCK(s) closesocket(s)
    #define SHUT_DOWN()   WSACleanup()
    #define SOCK_INIT()   WSADATA w; WSAStartup(MAKEWORD(2,2), &w)
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    typedef int sock_t;
    #define CLOSE_SOCK(s) close(s)
    #define SHUT_DOWN()
    #define SOCK_INIT()
#endif

#endif