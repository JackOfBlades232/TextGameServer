/* TextGameServer/hub.c */
#include "hub.h"
#include "logic.h"
#include "utils.h"
#include "logic_presets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_SESS_REFS_ARR_SIZE 16
#define INIT_ROOMS_ARR_SIZE     4
#define MAX_ROOMS_ARR_SIZE      16*INIT_ROOMS_ARR_SIZE

#define CREDENTIAL_MAX_LEN      64

// @TODO: generalize file path specification?
static const char passwd_path[] = "./passwd.txt";

// @TODO: fix chat graphics
// @TODO: restrict only one session per username

// @TODO: refac

typedef enum hub_user_state_tag {
    hs_input_username,
    hs_input_passwd,
    hs_create_user,
    hs_global_chat
} hub_user_state_t;

typedef struct hub_server_data_tag {
    FILE *passwd_f;
    FILE *stats_f;

    server_logic_t **rooms;
    int rooms_size;
} hub_server_data_t;

typedef struct hub_session_data_tag {
    hub_user_state_t state;
    char *expected_password;
} hub_session_data_t;

void hub_init_server_logic(server_logic_t *serv_l)
{
    serv_l->sess_cap = INIT_SESS_REFS_ARR_SIZE;
    serv_l->sess_cnt = 0;
    serv_l->sess_refs = malloc(serv_l->sess_cap * sizeof(*serv_l->sess_refs));

    serv_l->data = malloc(sizeof(hub_server_data_t));
    hub_server_data_t *sv_data = serv_l->data;

    // @TODO: check passwd file correctness, including max word sizes
    sv_data->passwd_f = fopen(passwd_path, "r+");
    ASSERT(sv_data->passwd_f);
    // @TODO: read stats file
    sv_data->stats_f = NULL;

    sv_data->rooms_size = INIT_ROOMS_ARR_SIZE;
    sv_data->rooms = calloc(sv_data->rooms_size, sizeof(*sv_data->rooms));

    for (int i = 0; i < sv_data->rooms_size; i++)
        sv_data->rooms[i] = NULL;
}

void hub_deinit_server_logic(server_logic_t *serv_l)
{
    free(serv_l->sess_refs);

    hub_server_data_t *sv_data = serv_l->data;

    if (sv_data->passwd_f) fclose(sv_data->passwd_f);
    if (sv_data->stats_f) fclose(sv_data->stats_f);

    free(sv_data->rooms);
    free(sv_data);
}

