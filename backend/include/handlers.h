#ifndef HANDLERS_H
#define HANDLERS_H

#include "http.h"
#include "models.h"

void handle_get_chores(const AppConfig *config, HttpResponse *response);
void handle_get_chore_by_id(const AppConfig *config, const char *id, HttpResponse *response);
void handle_create_chore(const AppConfig *config, const HttpRequest *request, HttpResponse *response);
void handle_patch_chore(const AppConfig *config, const char *id, const HttpRequest *request, HttpResponse *response);

void handle_get_messages(const AppConfig *config, HttpResponse *response);
void handle_create_message(const AppConfig *config, const HttpRequest *request, HttpResponse *response);

#endif
