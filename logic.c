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

room_preset_t hub_preset = {
    .name                 = "",

    .init_room_f          = &hub_init_room,
    .deinit_room_f        = &hub_deinit_room,
    .init_sess_f          = &hub_init_session_logic,
    .deinit_sess_f        = &hub_deinit_session_logic,
    .process_line_f       = &hub_process_line,
    .room_is_available_f  = &hub_is_available
};

room_preset_t fool_preset = {
    .name                 = "fool",

    .init_room_f          = &fool_init_room,
    .deinit_room_f        = &fool_deinit_room,
    .init_sess_f          = &fool_init_session_logic,
    .deinit_sess_f        = &fool_deinit_session_logic,
    .process_line_f       = &fool_process_line,
    .room_is_available_f  = &fool_room_is_available
};

server_room_t *make_room(room_preset_t *preset, const char *id, 
                                 FILE *logs_file_handle, void *payload)
{
    server_room_t *s_room = malloc(sizeof(*s_room));
    s_room->preset = preset;
    if (id) 
        s_room->name = strcat_alloc(preset->name, id);
    else
        s_room->name = strdup(preset->name);

    s_room->chat = make_chat();
    s_room->logs_file_handle = logs_file_handle;

    (*preset->init_room_f)(s_room, payload);

    return s_room;
}

void destroy_room(server_room_t *s_room)
{
    ASSERT(s_room);
    (*s_room->preset->deinit_room_f)(s_room);
    destroy_chat(s_room->chat);
    if (s_room->name) free(s_room->name);
    free(s_room);
}

session_logic_t *make_session_logic(server_room_t *s_room,
                                    session_interface_t *interf,
                                    char *username)
{
    ASSERT(s_room);
    ASSERT(interf);

    session_logic_t *sess_l = malloc(sizeof(*sess_l));
    sess_l->room = s_room;
    sess_l->interf = interf;
    sess_l->username = username;
    sess_l->is_in_chat = false;
    (*s_room->preset->init_sess_f)(sess_l);

    return sess_l;
}

void destroy_session_logic(session_logic_t *sess_l)
{
    ASSERT(sess_l);
    (*sess_l->room->preset->deinit_sess_f)(sess_l);
    free(sess_l);
}

void session_logic_process_line(session_logic_t *sess_l, const char *line)
{
    (*sess_l->room->preset->process_line_f)(sess_l, line);
}

// This reaction will be mutual for all logics
void session_logic_process_too_long_line(session_logic_t *sess_l)
{
    OUTBUF_POST(sess_l, "ERR: Line was too long\r\n");
    sess_l->interf->quit = true;
}
