/* TextGameServer/fool.h */
#ifndef FOOL_SENTRY
#define FOOL_SENTRY

#include "defs.h"
#include "logic.h"

void fool_init_room(server_room_t *s_room, void *payload);
void fool_deinit_room(server_room_t *s_room);
void fool_init_room_session(room_session_t *r_sess);
void fool_deinit_room_session(room_session_t *r_sess);
void fool_process_line(room_session_t *r_sess, const char *line);
bool fool_room_is_available(server_room_t *s_room);
bool fool_log_results(server_room_t *s_room);

#endif
