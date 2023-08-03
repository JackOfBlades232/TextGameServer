/* TextGameServer/hub.c */
#include "hub.h"
#include "logic.h"
#include "logic_presets.h"
#include <stdio.h>
#include <stdlib.h>

#define INIT_SESS_REFS_ARR_SIZE 16
#define INIT_ROOMS_ARR_SIZE  4

typedef enum hub_user_state_tag {
    hs_input_username,
    hs_input_passwd,
    hs_create_user,
    hs_global_chat
} hub_user_state_t;

typedef struct hub_server_data_tag {
    FILE *passwd_f;
    FILE *stats_f;

    server_logic_t **rooms;
    int rooms_size;
} hub_server_data_t;

typedef struct hub_session_data_tag {
    hub_user_state_t state;
} hub_session_data_t;

void hub_init_server_logic(server_logic_t *serv_l)
{
    serv_l->sess_cap = INIT_SESS_REFS_ARR_SIZE;
    serv_l->sess_cnt = 0;
    serv_l->sess_refs = malloc(serv_l->sess_cap * sizeof(*serv_l->sess_refs));

    serv_l->data = malloc(sizeof(hub_server_data_t));
    hub_server_data_t *sv_data = serv_l->data;

    // @TODO: read passwd file
    sv_data->passwd_f = NULL;
    // @TODO: read stats file
    sv_data->stats_f = NULL;

    sv_data->rooms_size = INIT_ROOMS_ARR_SIZE;
    sv_data->rooms = calloc(sv_data->rooms_size, sizeof(*sv_data->rooms));

    // @TEST
    for (int i = 0; i < sv_data->rooms_size; i++) {
        char id[16];
        sprintf(id, "%d", i);
        sv_data->rooms[i] = make_server_logic(&fool_preset, id);
        ASSERT(sv_data->rooms[i]);
    }
}

void hub_deinit_server_logic(server_logic_t *serv_l)
{
    free(serv_l->sess_refs);

    hub_server_data_t *sv_data = serv_l->data;

    if (sv_data->passwd_f) fclose(sv_data->passwd_f);
    if (sv_data->stats_f) fclose(sv_data->stats_f);

    free(sv_data->rooms);
    free(sv_data);
}

void hub_init_session_logic(session_logic_t *sess_l)
{
    sess_l->data = malloc(sizeof(hub_session_data_t));
    hub_session_data_t *s_data = sess_l->data;
    s_data->state = hs_input_username;
    OUTBUF_POSTF(sess_l, "%sWelcome to the TextGameServer! Input your username: ", clrscr);

    server_logic_t *serv_l = sess_l->serv;
    if (serv_l->sess_cnt >= serv_l->sess_cap) {
        int new_cap = serv_l->sess_cap;
        while (serv_l->sess_cnt >= new_cap)
            new_cap += INIT_SESS_REFS_ARR_SIZE;
        serv_l->sess_refs = realloc(serv_l->sess_refs, new_cap);
        for (int i = serv_l->sess_cap; i < new_cap; i++)
            serv_l->sess_refs[i] = NULL;
        serv_l->sess_cap = new_cap;
    }

    serv_l->sess_refs[serv_l->sess_cnt++] = sess_l;
}

void hub_deinit_session_logic(session_logic_t *sess_l)
{
    server_logic_t *serv_l = sess_l->serv;

    bool offset = false;
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        if (serv_l->sess_refs[i] == sess_l)
            offset = true;
        else if (offset) {
            serv_l->sess_refs[i-1] = serv_l->sess_refs[i];
            serv_l->sess_refs[i] = NULL;
        }
    }
    serv_l->sess_cnt--;

    free(sess_l->data);
}

void hub_process_line(session_logic_t *sess_l, const char *line)
{
    // @TODO: implement real hub logic

    // @TEST (just try to connect to first available room on printing smth)
    server_logic_t *serv_l = sess_l->serv;
    hub_server_data_t *sv_data = serv_l->data;

    server_logic_t *room;
    for (int i = 0; i <= sv_data->rooms_size; i++) {
        char id[16];

        if (i == sv_data->rooms_size) {
            int newsize = sv_data->rooms_size + INIT_ROOMS_ARR_SIZE;
            sv_data->rooms = 
                realloc(sv_data->rooms, newsize * sizeof(*sv_data->rooms));
            for (int j = sv_data->rooms_size; j < newsize; j++)
                sv_data->rooms[j] = NULL;
            sv_data->rooms_size = newsize;

            // @TEST
            sprintf(id, "%d", i);
            room = make_server_logic(&fool_preset, id);
            sv_data->rooms[i] = room;
            break;
        }

        room = sv_data->rooms[i];
        if (!room) {
            // @TEST
            sprintf(id, "%d", i);
            room = make_server_logic(&fool_preset, id);
            sv_data->rooms[i] = room;
            break;
        } else if (server_logic_is_available(room))
            break;
    }

    sess_l->interf->next_room = room;
}

bool hub_server_is_available(server_logic_t *serv_l)
{
    return true;
}
