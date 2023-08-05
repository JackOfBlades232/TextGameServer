/* TextGameServer/hub.h */
#ifndef HUB_SENTRY
#define HUB_SENTRY
#include "logic.h"

void hub_init_room(server_room_t *s_room, void *payload);
void hub_deinit_room(server_room_t *s_room);
void hub_init_room_session(room_session_t *r_sess);
void hub_deinit_room_session(room_session_t *r_sess);
void hub_process_line(room_session_t *r_sess, const char *line);
bool hub_is_available(server_room_t *s_room);

#endif
