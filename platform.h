#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    typedef int socklen_t;

    #define close_socket(s) closesocket(s)

    static inline int platform_init_sockets(void) {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    static inline void platform_cleanup_sockets(void) {
        WSACleanup();
    }

    #define SOCKET_ERROR_VAL INVALID_SOCKET
    typedef SOCKET socket_t;

#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>

    #define close_socket(s) close(s)

    static inline int platform_init_sockets(void) { return 0; }
    static inline void platform_cleanup_sockets(void) {}

    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    typedef int socket_t;

#endif

#endif /* PLATFORM_H */
