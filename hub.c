/* TextGameServer/hub.c */
#include "hub.h"
#include "logic.h"
#include "chat_funcs.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_SESS_REFS_ARR_SIZE 16
#define INIT_ROOMS_ARR_SIZE     4
#define MAX_ROOMS_ARR_SIZE      16*INIT_ROOMS_ARR_SIZE

#define CREDENTIAL_MAX_LEN      64

typedef enum hub_user_state_tag {
    hs_input_username,
    hs_input_passwd,
    hs_create_user,
    hs_global_chat
} hub_user_state_t;

typedef struct hub_server_data_tag {
    server_logic_t **rooms;
    int rooms_size;

    FILE *passwd_f;
    sized_array_t *logged_in_usernames_ref;
} hub_server_data_t;

typedef struct hub_session_data_tag {
    hub_user_state_t state;
    char *expected_password;
} hub_session_data_t;

static const char global_chat_greeting[] = 
        "Welcome to the global chat!\r\n"
        "Commands:\r\n"
        "   <list>: list all current rooms\r\n"
        "   <create>: create a new room\r\n"
        "   <join *room name*>: join a room\r\n"
        "   anything else: send message to char\r\n\r\n";

static bool passwd_file_is_correct(hub_server_data_t *sv_data);

void hub_init_server_logic(server_logic_t *serv_l, void *payload)
{
    serv_l->sess_cap = INIT_SESS_REFS_ARR_SIZE;
    serv_l->sess_cnt = 0;
    serv_l->sess_refs = malloc(serv_l->sess_cap * sizeof(*serv_l->sess_refs));

    serv_l->data = malloc(sizeof(hub_server_data_t));
    hub_server_data_t *sv_data = serv_l->data;

    sv_data->rooms_size = INIT_ROOMS_ARR_SIZE;
    sv_data->rooms = calloc(sv_data->rooms_size, sizeof(*sv_data->rooms));

    for (int i = 0; i < sv_data->rooms_size; i++)
        sv_data->rooms[i] = NULL;

    hub_payload_t *payload_data = payload;
    sv_data->logged_in_usernames_ref = payload_data->logged_in_usernames;

    sv_data->passwd_f = fopen(payload_data->passwd_path, "r+");
    ASSERT_ERR(sv_data->passwd_f);
    ASSERTF(passwd_file_is_correct(sv_data), "Invalid password file\n");
}

void hub_deinit_server_logic(server_logic_t *serv_l)
{
    free(serv_l->sess_refs);

    hub_server_data_t *sv_data = serv_l->data;
    if (sv_data->passwd_f) fclose(sv_data->passwd_f);
    free(sv_data->rooms);
    free(sv_data);
}

static inline void enter_global_chat(session_logic_t *sess_l, hub_session_data_t *s_data, server_logic_t *serv_l)
{
    sess_l->is_in_chat = true;
    chat_send_updates(serv_l->chat, sess_l, global_chat_greeting);
    s_data->state = hs_global_chat;
}

void hub_init_session_logic(session_logic_t *sess_l)
{
    sess_l->data = malloc(sizeof(hub_session_data_t));

    server_logic_t *serv_l = sess_l->serv;
    hub_session_data_t *s_data = sess_l->data;

    // Check if this is first switch to hub. if not, straight to glob chat. Otherwise, login
    if (sess_l->username)
        enter_global_chat(sess_l, s_data, serv_l);
    else {
        s_data->state = hs_input_username;
        s_data->expected_password = NULL;
        OUTBUF_POSTF(sess_l, "%sWelcome to the TextGameServer! Input your username: ", clrscr);
    }

    if (serv_l->sess_cnt >= serv_l->sess_cap) {
        int new_cap = serv_l->sess_cap;
        while (serv_l->sess_cnt >= new_cap)
            new_cap += INIT_SESS_REFS_ARR_SIZE;
        serv_l->sess_refs = realloc(serv_l->sess_refs, new_cap);
        for (int i = serv_l->sess_cap; i < new_cap; i++)
            serv_l->sess_refs[i] = NULL;
        serv_l->sess_cap = new_cap;
    }

    serv_l->sess_refs[serv_l->sess_cnt++] = sess_l;
}

void hub_deinit_session_logic(session_logic_t *sess_l)
{
    server_logic_t *serv_l = sess_l->serv;

    bool offset = false;
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        if (serv_l->sess_refs[i] == sess_l)
            offset = true;
        else if (offset) {
            serv_l->sess_refs[i-1] = serv_l->sess_refs[i];
            serv_l->sess_refs[i] = NULL;
        }
    }
    serv_l->sess_cnt--;

    hub_session_data_t *s_data = sess_l->data;
    if (s_data->expected_password) free(s_data->expected_password);
    free(s_data);
}

