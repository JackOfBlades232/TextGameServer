/* TextGameServer/server.c */
#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

// @TODO: implement as framework for custom logic
// @TODO: implement variation with threads
// @TODO: add out-buf and writefds
// @TODO: add framework-wide error/disconnection messages
// @TODO: add framework-wide too-long line constraint & message

#define LISTEN_QLEN 16
#define INIT_SESS_ARR_SIZE 32
#define INBUFSIZE 1024

typedef struct session_tag {
    int fd;
    unsigned int from_ip;
    unsigned short from_port;
    char buf[INBUFSIZE];
    int buf_used;
    bool quit;

    // @TODO: add pointer to session state
} session;

typedef struct server_tag {
    int ls;
    session **sessions;
    int sessions_size;

    // @TODO: add pointer to server state
} server;

void session_send_str(session *sess, const char *str)
{
    write(sess->fd, str, strlen(str));
}

session *make_session(int fd, unsigned int from_ip, unsigned short from_port)
{
    session *sess = malloc(sizeof(session));
    sess->fd = fd;
    sess->from_ip = ntohl(from_ip);
    sess->from_port = ntohs(from_port);
    sess->buf_used = 0;
    sess->quit = false;

    return sess;
}

void cleanup_session(session *sess)
{
    // @TODO: clean resources
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

    // @TODO: call into logic

    free(line);
}

bool session_do_read(session *sess)
{
    int rc, bufp = sess->buf_used;
    rc = read(sess->fd, sess->buf + bufp, INBUFSIZE-bufp);
    if (rc <= 0) {
        // @TODO: handle disconnection
        sess->quit = true;
        return false;
    }

    sess->buf_used += rc;
    session_check_lf(sess);
    if (sess->buf_used == INBUFSIZE) {
        // @TODO: handle too long line
        sess->quit = true;
        return false;
    }

    if (sess->quit)
        return false;
    return true;
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
}

void server_accept_client(server *serv)
{
    int sd, i;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    sd = accept(serv->ls, (struct sockaddr *) &addr, &len);
    ASSERT_ERR(sd >= 0);

    if (sd >= serv->sessions_size) { // resize if needed
        int newsize = serv->sessions_size;
        while (newsize <= sd)
            newsize += INIT_SESS_ARR_SIZE;
        serv->sessions = 
            realloc(serv->sessions, newsize * sizeof(*serv->sessions));
        for (i = serv->sessions_size; i < newsize; i++)
            serv->sessions[i] = NULL;
        serv->sessions_size = newsize;
    }

    serv->sessions[sd] = make_session(sd, addr.sin_addr.s_addr, addr.sin_port);
}

void server_close_session(server *serv, int sd)
{
    close(sd);
    cleanup_session(serv->sessions[sd]);
    free(serv->sessions[sd]);
    serv->sessions[sd] = NULL;
}

int main(int argc, char **argv) 
{
    server serv;
    long port;
    char *endptr;

    ASSERTF(argc == 2, "Args: <port>\n");

    port = strtol(argv[1], &endptr, 10);
    ASSERTF(*argv[1] && !*endptr, "Invalid port number\n");
        
    server_init(&serv, port);

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serv.ls, &readfds);

        int maxfd = serv.ls;
        for (int i = 0; i < serv.sessions_size; i++) {
            if (serv.sessions[i]) {
                FD_SET(i, &readfds);
                if (i > maxfd)
                    maxfd = i;
            }
        }

        int sr = select(maxfd+1, &readfds, NULL, NULL, NULL);
        ASSERT_ERR(sr >= 0);

        if (FD_ISSET(serv.ls, &readfds))
            server_accept_client(&serv);
        for (int i = 0; i < serv.sessions_size; i++) {
            if (serv.sessions[i] && FD_ISSET(i, &readfds)) {
                int ssr = session_do_read(serv.sessions[i]);
                if (!ssr)
                    server_close_session(&serv, i);
            }
        }
    }

    return 0;
}
