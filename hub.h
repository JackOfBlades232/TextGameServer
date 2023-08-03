/* TextGameServer/hub.h */
#ifndef HUB_SENTRY
#define HUB_SENTRY
#include "logic.h"

void hub_init_server_logic(server_logic_t *serv_l);
void hub_deinit_server_logic(server_logic_t *serv_l);
void hub_init_session_logic(session_logic_t *sess_l);
void hub_deinit_session_logic(session_logic_t *sess_l);
void hub_process_line(session_logic_t *sess_l, const char *line);
bool hub_server_is_available(server_logic_t *serv_l);

#endif
