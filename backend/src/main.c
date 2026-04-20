#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "db.h"
#include "models.h"
#include "net.h"
#include "server.h"

int main(int argc, char **argv) {
  srand((unsigned int)time(NULL));

  AppConfig config = {
    .port = 8080,
    .chores_csv_path = "backend/data/chores.csv",
    .messages_csv_path = "backend/data/messages.csv",
    .roommates_csv_path = "backend/data/roommates.csv",
  };

  if (argc >= 2) {
    config.port = atoi(argv[1]);
    if (config.port <= 0) {
      config.port = 8080;
    }
  }

  if (db_ensure_storage(&config) != 0) {
    fprintf(stderr, "Failed to initialize CSV database files.\n");
    return 1;
  }

  if (net_init() != 0) {
    fprintf(stderr, "Network initialization failed.\n");
    return 1;
  }

  int result = server_run(&config);
  net_cleanup();
  return result == 0 ? 0 : 1;
}
