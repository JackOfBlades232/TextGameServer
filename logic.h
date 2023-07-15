/* TextGameServer/logic.h */
#ifndef LOGIC_SENTRY
#define LOGIC_SENTRY

#include "defs.h"

typedef struct session_interface_tag {
    char *out_buf;
    int out_buf_len;
    bool quit;
} session_interface;

typedef struct session_state_tag session_state;
typedef struct server_state_tag server_state;

server_state *make_server_state();
session_state *make_session_state(server_state *serv_s,
                                  session_interface *interf);
void destroy_session_state(session_state *sess_s);
void session_state_process_line(session_state *sess_s, const char *line);

#endif
