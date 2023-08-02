/* TextGameServer/logic.h */
#ifndef LOGIC_SENTRY
#define LOGIC_SENTRY

#include "defs.h"

typedef struct session_interface_tag {
    char *out_buf;
    int out_buf_len;
    bool quit;
} session_interface_t;

typedef struct session_logic_tag session_logic_t;
typedef struct server_logic_tag server_logic_t;

server_logic_t *make_server_logic();
void destroy_server_logic(server_logic_t *serv_l);
session_logic_t *make_session_logic(server_logic_t *serv_s,
                                    session_interface_t *interf);
void destroy_session_logic(session_logic_t *sess_s);
void session_logic_process_line(session_logic_t *sess_s, const char *line);

// Not passing the line in, just process the event (like send smth and quit)
void session_logic_process_too_long_line(session_logic_t *sess_l);

// Universal utility macros for posting responses
#define OUTBUF_POST(_sess_l, _str) do { \
    if (_sess_l->interf->out_buf) free(_sess_l->interf->out_buf); \
    _sess_l->interf->out_buf = strdup(_str); \
    _sess_l->interf->out_buf_len = strlen(_sess_l->interf->out_buf); \
} while (0)

#define OUTBUF_POSTF(_sess_l, _fmt, ...) do { \
    if (_sess_l->interf->out_buf) free(_sess_l->interf->out_buf); \
    size_t req_size = snprintf(NULL, 0, _fmt, ##__VA_ARGS__) + 1; \
    _sess_l->interf->out_buf = malloc(req_size * sizeof(*_sess_l->interf->out_buf)); \
    _sess_l->interf->out_buf_len = sprintf(_sess_l->interf->out_buf, _fmt, ##__VA_ARGS__); \
} while (0)

#endif
