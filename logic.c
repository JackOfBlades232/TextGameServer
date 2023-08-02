/* TextGameServer/logic.c */
#include "logic.h"
#include <stdlib.h>

server_logic_t *make_server_logic(logic_state_functable_t *functable)
{
    server_logic_t *serv_l = malloc(sizeof(*serv_l));
    serv_l->functable = functable;
    (*functable.init_serv_f)(serv_l);

    return serv_l;
}

void destroy_server_logic(server_logic_t *serv_l)
{
    ASSERT(serv_l && serv_l->data);
    (*serv_l->functable.deinit_serv_f)(serv_l);
    free(serv_l);
}

session_logic_t *make_session_logic(server_logic_t *serv_l,
                                    session_interface_t *interf)
{
    ASSERT(serv_l);
    ASSERT(interf);

    session_logic_t *sess_l = malloc(sizeof(*sess_l));
    sess_l->serv = serv_l;
    sess_l->interf = interf;
    (*serv_l->functable.init_sess_f)(sess_l);

    return sess_l;
}

void destroy_session_logic(session_logic_t *sess_l)
{
    ASSERT(sess_l && sess_l->data);
    (*sess_l->serv->functable.deinit_sess_f)(sess_l);
    free(sess_l);
}

void session_logic_process_line(session_logic_t *sess_l, const char *line)
{
    (*sess_l->serv->functable.process_line_f)(sess_l, line);
}

// This reaction will be mutual for all logics
// @HUH: maybe this should be also logic-dependant?
void session_logic_process_too_long_line(session_logic_t *sess_l)
{
    OUTBUF_POST(sess_l, "ERR: Line was too long\r\n");
    sess_l->interf->quit = true;
}