static bool user_already_logged_in(hub_server_data_t *sv_data, const char *usernm);
static char *lookup_username_and_get_password(hub_server_data_t *sv_data, const char *usernm);
static bool add_user(hub_server_data_t *sv_data, const char *usernm, const char *passwd);

static void send_rooms_list(session_logic_t *sess_l, server_logic_t *serv_l);
static void create_and_join_room(session_logic_t *sess_l, server_logic_t *serv_l);
static void try_join_existing_room(session_logic_t *sess_l, hub_server_data_t *sv_data, const char *room_name);

void hub_process_line(session_logic_t *sess_l, const char *line)
{
    hub_session_data_t *s_data = sess_l->data;
    server_logic_t *serv_l = sess_l->serv;
    hub_server_data_t *sv_data = serv_l->data;

    switch (s_data->state) {
        case hs_input_username:
            {
                if (user_already_logged_in(sv_data, line)) {
                    OUTBUF_POST(sess_l, "Such a user is already logged in, try another account\r\nInput your username: ");
                    break;
                }
                sess_l->username = strdup(line);
                s_data->expected_password = lookup_username_and_get_password(sv_data, line);
                if (s_data->expected_password) {
                    OUTBUF_POST(sess_l, "Input your password: ");
                    s_data->state = hs_input_passwd;
                } else {
                    OUTBUF_POST(sess_l, "Such a user does not exist, input new password: ");
                    s_data->state = hs_create_user;
                }
            } break;

        case hs_input_passwd:
            {
                if (user_already_logged_in(sv_data, sess_l->username)) {
                    s_data->state = hs_input_username;
                    OUTBUF_POST(sess_l, "While you were thiking, someone has logged into this account!\r\nInput your username: ");
                    break;
                }
                if (streq(s_data->expected_password, line)) {
                    enter_global_chat(sess_l, s_data, serv_l);
                    sess_l->interf->need_to_register_username = true;
                } else {
                    s_data->state = hs_input_username;
                    OUTBUF_POST(sess_l, "The password is incorrect! Rack your memory and try again\r\nInput your username: ");
                }
                free(s_data->expected_password);
                s_data->expected_password = NULL;
            } break;

        case hs_create_user:
            {
                if (user_already_logged_in(sv_data, sess_l->username)) {
                    s_data->state = hs_input_username;
                    OUTBUF_POST(sess_l, "While you were thiking, someone has logged into this account!\r\nInput your username: ");
                    break;
                }
                if (add_user(sv_data, sess_l->username, line)) {
                    enter_global_chat(sess_l, s_data, serv_l);
                    sess_l->interf->need_to_register_username = true;
                } else {
                    s_data->state = hs_input_username;
                    OUTBUF_POST(sess_l, "The username or password is invalid, try registering again\r\nInput your username: ");
                }
            } break;

        case hs_global_chat:
            {
                ASSERT(sess_l->is_in_chat);
                if (streq(line, "list"))
                    send_rooms_list(sess_l, serv_l); 
                else if (streq(line, "create"))
                    create_and_join_room(sess_l, serv_l);
                else if (strncmp(line, "join ", 5) == 0)
                    try_join_existing_room(sess_l, sv_data, line+5);
                else if (strlen(line) > 0) {
                    if (!chat_try_post_message(serv_l->chat, serv_l, sess_l, line))
                        OUTBUF_POST(sess_l, "The message is too long!\r\n");
                }
            } break;
    }

}

bool hub_server_is_available(server_logic_t *serv_l)
{
    return true;
}

static bool user_already_logged_in(hub_server_data_t *sv_data, const char *usernm)
{
    for (int i = 0; i < sv_data->logged_in_usernames_ref->size; i++) {
        char *existing_name = sv_data->logged_in_usernames_ref->data[i];
        if (existing_name && streq(usernm, existing_name))
            return true;
    }

    return false;
}

static char *lookup_username_and_get_password(hub_server_data_t *sv_data, const char *usernm)
{
    FILE *f = sv_data->passwd_f;
    rewind(f);

    char cred_buf[CREDENTIAL_MAX_LEN+1];
    size_t buflen;
    int break_c = '\0';

    bool reading_usernm = true;
    while (break_c != EOF) {
        buflen = fread_word_to_buf(f, cred_buf, sizeof(cred_buf), &break_c);
        if (buflen == 0)
            continue;

        if (reading_usernm && streq(cred_buf, usernm))
            break;

        reading_usernm = !reading_usernm;
    }

    if (break_c == EOF)
        return NULL;

    do {
        buflen = fread_word_to_buf(f, cred_buf, sizeof(cred_buf), &break_c);
    } while (break_c != EOF && buflen == 0);

    if (buflen == 0)
        return NULL;

    return strdup(cred_buf);
}

