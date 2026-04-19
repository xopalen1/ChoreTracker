#ifndef ROUTER_H
#define ROUTER_H

#include "http.h"
#include "models.h"

void router_handle_request(const AppConfig *config, const HttpRequest *request, HttpResponse *response);

#endif
