/* TextGameServer/sudoku.h */
#ifndef SUDOKU_SENTRY
#define SUDOKU_SENTRY

#include "defs.h"
#include "logic.h"

void sudoku_init_room(server_room_t *s_room, void *payload);
void sudoku_deinit_room(server_room_t *s_room);
void sudoku_init_room_session(room_session_t *r_sess);
void sudoku_deinit_room_session(room_session_t *r_sess);
void sudoku_process_line(room_session_t *r_sess, const char *line);
bool sudoku_room_is_available(server_room_t *s_room);

#endif
