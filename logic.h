/* TextGameServer/logic.h */
#ifndef LOGIC_SENTRY
#define LOGIC_SENTRY

#include "defs.h"

typedef struct session_interface_tag {
    char *out_buf;
    int out_buf_len;
    bool quit;
} session_interface;

typedef struct session_logic_tag session_logic;
typedef struct server_logic_tag server_logic;

server_logic *make_server_logic();
session_logic *make_session_logic(server_logic *serv_s,
                                  session_interface *interf);
void destroy_session_logic(session_logic *sess_s);
void session_logic_process_line(session_logic *sess_s, const char *line);
void session_logic_post_too_long_line_msg(session_logic *sess_l);

#endif
