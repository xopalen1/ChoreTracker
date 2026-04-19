#ifndef NET_H
#define NET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#else
#include <sys/socket.h>
typedef int socket_t;
#endif

int net_init(void);
void net_cleanup(void);
void net_close(socket_t sock);

#endif
