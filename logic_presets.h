/* TextGameServer/logic_presets.h */
#include "logic.h"
#include "hub.h"
#include "fool.h"

static logic_preset_t hub_preset = {
    .name                 = "",

    .init_serv_f          = &hub_init_server_logic,
    .deinit_serv_f        = &hub_deinit_server_logic,
    .init_sess_f          = &hub_init_session_logic,
    .deinit_sess_f        = &hub_deinit_session_logic,
    .process_line_f       = &hub_process_line,
    .serv_is_available_f  = &hub_server_is_available
};

static logic_preset_t fool_preset = {
    .name                 = "fool",

    .init_serv_f          = &fool_init_server_logic,
    .deinit_serv_f        = &fool_deinit_server_logic,
    .init_sess_f          = &fool_init_session_logic,
    .deinit_sess_f        = &fool_deinit_session_logic,
    .process_line_f       = &fool_process_line,
    .serv_is_available_f  = &fool_server_is_available
};
