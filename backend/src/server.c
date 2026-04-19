#include "server.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "http.h"
#include "net.h"
#include "router.h"

int server_run(const AppConfig *config) {
  socket_t server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == (socket_t)-1) {
    perror("socket");
    return -1;
  }

  int opt = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((unsigned short)config->port);

  if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("bind");
    net_close(server);
    return -1;
  }

  if (listen(server, 16) != 0) {
    perror("listen");
    net_close(server);
    return -1;
  }

  printf("Backend listening on http://localhost:%d\n", config->port);

  for (;;) {
    struct sockaddr_in client_addr;
#ifdef _WIN32
    int client_len = sizeof(client_addr);
#else
    socklen_t client_len = sizeof(client_addr);
#endif
    socket_t client = accept(server, (struct sockaddr *)&client_addr, &client_len);
    if (client == (socket_t)-1) {
      continue;
    }

    HttpRequest request;
    HttpResponse response = {0};

    if (http_read_request(client, &request) != 0) {
      http_response_set_error(&response, 400, "Malformed request");
    } else {
      router_handle_request(config, &request, &response);
      http_request_free(&request);
    }

    if (!response.body) {
      http_response_set_error(&response, 500, "Unable to build response");
    }

    http_send_response(client, &response);
    http_response_free(&response);
    net_close(client);
  }

  net_close(server);
  return 0;
}