static bool cred_is_of_valid_format(const char *cred)
{
    size_t clen = strlen(cred);
    if (clen == 0 || clen > CREDENTIAL_MAX_LEN)
        return false;

    for (; *cred; cred++) {
        if (!char_is_a_symbol(*cred))
            return false;
    }
    
    return true;
}

static bool add_user(hub_server_data_t *sv_data, const char *usernm, const char *passwd)
{
    FILE *f = sv_data->passwd_f;
    fseek(f, 0, SEEK_END);
    if (cred_is_of_valid_format(usernm) && cred_is_of_valid_format(passwd)) {
        fprintf(f, "%s %s\n", usernm, passwd);
        fflush(f);
        return true;
    } else
        return false;
}

static void send_rooms_list(session_logic_t *sess_l, server_logic_t *serv_l)
{
    hub_server_data_t *sv_data = serv_l->data;
    string_builder_t *sb = sb_create();
    sb_add_strf(sb, "\r\nServer rooms (max=%d):\r\n", MAX_ROOMS_ARR_SIZE);
    for (int i = 0; i < sv_data->rooms_size; i++) {
        server_logic_t *room = sv_data->rooms[i];
        if (room)
            sb_add_strf(sb, "   %s %d/%d %s\r\n", room->name,
                    room->sess_cnt, room->sess_cap,
                    server_logic_is_available(room) ? "" : "(closed)");
    }
    sb_add_str(sb, "\r\n");

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(sess_l, full_str);
    free(full_str);
    sb_free(sb);
}

static void create_and_join_room(session_logic_t *sess_l, server_logic_t *serv_l)
{
    hub_server_data_t *sv_data = serv_l->data;
    game_payload_t payload = { .hub_ref = serv_l };

    server_logic_t *room = NULL;
    for (int i = 0; i <= sv_data->rooms_size; i++) {
        char id[16];

        if (i == sv_data->rooms_size) {
            if (i >= MAX_ROOMS_ARR_SIZE) {
                OUTBUF_POST(sess_l, "Max number of rooms is reached, wait for someone to finish playing\r\n");
                return;
            }

            int newsize = sv_data->rooms_size + INIT_ROOMS_ARR_SIZE;
            sv_data->rooms = 
                realloc(sv_data->rooms, newsize * sizeof(*sv_data->rooms));
            for (int j = sv_data->rooms_size; j < newsize; j++)
                sv_data->rooms[j] = NULL;
            sv_data->rooms_size = newsize;
        } 

        if (!sv_data->rooms[i]) {
            sprintf(id, "%d", i);
            room = make_server_logic(&fool_preset, id, serv_l->logs_file_handle, &payload);
            sv_data->rooms[i] = room;
            break;
        } else if (sv_data->rooms[i]->sess_cnt <= 0) {
            // Clean up empty rooms
            destroy_server_logic(sv_data->rooms[i]);
            sv_data->rooms[i] = NULL;
        }
    }

    sess_l->interf->next_room = room;
}

static void try_join_existing_room(session_logic_t *sess_l, hub_server_data_t *sv_data, const char *room_name)
{
    for (int i = 0; i < sv_data->rooms_size; i++) {
        server_logic_t *room = sv_data->rooms[i];
        if (room && streq(room_name, room->name) && server_logic_is_available(room)) {
            sess_l->interf->next_room = room;
            return;
        }
    }

    OUTBUF_POST(sess_l, "Couldn't access the chosen room! Sumimasen\r\n");
}

static bool passwd_file_is_correct(hub_server_data_t *sv_data)
{
    FILE *f = sv_data->passwd_f;
    rewind(f);

    char cred_buf[CREDENTIAL_MAX_LEN+2];
    size_t buflen;
    int break_c = '\0';

    bool reading_usernm = true;
    while (break_c != EOF) {
        buflen = fread_word_to_buf(f, cred_buf, sizeof(cred_buf), &break_c);
        if (buflen == 0)
            continue;
        else if (buflen > CREDENTIAL_MAX_LEN)
            return false;

        reading_usernm = !reading_usernm;
    }

    return reading_usernm;
}
