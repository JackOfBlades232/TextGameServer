/* TextGameServer/logic.h */
#ifndef LOGIC_SENTRY
#define LOGIC_SENTRY

#include "defs.h"

typedef struct logic_preset_tag logic_preset_t;
typedef struct session_logic_tag session_logic_t;

typedef struct server_logic_tag {
    logic_preset_t *preset;

    char *name;
    session_logic_t **sess_refs;
    int sess_cnt, sess_cap;

    void *data;
} server_logic_t;

typedef struct session_interface_tag {
    char *out_buf;
    int out_buf_len;

    server_logic_t *next_room;

    bool quit;
} session_interface_t;

// Functions for working with session/server data & interface in different logic modules
typedef void (*init_serv_func_t)(server_logic_t *);
typedef void (*deinit_serv_func_t)(server_logic_t *);
typedef void (*init_sess_func_t)(session_logic_t *);
typedef void (*deinit_sess_func_t)(session_logic_t *);
typedef void (*state_process_line_func_t)(session_logic_t *, const char *);
typedef bool (*serv_is_available_func_t)(server_logic_t *);

struct logic_preset_tag {
    const char *name;

    init_serv_func_t           init_serv_f;
    deinit_serv_func_t         deinit_serv_f;
    init_sess_func_t           init_sess_f;
    deinit_sess_func_t         deinit_sess_f;
    state_process_line_func_t  process_line_f;
    serv_is_available_func_t   serv_is_available_f;
};

struct session_logic_tag {
    server_logic_t *serv;
    session_interface_t *interf;

    char *username;

    void *data;
};

server_logic_t *make_server_logic(logic_preset_t *preset, const char *id);
void destroy_server_logic(server_logic_t *serv_l);
session_logic_t *make_session_logic(server_logic_t *serv_l,
                                    session_interface_t *interf,
                                    char *username);
void destroy_session_logic(session_logic_t *sess_s);
void session_logic_process_line(session_logic_t *sess_s, const char *line);

// Not passing the line in, just process the event (like send smth and quit)
void session_logic_process_too_long_line(session_logic_t *sess_l);

static inline bool server_logic_is_available(server_logic_t *serv_l)
{
    return (*serv_l->preset->serv_is_available_f)(serv_l);
}

// Universal utility thigys for posting responses
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

static char clrscr[] = "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                       "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                       "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
                       "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";

#endif
