/* TextGameServer/logic.c */
#include "logic.h"
#include "utils.h"
#include "chat_funcs.h"
#include "hub.h"
#include "fool.h"
#include <stdlib.h>
#include <string.h>

char clrscr[] = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";

logic_preset_t hub_preset = {
    .name                 = "",

    .init_serv_f          = &hub_init_server_logic,
    .deinit_serv_f        = &hub_deinit_server_logic,
    .init_sess_f          = &hub_init_session_logic,
    .deinit_sess_f        = &hub_deinit_session_logic,
    .process_line_f       = &hub_process_line,
    .serv_is_available_f  = &hub_server_is_available
};

logic_preset_t fool_preset = {
    .name                 = "fool",

    .init_serv_f          = &fool_init_server_logic,
    .deinit_serv_f        = &fool_deinit_server_logic,
    .init_sess_f          = &fool_init_session_logic,
    .deinit_sess_f        = &fool_deinit_session_logic,
    .process_line_f       = &fool_process_line,
    .serv_is_available_f  = &fool_server_is_available
};

server_logic_t *make_server_logic(logic_preset_t *preset, const char *id, 
                                  FILE *logs_file_handle, void *payload)
{
    server_logic_t *serv_l = malloc(sizeof(*serv_l));
    serv_l->preset = preset;
    if (id) 
        serv_l->name = strcat_alloc(preset->name, id);
    else
        serv_l->name = strdup(preset->name);

    serv_l->chat = make_chat();
    serv_l->logs_file_handle = logs_file_handle;

    (*preset->init_serv_f)(serv_l, payload);

    return serv_l;
}

void destroy_server_logic(server_logic_t *serv_l)
{
    ASSERT(serv_l);
    (*serv_l->preset->deinit_serv_f)(serv_l);
    destroy_chat(serv_l->chat);
    if (serv_l->name) free(serv_l->name);
    free(serv_l);
}

session_logic_t *make_session_logic(server_logic_t *serv_l,
                                    session_interface_t *interf,
                                    char *username)
{
    ASSERT(serv_l);
    ASSERT(interf);

    session_logic_t *sess_l = malloc(sizeof(*sess_l));
    sess_l->serv = serv_l;
    sess_l->interf = interf;
    sess_l->username = username;
    sess_l->is_in_chat = false;
    (*serv_l->preset->init_sess_f)(sess_l);

    return sess_l;
}

void destroy_session_logic(session_logic_t *sess_l)
{
    ASSERT(sess_l);
    (*sess_l->serv->preset->deinit_sess_f)(sess_l);
    free(sess_l);
}

void session_logic_process_line(session_logic_t *sess_l, const char *line)
{
    (*sess_l->serv->preset->process_line_f)(sess_l, line);
}

// This reaction will be mutual for all logics
// @HUH: maybe this should be also logic-dependant?
void session_logic_process_too_long_line(session_logic_t *sess_l)
{
    OUTBUF_POST(sess_l, "ERR: Line was too long\r\n");
    sess_l->interf->quit = true;
}
