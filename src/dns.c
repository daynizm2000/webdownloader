#include "../include/webd.h"
#include <pthread.h>
#include <netdb.h>
#include <sys/eventfd.h>
#include <unistd.h>


#define THREAD_NAME "dns_worker"


static void *webd_dns_worker(void *arg)
{
        uint64_t counter = 1;
        struct webdownloader *webd = arg;


        while (webd->dns.running) {
                unsigned int index;
                int ret;
                struct webd_connection *conn;
                char h_full[WEBD_HTTP_HOST_MAXLEN + 1];
                char p_full[WEBD_HTTP_PORT_MAXLEN + 1];


                webd_queue_wait(webd->dns.queue, !webd->dns.running);

                if (!webd->dns.running)
                        break;


                webd_queue_out(&webd->dns.queue, &index, sizeof(index));


                conn = &webd->conns[index];


                webd_strncpy_safe(h_full, webd_strview_data(conn->urlinfo.host),
                        webd_strview_len(conn->urlinfo.host));

                webd_strncpy_safe(p_full, webd_strview_data(conn->urlinfo.port),
                        webd_strview_len(conn->urlinfo.port));


                conn->__resume_state = conn->state;
                conn->state = WEBD_STATE_DNS;


                ret = getaddrinfo(h_full, p_full, NULL, &conn->addrinfo);


                if (ret != 0) {
                        webd_warn("[Thread: '%s'] failed to getaddrinfo: %s\n", THREAD_NAME, gai_strerror(ret));
                        conn->addrinfo = NULL;
                }


                write(conn->eventfd, &counter, sizeof(counter));
        }


        return NULL;
}


webd_errno_t webd_dns_add_conn_to_queue(webd_queue_t *queue, struct webd_connection *conn)
{
        return webd_queue_in(queue, &conn->__index, sizeof(conn->__index));
}


webd_errno_t webd_dns_init(struct webdownloader *webd)
{
        if (!webd)
                return WEBD_FAILURE;


        webd->dns.running = 1;


        if (webd_queue_init(&webd->dns.queue, webd->dns.__buffer, WEBD_DNS_QUEUE_BUFSIZE) != WEBD_SUCCESS)
                webd_err_ret(WEBD_FAILURE, "Failed to create a queue for DNS processing\n");


        if (pthread_create(&webd->dns.worker, NULL, webd_dns_worker, webd) < 0)
                webd_err_ret(WEBD_FAILURE, "Failed to create a thread for DNS processing\n");


        pthread_detach(webd->dns.worker);


        return WEBD_SUCCESS;
}


void webd_dns_destroy(struct webdownloader *webd)
{
        if (!webd)
                return;


        webd->dns.running = 0;
        webd_queue_destroy(&webd->dns.queue);
}