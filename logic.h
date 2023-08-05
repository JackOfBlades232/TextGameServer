/* TextGameServer/logic.h */
#ifndef LOGIC_SENTRY
#define LOGIC_SENTRY

#include "defs.h"
#include "chat.h"
#include "utils.h"

typedef struct room_preset_tag room_preset_t;
typedef struct room_session_tag room_session_t;

typedef struct server_room_tag {
    room_preset_t *preset;

    char *name;
    room_session_t **sess_refs;
    int sess_cnt, sess_cap;

    chat_t *chat;
    FILE *logs_file_handle;

    void *data;
} server_room_t;

typedef struct session_interface_tag {
    char *out_buf;
    int out_buf_len;

    server_room_t *next_room;
    bool need_to_register_username;

    bool quit;
} session_interface_t;

// Functions for working with session/server data & interface in different logic modules
typedef void (*init_room_func_t)(server_room_t *, void *payload);
typedef void (*deinit_room_func_t)(server_room_t *);
typedef void (*init_sess_func_t)(room_session_t *);
typedef void (*deinit_sess_func_t)(room_session_t *);
typedef void (*state_process_line_func_t)(room_session_t *, const char *);
typedef bool (*room_is_available_func_t)(server_room_t *);

struct room_preset_tag {
    const char *name;

    init_room_func_t           init_room_f;
    deinit_room_func_t         deinit_room_f;
    init_sess_func_t           init_sess_f;
    deinit_sess_func_t         deinit_sess_f;
    state_process_line_func_t  process_line_f;
    room_is_available_func_t   room_is_available_f;
};

struct room_session_tag {
    server_room_t *room;
    session_interface_t *interf;

    char *username;
    bool is_in_chat;

    void *data;
};

server_room_t *make_room(room_preset_t *preset, const char *id, 
                         FILE *logs_file_handle, void *payload);
void destroy_room(server_room_t *s_room);
room_session_t *make_room_session(server_room_t *s_room,
                                  session_interface_t *interf,
                                  char *username);
void destroy_room_session(room_session_t *r_sess);
void room_session_process_line(room_session_t *r_sess, const char *line);

// Not passing the line in, just process the event (like send smth and quit)
void room_session_process_too_long_line(room_session_t *r_sess);

static inline bool room_is_available(server_room_t *s_room)
{
    return (*s_room->preset->room_is_available_f)(s_room);
}

// Universal utility thigys for posting responses
#define OUTBUF_POST(_r_sess, _str) do { \
    if (_r_sess->interf->out_buf) free(_r_sess->interf->out_buf); \
    _r_sess->interf->out_buf = strdup(_str); \
    _r_sess->interf->out_buf_len = strlen(_r_sess->interf->out_buf); \
} while (0)

#define OUTBUF_POSTF(_r_sess, _fmt, ...) do { \
    if (_r_sess->interf->out_buf) free(_r_sess->interf->out_buf); \
    size_t req_size = snprintf(NULL, 0, _fmt, ##__VA_ARGS__) + 1; \
    _r_sess->interf->out_buf = malloc(req_size * sizeof(*_r_sess->interf->out_buf)); \
    _r_sess->interf->out_buf_len = sprintf(_r_sess->interf->out_buf, _fmt, ##__VA_ARGS__); \
} while (0)

extern char clrscr[];

extern room_preset_t hub_preset;
extern room_preset_t fool_preset;

typedef struct hub_payload_tag {
    sized_array_t *logged_in_usernames;
    const char *passwd_path;
} hub_payload_t;

typedef struct game_payload_tag {
    server_room_t *hub_ref;
} game_payload_t;

#endif
