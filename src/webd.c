#include "../include/webd.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>


volatile sig_atomic_t g_webd_event_loop_running = 1;
static volatile sig_atomic_t g_webd_sigstop = 0;


static void webd_sighandler(int sig)
{
        if (sig == SIGINT || sig == SIGTERM) {
                g_webd_sigstop = sig;
                g_webd_event_loop_running = 0;
        }
}


static webd_errno_t webd_conn_setup(struct webdownloader *webd,
        struct webd_connection *conn, webd_conn_init_ctx *init_ctx)
{
        struct epoll_event ev;


        if (webd_conn_init(conn, init_ctx) != WEBD_SUCCESS)
                webd_uwarn_ret(webd_str_data(init_ctx->url), WEBD_FAILURE, "Failed to initialize the connection\n");

        
        ev.data.ptr = conn;
        ev.events = WEBD_EPOLL_EVENT_FLAGS & ~EPOLLOUT;


        if (epoll_ctl(webd->epfd, EPOLL_CTL_ADD, conn->eventfd, &ev) < 0) {
                webd_conn_destroy(conn, WEBD_CONN_DESTROY_DELETE_FILE);
                webd_uwarn_ret(webd_str_data(init_ctx->url), WEBD_FAILURE, "Failed to add the connection to the event pool\n");
        }


        return WEBD_SUCCESS;
}


webd_errno_t webd_init(struct webdownloader *webd, const char **urls, size_t count)
{
        int i;
        int j;

        if (!webd || !count)
                return WEBD_FAILURE;


        memset(webd, 0, sizeof(struct webdownloader));


        if (webd_tls_init(webd) != WEBD_SUCCESS)
                webd_err_ret(WEBD_FAILURE, "Error initializing the TLS\n");


        if (webd_dns_init(webd) != WEBD_SUCCESS)
                webd_err_goto(fail, "Error initializing the DNS handler\n");


        webd->conns = malloc(sizeof(struct webd_connection) * count);
        
        if (!webd->conns)
                webd_err_ret(WEBD_FAILURE, "Memory allocation error\n");


        webd->epfd = epoll_create1(0);

        if (webd->epfd < 0)
                webd_err_goto(fail, "Failed to epoll create\n");


        webd_log("Event pool successfully created\n");
        

        for (i = 0, j = 0; j < (int)count; j++) {
                webd_conn_init_ctx init_ctx;


                webd_str_init(init_ctx.url, (char*)urls[j], NULL);
                init_ctx.ssl_ctx = webd->ssl_ctx;
                init_ctx.index = i;
                init_ctx.dns_queue = &webd->dns.queue;


                if (webd_conn_setup(webd, &webd->conns[i], &init_ctx) != WEBD_SUCCESS)
                        continue;


                i++;
        }


        webd->count = i;


        if (!webd->count)
                webd_err_goto(fail, "Could not establish a connection to any of the servers\n");


        webd_log("Successfully initialized %zu client(s)\n", webd->count);


        return WEBD_SUCCESS;

fail:

        if (webd->conns) {
                free(webd->conns);
                webd->conns = NULL;
        }

        if (webd->epfd >= 0) {
                close(webd->epfd);
                webd->epfd = -1;
        }


        webd_tls_destroy(webd);
        webd_dns_destroy(webd);


        return WEBD_FAILURE;
}


