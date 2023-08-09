/* TextGameServer/server.c */
#include "defs.h"
#include "utils.h"
#include "logic.h"
#include "room_presets.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

// @TODO: Add "show tutor" command which shows the player commands text until he presses enter

#define LISTEN_QLEN          16
#define INIT_SESS_ARR_SIZE   32
#define INIT_ROOMS_ARR_SIZE  4
#define INBUFSIZE            1024

typedef struct session_tag {
    int fd;
    char buf[INBUFSIZE];
    int buf_used;

    session_interface_t interf;
    room_session_t *rs;
    char *username;
} session;

typedef struct server_tag {
    int ls;
    session **sessions;
    int sessions_size;

    // Custom logic
    server_room_t *hub;
    sized_array_t logged_in_usernames;
    FILE *result_logs_f;
} server;

static const char passwd_path[] = "./passwd.txt";
static const char logs_path[] = "./res_logs.txt";

session *make_session(int fd, server_room_t *room)
{
    session *sess = malloc(sizeof(*sess));
    sess->fd = fd;
    sess->buf_used = 0;

    sess->interf.out_buf = NULL;
    sess->interf.out_buf_len = 0;
    sess->interf.next_room = NULL;
    sess->interf.need_to_register_username = false;
    sess->interf.quit = false;

    sess->username = NULL;
    sess->rs = make_room_session(room, &sess->interf, sess->username);
    return sess;
}

void cleanup_session(session *sess)
{
    if (sess->interf.out_buf) free(sess->interf.out_buf);
    if (sess->rs) destroy_room_session(sess->rs);
    if (sess->username) free(sess->username);
}

void session_check_lf(session *sess)
{
    int pos = -1;
    char *line;
    for (int i = 0; i < sess->buf_used; i++) {
        if (sess->buf[i] == '\n') {
            pos = i;
            break;
        }
    }
    if (pos == -1) 
        return;

    line = malloc(pos+1);
    memcpy(line, sess->buf, pos);
    line[pos] = '\0';
    sess->buf_used -= pos+1;
    memmove(sess->buf, sess->buf+pos+1, sess->buf_used);
    if (line[pos-1] == '\r')
        line[pos-1] = '\0';

    room_session_process_line(sess->rs, line);
    free(line);
}

bool session_do_read(session *sess)
{
    // If waiting to send data or marked for change room/quit, skip turn
    if (sess->interf.out_buf || sess->interf.next_room || sess->interf.quit)
        return true;

    int rc, bufp = sess->buf_used;
    rc = read(sess->fd, sess->buf + bufp, INBUFSIZE-bufp);
    if (rc <= 0) // Disconnected
        return false;

    sess->buf_used += rc;
    session_check_lf(sess);

    // If session logic set quit to true, still perform the write if need be
    if (sess->buf_used == INBUFSIZE)
        room_session_process_too_long_line(sess->rs);

    return true;
}

bool session_do_write(session *sess)
{
    ASSERT(sess->interf.out_buf && sess->interf.out_buf_len > 0);

    // @NOTE for robustness it might be good to implement cutting the out_buf
    // up, but actual message sizes do not require this
    int wc = write(sess->fd, 
                   sess->interf.out_buf, 
                   sess->interf.out_buf_len);
    free(sess->interf.out_buf);
    sess->interf.out_buf = NULL;
    sess->interf.out_buf_len = 0;

    if (wc <= 0) // Disconnected
        return false;

    return true;
}

void switch_session_room(server *serv, session *sess)
{
    ASSERT(sess->interf.next_room);

    destroy_room_session(sess->rs);
    sess->rs = make_room_session(sess->interf.next_room, &sess->interf, sess->username);
    sess->interf.next_room = NULL;
}

