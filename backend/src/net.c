#include "net.h"

#ifdef _WIN32
int net_init(void) {
  WSADATA wsa_data;
  return WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

void net_cleanup(void) {
  WSACleanup();
}

void net_close(socket_t sock) {
  closesocket(sock);
}
#else
#include <unistd.h>

int net_init(void) {
  return 0;
}

void net_cleanup(void) {
}

void net_close(socket_t sock) {
  close(sock);
}
#endif
