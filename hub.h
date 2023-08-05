/* TextGameServer/hub.h */
#ifndef HUB_SENTRY
#define HUB_SENTRY
#include "logic.h"

void hub_init_room(server_room_t *s_room, void *payload);
void hub_deinit_room(server_room_t *s_room);
void hub_init_session_logic(session_logic_t *sess_l);
void hub_deinit_session_logic(session_logic_t *sess_l);
void hub_process_line(session_logic_t *sess_l, const char *line);
bool hub_is_available(server_room_t *s_room);

#endif