void server_init(server *serv, int port)
{
    int sock, opt;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_ERR(sock >= 0);

    opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    int br = bind(sock, (struct sockaddr *) &addr, sizeof(addr)); 
    ASSERT_ERR(br == 0);

    listen(sock, LISTEN_QLEN);
    serv->ls = sock;

    serv->sessions = calloc(INIT_SESS_ARR_SIZE, sizeof(*serv->sessions));
    serv->sessions_size = INIT_SESS_ARR_SIZE;

    serv->result_logs_f = fopen(logs_path, "a");
    ASSERT(serv->result_logs_f);

    hub_payload_t payload = { 
        .logged_in_usernames = &serv->logged_in_usernames, 
        .passwd_path = passwd_path
    };
    serv->hub = make_room(&hub_preset, NULL, serv->result_logs_f, &payload);
    ASSERT(serv->hub);

    serv->logged_in_usernames.data = calloc(INIT_SESS_ARR_SIZE, sizeof(*serv->logged_in_usernames.data));
    serv->logged_in_usernames.size = INIT_SESS_ARR_SIZE;
}

void server_accept_client(server *serv)
{
    int sd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    sd = accept(serv->ls, (struct sockaddr *) &addr, &len);
    ASSERT_ERR(sd >= 0);

    int flags = fcntl(sd, F_GETFL);
    fcntl(sd, F_SETFL, flags | O_NONBLOCK);

    if (sd >= serv->sessions_size) { // resize if needed
        int newsize = serv->sessions_size;
        while (newsize <= sd)
            newsize += INIT_SESS_ARR_SIZE;
        serv->sessions = 
            realloc(serv->sessions, newsize * sizeof(*serv->sessions));
        serv->logged_in_usernames.data = 
            realloc(serv->logged_in_usernames.data,
                    newsize * sizeof(*serv->logged_in_usernames.data));
        for (int i = serv->sessions_size; i < newsize; i++)
            serv->sessions[i] = NULL;
        serv->sessions_size = newsize;
        serv->logged_in_usernames.size = newsize;
    }

    serv->sessions[sd] = make_session(sd, serv->hub);
    ASSERT(serv->sessions[sd]);
}

void server_close_session(server *serv, int sd)
{
    close(sd);
    cleanup_session(serv->sessions[sd]);
    free(serv->sessions[sd]);
    serv->sessions[sd] = NULL;
    serv->logged_in_usernames.data[sd] = NULL;
}

void init_subsystems()
{
    srand(time(NULL));
}

int main(int argc, char **argv) 
{
    server serv;
    long port;
    char *endptr;

    ASSERTF(argc == 2, "Args: <port>\n");

    port = strtol(argv[1], &endptr, 10);
    ASSERTF(*argv[1] && !*endptr, "Invalid port number\n");
        
    init_subsystems();
    server_init(&serv, port);

    for (;;) {
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(serv.ls, &readfds);

        int maxfd = serv.ls;
        for (int i = 0; i < serv.sessions_size; i++) {
            session *sess = serv.sessions[i];
            if (sess) {
                FD_SET(i, &readfds);
                if (sess->interf.out_buf)
                    FD_SET(i, &writefds);
                if (i > maxfd)
                    maxfd = i;
            }
        }

        int sr = select(maxfd+1, &readfds, &writefds, NULL, NULL);
        ASSERT_ERR(sr >= 0);

        if (FD_ISSET(serv.ls, &readfds))
            server_accept_client(&serv);
        for (int i = 0; i < serv.sessions_size; i++) {
            session *sess = serv.sessions[i];
            if (sess) {
                if (
                        // Try read incoming data, close if disconnected
                        (FD_ISSET(i, &readfds) && !session_do_read(sess)) ||
                        // Try write queued data, close if disconnected
                        (FD_ISSET(i, &writefds) && !session_do_write(sess)) ||
                        // If logic says "quit" and all data is sent, also close
                        (sess->interf.quit && !sess->interf.out_buf)
                   ) 
                {
                    server_close_session(&serv, i);
                    continue;
                }  

                if (sess->interf.need_to_register_username) {
                    sess->username = sess->rs->username;
                    serv.logged_in_usernames.data[i] = sess->rs->username;
                    sess->interf.need_to_register_username = false;
                } 

                if (sess->interf.next_room && !sess->interf.quit && !sess->interf.out_buf) 
                    switch_session_room(&serv, sess);
            }
        }
    }

    // Let the OS deinit stuff
    return 0;
}