webd_errno_t webd_accept_step_status(struct webdownloader *webd, struct webd_connection *conn, webd_errno_t ret)
{
        const char *base_url = webd_str_data(conn->urlinfo.url);


        switch ((int)ret) {
                case WEBD_RECONNECT: {
                        webd_ulog(base_url, "Reconnecting...\n");
                        
                        return webd_conn_reconnect(webd, conn);
                }
                case WEBD_REDIRECT: {
                        webd_str_t strobj_url;
                        unsigned index;
                        webd_conn_init_ctx init_ctx;
                        webd_errno_t ret;


                        strobj_url = conn->read_struct.http_res.location;
                        index = conn->__index;



                        webd_ulog(base_url, "Switching the connection to another address provided by the server\n");
                        webd_ulog(webd_str_data(strobj_url), "Server address changed to %s\n", webd_str_data(strobj_url));


                        webd_conn_destroy(conn, WEBD_CONN_DESTROY_DELETE_FILE);


                        init_ctx.ssl_ctx = webd->ssl_ctx;
                        init_ctx.url = strobj_url;
                        init_ctx.dns_queue = &webd->dns.queue;
                        init_ctx.index = index;


                        ret = webd_conn_setup(webd, conn, &init_ctx);

                        if (ret != WEBD_SUCCESS) {
                                webd_str_free(strobj_url);
                                return ret;
                        }


                        return WEBD_BUSY;
                }
                case WEBD_EPOLL_ADD_BUSY:
                case WEBD_EPOLL_ADD: {
                        struct epoll_event ev = {
                                .events = WEBD_EPOLL_EVENT_FLAGS,
                                .data.ptr = conn
                        };


                        if (epoll_ctl(webd->epfd, EPOLL_CTL_ADD, conn->sockfd, &ev) < 0)
                                webd_uwarn_ret(base_url, WEBD_FAILURE, "Failed to add the connection to the event pool\n");


                        return (ret == WEBD_EPOLL_ADD) ? WEBD_SUCCESS : WEBD_BUSY;
                }
        }


        return ret;
}


webd_errno_t webd_event_loop(struct webdownloader *webd)
{
        int closed;
        struct sigaction sa = {0};


        if (!webd)
                return WEBD_FAILURE;


        sa.sa_handler = webd_sighandler;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);


        closed = 0;


        while (g_webd_event_loop_running && closed < (int)webd->count) {
                int n;
                struct epoll_event events[WEBD_EPOLL_MAX_EVENTS];


                n = epoll_wait(webd->epfd, events, WEBD_EPOLL_MAX_EVENTS, -1);

                if (n < 0) {
                        if (errno == EINTR)
                                continue;

                        webd_err_goto(fail, "An error occurred while waiting for an event from the server(s)\n");
                }


                for (int i = 0; i < n; i++) {
                        struct webd_connection *conn;
                        struct epoll_event *out_ev;
                        const char *url;


                        out_ev = &events[i];
                        conn = out_ev->data.ptr;
                        url = webd_str_data(conn->urlinfo.url);


                        if (out_ev->events & (EPOLLERR | EPOLLHUP)) {
                                webd_warn("Event pool: %s. Closing connection...\n",
                                        (out_ev->events & (EPOLLERR | EPOLLHUP)) ? "EPOLLERR EPOLLHUP" :
                                        (out_ev->events & EPOLLERR) ? "EPOLLERR" : "EPOLLHUP");

                                webd_conn_destroy(conn, WEBD_CONN_DESTROY_DELETE_FILE);
                                closed++;

                                continue;
                        }
                        
                        if (out_ev->events & EPOLLIN || out_ev->events & EPOLLOUT) {
                                while (g_webd_event_loop_running) {
                                        webd_errno_t ret;


                                        ret = webd_conn_step(conn);
                                        ret = webd_accept_step_status(webd, conn, ret);


                                        if (ret == WEBD_FAILURE || ret == WEBD_COMPLETED) {
                                                webd_ulog(url, "Closing connection\n");


                                                webd_conn_destroy(conn, (ret == WEBD_FAILURE) ? WEBD_CONN_DESTROY_DELETE_FILE : 0);
                                                closed++;

                                                break;
                                        }
                                        else if (ret == WEBD_SUCCESS) {
                                                continue;
                                        }
                                        else {
                                                break;
                                        }
                                }


                                continue;
                        }
                }
        }


        if (g_webd_sigstop)
                webd_log("Webdownloader stopped by %s signal\n",
                        (g_webd_sigstop == SIGINT) ? "SIGINT" : "SIGTERM");


        return WEBD_SUCCESS;

fail:

        return WEBD_FAILURE;
}


void webd_destroy(struct webdownloader *webd)
{
        webd_dns_destroy(webd);


        for (size_t i = 0; i < webd->count; i++)
                if (!WEBD_OBJ_IS_FREE(webd->conns[i]))
                        webd_conn_destroy(&webd->conns[i], 0);


        free(webd->conns);
        webd_tls_destroy(webd);

        webd_log("All connections was destroyed\n");


        if (webd->epfd >= 0)
                close(webd->epfd);

        webd_log("Epoll FD was closed\n");


        memset(webd, 0, sizeof(struct webdownloader));
        webd->epfd = -1;
}