void hub_init_session_logic(session_logic_t *sess_l)
{
    sess_l->data = malloc(sizeof(hub_session_data_t));

    hub_session_data_t *s_data = sess_l->data;

    // Check if this is first switch to hub. if not, straight to glob chat. Otherwise, login
    if (sess_l->username)
        s_data->state = hs_global_chat;
    else {
        s_data->state = hs_input_username;
        s_data->expected_password = NULL;
        OUTBUF_POSTF(sess_l, "%sWelcome to the TextGameServer! Input your username: ", clrscr);
    }

    server_logic_t *serv_l = sess_l->serv;
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

static char *lookup_username_and_get_password(hub_server_data_t *sv_data, const char *usernm);
static bool add_user(hub_server_data_t *sv_data, const char *usernm, const char *passwd);

static void forward_message_to_all_users(server_logic_t *serv_l, session_logic_t *sess_l, const char *msg);
static void send_rooms_list(session_logic_t *sess_l, server_logic_t *serv_l);
static void create_and_join_room(session_logic_t *sess_l, hub_server_data_t *sv_data);
static void try_join_existing_room(session_logic_t *sess_l, hub_server_data_t *sv_data, const char *room_name);

// @TODO: refac?
void hub_process_line(session_logic_t *sess_l, const char *line)
{
    hub_session_data_t *s_data = sess_l->data;
    server_logic_t *serv_l = sess_l->serv;
    hub_server_data_t *sv_data = serv_l->data;

    switch (s_data->state) {
        case hs_input_username:
            {
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
                if (streq(s_data->expected_password, line)) {
                    // @TODO: add more helpful greeting
                    OUTBUF_POSTF(sess_l, "%sWelcome to the global chat!\r\nYou: ", clrscr);
                    s_data->state = hs_global_chat;
                } else {
                    s_data->state = hs_input_username;
                    OUTBUF_POST(sess_l, "The password is incorrect! Rack your memory and try again\r\nInput your username: ");
                }
                free(s_data->expected_password);
                s_data->expected_password = NULL;
            } break;

        case hs_create_user:
            {
                if (add_user(sv_data, sess_l->username, line)) {
                    OUTBUF_POSTF(sess_l, "%sWelcome to the global chat!\r\nYou: ", clrscr);
                    s_data->state = hs_global_chat;
                } else {
                    s_data->state = hs_input_username;
                    OUTBUF_POST(sess_l, "The username or password is invalid, try registering again\r\nInput your username: ");
                }
            } break;

        case hs_global_chat:
            {
                if (strlen(line) == 0)
                    OUTBUF_POST(sess_l, "You: ");
                else if (streq(line, "list"))
                    send_rooms_list(sess_l, serv_l); 
                else if (streq(line, "create"))
                    create_and_join_room(sess_l, sv_data);
                else if (strncmp(line, "join ", 5) == 0)
                    try_join_existing_room(sess_l, sv_data, line+5);
                else {
                    forward_message_to_all_users(serv_l, sess_l, line);
                    OUTBUF_POST(sess_l, "You: ");
                }
            } break;
    }

}

bool hub_server_is_available(server_logic_t *serv_l)
{
    return true;
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

static void forward_message_to_all_users(server_logic_t *serv_l, session_logic_t *sess_l, const char *msg)
{
    ASSERT(sess_l->username);
    for (int i = 0; i < serv_l->sess_cnt; i++) {
        session_logic_t *other_sess_l = serv_l->sess_refs[i];
        if (other_sess_l != sess_l)
            OUTBUF_POSTF(other_sess_l, "\r\n%s: %s\r\nYou: ", sess_l->username, msg);
    }
}

static void send_rooms_list(session_logic_t *sess_l, server_logic_t *serv_l)
{
    hub_server_data_t *sv_data = serv_l->data;
    string_builder_t *sb = sb_create();
    sb_add_str(sb, "\r\nServer rooms:\r\n");
    for (int i = 0; i < sv_data->rooms_size; i++) {
        server_logic_t *room = sv_data->rooms[i];
        if (room)
            sb_add_strf(sb, "   %s %d/%d %s\r\n", room->name,
                    room->sess_cnt, room->sess_cap,
                    server_logic_is_available(room) ? "" : "(closed)");
    }
    sb_add_str(sb, "\r\nYou: ");

    char *full_str = sb_build_string(sb);
    OUTBUF_POST(sess_l, full_str);
    free(full_str);
    sb_free(sb);
}

static void create_and_join_room(session_logic_t *sess_l, hub_server_data_t *sv_data)
{
    server_logic_t *room = NULL;
    for (int i = 0; i <= sv_data->rooms_size; i++) {
        char id[16];

        if (i == sv_data->rooms_size) {
            if (i >= MAX_ROOMS_ARR_SIZE) {
                OUTBUF_POST(sess_l, "Max number of rooms is reached, wait for someone to finish playing\r\nYou: ");
                return;
            }

            int newsize = sv_data->rooms_size + INIT_ROOMS_ARR_SIZE;
            sv_data->rooms = 
                realloc(sv_data->rooms, newsize * sizeof(*sv_data->rooms));
            for (int j = sv_data->rooms_size; j < newsize; j++)
                sv_data->rooms[j] = NULL;
            sv_data->rooms_size = newsize;

            // @TODO: factor out?
            sprintf(id, "%d", i);
            room = make_server_logic(&fool_preset, id);
            sv_data->rooms[i] = room;
            break;
        } else if (!sv_data->rooms[i]) {
            sprintf(id, "%d", i);
            room = make_server_logic(&fool_preset, id);
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

    OUTBUF_POST(sess_l, "Couldn't access the chosen room! Sumimasen\r\nYou: ");
}