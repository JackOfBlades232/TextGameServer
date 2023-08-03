/* TextGameServer/module_functables.h */
#include "logic.h"
#include "fool.h"

static logic_state_functable_t fool_functable = {
    .init_serv_f          = &fool_init_server_logic,
    .deinit_serv_f        = &fool_deinit_server_logic,
    .init_sess_f          = &fool_init_session_logic,
    .deinit_sess_f        = &fool_deinit_session_logic,
    .process_line_f       = &fool_process_line,
    .serv_is_available_f  = &fool_server_is_available
};
