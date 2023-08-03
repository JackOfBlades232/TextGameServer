/* TextGameServer/hub.c */
#include "hub.h"
#include "logic.h"
#include <stdio.h>

#define INIT_ROOMS_ARR_SIZE  4

typedef struct hub_server_data_tag {
    FILE *passwd_f;
    FILE *stats_f;

    server_logic_t **rooms;
    int rooms_size;
} hub_server_data_t;

// In the hub a player has no local data, thus session data is not implemented

void hub_init_server_logic(server_logic_t *serv_l)
{
}

void hub_deinit_server_logic(server_logic_t *serv_l)
{
}

void hub_init_session_logic(session_logic_t *sess_l)
{
}

void hub_deinit_session_logic(session_logic_t *sess_l)
{
}

void hub_process_line(session_logic_t *sess_l, const char *line)
{
}

bool hub_server_is_available(server_logic_t *serv_l)
{
    return false;
}
