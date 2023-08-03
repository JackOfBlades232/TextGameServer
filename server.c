/* TextGameServer/server.c */
#include "defs.h"
#include "logic.h"
#include "logic_presets.h"
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

// @TODO: one should be able to exit from game to hub, both voluntarily and if logic kicks.
//      @IDEA: thus, the rooms created in the hub have to hold a ref to it. We could do this by having make/init take a void * payload
// @TODO: implement fool modifications: nickname display, in-game chat module (with switch in-logic)
// @TODO: think about statistics storage (how game-specific should they be?)


// @TODO: implement variation with threads (one thread deals with a subset of containers)
//      @NOTE: for thread cotainer separations I will need enum type for games (make it equal to func table list index?)
// @TODO: implement normal quit on ^C

// @TODO: really do something about the naming (the logics, rooms and shit are confusing)

#define LISTEN_QLEN          16
#define INIT_SESS_ARR_SIZE   32
#define INIT_ROOMS_ARR_SIZE  4
#define INBUFSIZE            1024

typedef struct session_tag {
    int fd;
    char buf[INBUFSIZE];
    int buf_used;

    session_interface_t l_interf;
    session_logic_t *logic;
} session;

typedef struct server_tag {
    int ls;
    session **sessions;
    int sessions_size;

    server_logic_t *hub;
} server;

session *make_session(int fd, server_logic_t *room)
{
    session *sess = malloc(sizeof(*sess));
    sess->fd = fd;
    sess->buf_used = 0;

    sess->l_interf.out_buf = NULL;
    sess->l_interf.out_buf_len = 0;
    sess->l_interf.next_room = NULL;
    sess->l_interf.quit = false;

    sess->logic = make_session_logic(room, &sess->l_interf, NULL);
    return sess;
}

void cleanup_session(session *sess)
{
    if (sess->l_interf.out_buf) free(sess->l_interf.out_buf);
    if (sess->logic) destroy_session_logic(sess->logic);
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

    session_logic_process_line(sess->logic, line);
    free(line);
}

bool session_do_read(session *sess)
{
    // If waiting to send data or marked for change room/quit, skip turn
    if (sess->l_interf.out_buf || sess->l_interf.next_room || sess->l_interf.quit)
        return true;

    int rc, bufp = sess->buf_used;
    rc = read(sess->fd, sess->buf + bufp, INBUFSIZE-bufp);
    if (rc <= 0) // Disconnected
        return false;

    sess->buf_used += rc;
    session_check_lf(sess);

    // If session logic set quit to true, still perform the write if need be
    if (sess->buf_used == INBUFSIZE)
        session_logic_process_too_long_line(sess->logic);

    return true;
}

bool session_do_write(session *sess)
{
    ASSERT(sess->l_interf.out_buf && sess->l_interf.out_buf_len > 0);

    // @TODO: Implement cutting up?
    int wc = write(sess->fd, 
                   sess->l_interf.out_buf, 
                   sess->l_interf.out_buf_len);
    free(sess->l_interf.out_buf);
    sess->l_interf.out_buf = NULL;
    sess->l_interf.out_buf_len = 0;

    if (wc <= 0) // Disconnected
        return false;

    return true;
}

void session_switch_room(session *sess)
{
    ASSERT(sess->l_interf.next_room);

    char *username = strdup(sess->logic->username);
    destroy_session_logic(sess->logic);
    sess->logic = make_session_logic(sess->l_interf.next_room, &sess->l_interf, username);
    sess->l_interf.next_room = NULL;
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

    serv->hub = make_server_logic(&hub_preset, NULL);
    ASSERT(serv->hub);
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
        for (int i = serv->sessions_size; i < newsize; i++)
            serv->sessions[i] = NULL;
        serv->sessions_size = newsize;
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
                if (sess->l_interf.out_buf)
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
                        (sess->l_interf.quit && !sess->l_interf.out_buf)
                   ) 
                {
                    server_close_session(&serv, i);
                }

                // If sent all that was in the buffer and not quitting, and logic says "next room", do so
                if (sess->l_interf.next_room && !sess->l_interf.quit && !sess->l_interf.out_buf) 
                    session_switch_room(sess);
            }
        }
    }

    // Let the OS deinit stuff
    return 0;
}
