/* TextGameServer/fool.h */
#ifndef FOOL_SENTRY
#define FOOL_SENTRY

#include "defs.h"
#include "logic.h"

void fool_init_server_logic(server_logic_t *serv_l, void *payload);
void fool_deinit_server_logic(server_logic_t *serv_l);
void fool_init_session_logic(session_logic_t *sess_l);
void fool_deinit_session_logic(session_logic_t *sess_l);
void fool_process_line(session_logic_t *sess_l, const char *line);
bool fool_server_is_available(server_logic_t *serv_l);
bool fool_log_results(server_logic_t *serv_l);

#endif
