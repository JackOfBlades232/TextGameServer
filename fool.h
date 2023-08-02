/* TextGameServer/fool.h */
#ifndef FOOL_SENTRY
#define FOOL_SENTRY

void fool_init_server_logic(server_logic_t *serv_l);
void fool_deinit_server_logic(server_logic_t *serv_l);
void fool_init_session_logic(session_logic_t *sess_l);
void fool_deinit_session_logic(session_logic_t *sess_l);
void fool_process_line(session_logic_t *sess_l, const char *line);

#endif
