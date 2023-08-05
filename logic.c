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
    .init_sess_f          = &hub_init_room_session,
    .deinit_sess_f        = &hub_deinit_room_session,
    .process_line_f       = &hub_process_line,
    .room_is_available_f  = &hub_is_available
};

room_preset_t fool_preset = {
    .name                 = "fool",

    .init_room_f          = &fool_init_room,
    .deinit_room_f        = &fool_deinit_room,
    .init_sess_f          = &fool_init_room_session,
    .deinit_sess_f        = &fool_deinit_room_session,
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

room_session_t *make_room_session(server_room_t *s_room,
                                  session_interface_t *interf,
                                  char *username)
{
    ASSERT(s_room);
    ASSERT(interf);

    room_session_t *r_sess = malloc(sizeof(*r_sess));
    r_sess->room = s_room;
    r_sess->interf = interf;
    r_sess->username = username;
    r_sess->is_in_chat = false;
    (*s_room->preset->init_sess_f)(r_sess);

    return r_sess;
}

void destroy_room_session(room_session_t *r_sess)
{
    ASSERT(r_sess);
    (*r_sess->room->preset->deinit_sess_f)(r_sess);
    free(r_sess);
}

void room_session_process_line(room_session_t *r_sess, const char *line)
{
    (*r_sess->room->preset->process_line_f)(r_sess, line);
}

// This reaction will be mutual for all logics
void room_session_process_too_long_line(room_session_t *r_sess)
{
    OUTBUF_POST(r_sess, "ERR: Line was too long\r\n");
    r_sess->interf->quit = true;
}
