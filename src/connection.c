#define _GNU_SOURCE


#include "../include/webd.h"
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/eventfd.h>


static webd_errno_t webd_conn_connect(struct webd_connection *conn)
{
        const char *url;


        if (!conn)
                return WEBD_FAILURE;


        url = webd_str_data(conn->urlinfo.url);


        if (connect(conn->sockfd, conn->ai_curr->ai_addr, conn->ai_curr->ai_addrlen) < 0) {
                if (errno == EAGAIN || errno == EALREADY || errno == EINPROGRESS) {
                        if (conn->state != WEBD_STATE_CONNECT)
                                conn->__resume_state = conn->state;
                        
                        conn->state = WEBD_STATE_CONNECT;
                        
                        webd_ulog_ret(url, WEBD_BUSY, "Connection postponed\n");
                }
                else if (errno == EISCONN) {
                        if (conn->state == WEBD_STATE_CONNECT)
                                conn->state = conn->__resume_state;
                        

                        webd_ulog(url, "Successfully connected to the server\n");

                        return WEBD_SUCCESS;
                }


                webd_uerr_ret(url, WEBD_FAILURE, "Failed to connect to the server\n");
        }


        if (conn->state == WEBD_STATE_CONNECT)
                conn->state = conn->__resume_state;
        

        webd_ulog(url, "Successfully connected to the server\n");


        return WEBD_SUCCESS;
}


static webd_errno_t webd_conn_server_selection(struct webd_connection *conn)
{
        const char *url;


        if (!conn)
                return WEBD_FAILURE;


        url = webd_str_data(conn->urlinfo.url);
        conn->state = conn->__resume_state;


        if (!conn->addrinfo)
                webd_uerr_ret(url, WEBD_FAILURE, "A DNS error occurred\n");


        for (conn->ai_curr = conn->addrinfo; conn->ai_curr != NULL; conn->ai_curr = conn->ai_curr->ai_next) {
                webd_errno_t ret;


                conn->sockfd = socket(conn->ai_curr->ai_family, conn->ai_curr->ai_socktype, conn->ai_curr->ai_protocol);

                if (conn->sockfd < 0) {
                        webd_ulog(url, "Failed to create socket. Let's try again...\n");
                        continue;
                }


                webd_fd_set_nonblock(conn->sockfd);


                ret = webd_conn_connect(conn);

                if (ret == WEBD_FAILURE) {
                        webd_ulog(url, "Failed to connect to the server. Let's try again...\n");
                        continue;
                }
                else if (ret == WEBD_BUSY) {
                        return WEBD_EPOLL_ADD_BUSY;
                }


                break;
        }


        if (conn->sockfd < 0) 
                webd_uerr_ret(url, WEBD_FAILURE, "Failed to connect to the server\n");


        return WEBD_EPOLL_ADD;
}


webd_errno_t webd_conn_reconnect(struct webdownloader *webd, struct webd_connection *conn)
{
        struct epoll_event ev;


        if (!webd || !conn || WEBD_OBJ_IS_FREE(*conn))
                return WEBD_FAILURE;


        close(conn->sockfd);

        
        conn->sockfd = socket(conn->ai_curr->ai_family, conn->ai_curr->ai_socktype,
                conn->ai_curr->ai_protocol);

        if (conn->sockfd < 0)
                webd_uerr_ret(webd_str_data(conn->urlinfo.url), WEBD_FAILURE, "Failed to create socket\n");


        webd_fd_set_nonblock(conn->sockfd);


        ev.data.ptr = conn;
        ev.events = WEBD_EPOLL_EVENT_FLAGS;


        if (epoll_ctl(webd->epfd, EPOLL_CTL_ADD, conn->sockfd, &ev) < 0)
                webd_uerr_ret(webd_str_data(conn->urlinfo.url), WEBD_FAILURE, "Failed to add the connection to the event pool\n");

        
        conn->write_struct.pos = 0;
        conn->read_struct.pos = 0;
        conn->read_struct.http_res.header.len = 0;


        return webd_conn_connect(conn);
}


static ssize_t webd_conn_raw_read(struct webd_connection *conn, void *buffer, size_t count)
{
        return read(conn->sockfd, buffer, count);
}


static ssize_t webd_conn_raw_write(struct webd_connection *conn, const void *buffer, size_t count)
{
        return write(conn->sockfd, buffer, count);
}


static void webd_conn_init_generate_fname(webd_url_t *urlinfo, char *dest, const char *host)
{
        if (!webd_strview_len(urlinfo->file_path)) {
                strcpy(dest, host);
                strcat(dest, ".html");
        }
        else {
                char *lsep = memrchr(webd_strview_data(urlinfo->file_path), '/', webd_strview_len(urlinfo->file_path));
                const char *path;
                size_t len;


                if (lsep) {
                        path = lsep + 1;
                        len = webd_strview_len(urlinfo->file_path) - (lsep - webd_strview_data(urlinfo->file_path)) - 1;
                }
                else {
                        path = webd_strview_data(urlinfo->file_path);
                        len = webd_strview_len(urlinfo->file_path);
                }

                if (path[len] == '/')
                        len--;

                if (*path == '?') {
                        path++;
                        len--;
                }


                if (len) {
                        webd_strncpy_safe(dest, path, len);
                }
                else {
                        strcpy(dest, host);
                        strcat(dest, ".html");
                }
        }

        if (access(dest, F_OK) == 0) {
                struct timespec rawtime;
                char *sep;
                char ext_backup[64];


                clock_gettime(CLOCK_REALTIME, &rawtime);


                sep = strrchr(dest, '.');

                if (!sep)
                        sep = dest + strlen(dest);
                else
                        strcpy(ext_backup, sep);


                sprintf(sep, "_%ld%s", rawtime.tv_nsec, (sep) ? ext_backup : "");
        }
}


static webd_errno_t webd_conn_init_setting_by_proto(struct webd_connection *conn, webd_conn_init_ctx *init_ctx)
{
        switch ((int)conn->urlinfo.protocol) {
                case WEBD_PROTO_HTTP: {
                        conn->tls.ssl = NULL;
                        conn->tls.ssl_ctx = NULL;
                        conn->state = WEBD_STATE_WRITE;


                        conn->ops.read = webd_conn_raw_read;
                        conn->ops.write = webd_conn_raw_write;
                        conn->ops.download = webd_fdownload;


                        break;
                }
                case WEBD_PROTO_HTTPS: {
                        conn->tls.ssl_ctx = init_ctx->ssl_ctx;

                        if (webd_conn_tls_init(conn) != WEBD_SUCCESS)
                                webd_uerr_ret(webd_str_data(conn->urlinfo.url), WEBD_FAILURE, "Failed to initialize TLS\n");

                        
                        conn->ops.read = webd_conn_tls_read;
                        conn->ops.write = webd_conn_tls_write;
                        conn->ops.download = webd_fdownload;

                        
                        conn->state = WEBD_STATE_TLS_HANDSHAKE;


                        break;
                }
        }


        return WEBD_SUCCESS;
}


webd_errno_t webd_conn_init(struct webd_connection *conn, webd_conn_init_ctx *init_ctx)
{
        char h_full[WEBD_HTTP_HOST_MAXLEN + 1];
        char p_full[WEBD_HTTP_PORT_MAXLEN + 1];
        webd_url_t *urlinfo;
        webd_str_t *url;


        if (!conn || !init_ctx || !init_ctx->ssl_ctx)
                return WEBD_FAILURE;


        urlinfo = &conn->urlinfo;
        conn->addrinfo = NULL;
        url = &init_ctx->url;


        if (webd_url_init(urlinfo, url) != WEBD_SUCCESS)
                webd_uerr_ret(webd_str_data(*url), WEBD_FAILURE, "Failed to URL initialized\n");


        conn->__free = 0;
        conn->__index = init_ctx->index;
        conn->sockfd = -1;


        webd_strncpy_safe(h_full, webd_strview_data(urlinfo->host), webd_strview_len(urlinfo->host));
        webd_strncpy_safe(p_full, webd_strview_data(urlinfo->port), webd_strview_len(urlinfo->port));


        conn->eventfd = eventfd(0, EFD_NONBLOCK);
        
        if (conn->eventfd < 0)
                webd_uerr_goto(webd_str_data(*url), fail, "Failed to eventfd create\n");


        if (webd_conn_init_setting_by_proto(conn, init_ctx) != WEBD_SUCCESS)
                goto fail;


        if (webd_dns_add_conn_to_queue(init_ctx->dns_queue, conn) != WEBD_SUCCESS)
                goto fail;


        webd_conn_init_generate_fname(urlinfo, conn->file_name, h_full);


        conn->fd = open(conn->file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (conn->fd < 0)
                webd_uerr_goto(webd_str_data(*url), fail, "Error opening file for download\n");


        webd_fd_set_nonblock(conn->fd);


        webd_http_request_init(&conn->write_struct.http_req, &conn->urlinfo);
        webd_http_response_init(&conn->read_struct.http_res);


        return WEBD_SUCCESS;

fail:

        if (!WEBD_OBJ_IS_FREE(conn->urlinfo))
                webd_url_destroy(&conn->urlinfo);

        if (conn->addrinfo) {
                freeaddrinfo(conn->addrinfo);
                conn->addrinfo = NULL;
        }

        if (conn->tls.ssl)
                webd_conn_tls_free(conn);

        if (conn->sockfd >= 0) {
                close(conn->sockfd);
                conn->sockfd = -1;
        }

        if (conn->eventfd >= 0) {
                close(conn->eventfd);
                conn->eventfd = -1;
        }

        if (conn->fd >= 0) {
                webd_delete_file(conn->file_name);
                close(conn->fd);
        }


        conn->__free = 1;

        
        return WEBD_FAILURE;
}


webd_errno_t webd_conn_step(struct webd_connection *conn)
{
        if (!conn || WEBD_OBJ_IS_FREE(*conn))
                return WEBD_FAILURE;


        switch ((int)conn->state) {
                case WEBD_STATE_DNS: {
                        return webd_conn_server_selection(conn);
                }
                case WEBD_STATE_CONNECT: {
                        return webd_conn_connect(conn);
                }
                case WEBD_STATE_TLS_HANDSHAKE: {
                        return webd_conn_tls_handshake(conn);
                }
                case WEBD_STATE_WRITE: {
                        return webd_conn_write(conn);
                }
                case WEBD_STATE_READ: {
                        return webd_conn_read(conn);
                }
        }


        return WEBD_SUCCESS;
}


static webd_errno_t webd_conn_write_req(struct webd_connection *conn)
{
        const char *url = webd_str_data(conn->urlinfo.url);
        webd_http_write_t *write_struct = &conn->write_struct;


        webd_ulog(url, "Starting to send an HTTP request...\n");


        while (write_struct->pos < write_struct->http_req.header.len) {
                ssize_t written = conn->ops.write(conn, write_struct->http_req.header.data + write_struct->pos,
                        write_struct->http_req.header.len - write_struct->pos);

                if (written < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)
                                webd_ulog_ret(url, WEBD_BUSY, "HTTP request recording deferred\n");

                        webd_uwarn_ret(url, WEBD_FAILURE, "Error writing HTTP request to server socket\n");
                }
                else if (!written) {
                        webd_ulog_ret(url, WEBD_RECONNECT, "The server socket closed the connection\n");
                }


                write_struct->pos += written;
        }


        webd_ulog(url, "HTTP request successfully sent to the server\n");


        return WEBD_SUCCESS;
}


webd_errno_t webd_conn_write(struct webd_connection *conn)
{
        const char *url;
        webd_http_write_t *write_struct;


        if (!conn || WEBD_OBJ_IS_FREE(*conn))
                return WEBD_FAILURE;


        url = webd_str_data(conn->urlinfo.url);
        write_struct = &conn->write_struct;

        
        switch ((int)write_struct->state) {
                case WEBD_WRITE_STATE_SEND_REQUEST: {
                        webd_errno_t ret;


                        if (!write_struct->http_req.header.len)
                                if (webd_http_request_header_generate(&write_struct->http_req) != WEBD_SUCCESS)
                                        webd_uerr_ret(url, WEBD_FAILURE, "Failed to generate HTTP request\n");

                        
                        ret = webd_conn_write_req(conn);

                        if (ret != WEBD_SUCCESS)
                                return ret;

                        
                        conn->state = WEBD_STATE_READ;

                        break;
                }
        }
        
        return WEBD_SUCCESS;
}


static webd_errno_t webd_conn_read_response(struct webd_connection *conn)
{
        const char *url = webd_str_data(conn->urlinfo.url);
        webd_http_read_t *read_struct = &conn->read_struct;
        webd_http_header_t *header = &read_struct->http_res.header;
        ssize_t read_bytes;


        webd_ulog(url, "Starting to read the response from the server...\n");

        
        while (1) {
                void *end;


                read_bytes = conn->ops.read(conn,
                        header->data + read_struct->pos,
                        WEBD_HTTP_HEADER_MAXSIZE - read_struct->pos);

                if (read_bytes < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                webd_ulog_ret(url, WEBD_BUSY, "Reading the response from the server has been deferred\n");

                        webd_uwarn_ret(url, WEBD_FAILURE, "Error reading HTTP response from server socket\n");
                }
                else if (!read_bytes) {
                        webd_ulog_ret(url, WEBD_RECONNECT, "The server socket closed the connection\n");
                }

                
                read_struct->pos += read_bytes;
                header->len += read_bytes;


                end = memmem(read_struct->http_res.header.data, read_struct->pos,
                        "\r\n\r\n", webd_static_strlen("\r\n\r\n"));

                if (end) {
                        header->len -= ((header->data + read_struct->pos) - ((char*)end + webd_static_strlen("\r\n\r\n")));
                        header->data[header->len - 1] = '\0';

                        read_struct->rem.data = header->data + header->len;
                        read_struct->rem.len = read_struct->pos - header->len;


                        break;
                }
        }


        webd_ulog(url, "Server response read successfully\n");

        
        return WEBD_SUCCESS;
}


static webd_errno_t webd_conn_action(struct webd_connection *conn, webd_action_t action)
{
        switch ((int)action) {
                case WEBD_ACTION_DOWNLOAD: {
                        conn->read_struct.state = WEBD_READ_STATE_DOWNLOAD;
                        return webd_conn_read(conn);
                }
                case WEBD_ACTION_ERROR: {
                        conn->read_struct.state = WEBD_READ_STATE_READ_RESPONSE;
                        return WEBD_FAILURE;
                }
                case WEBD_ACTION_REDIRECT: {
                        return WEBD_REDIRECT;
                }
                default: {
                        return WEBD_FAILURE;
                }
        }
}


static webd_errno_t webd_download_setting_by_read_mode(struct webd_connection *conn)
{
        switch ((int)conn->read_struct.http_res.read_mode) {
                case WEBD_HTTP_MODE_CONTENT_LENGTH: {
                        conn->downloader.content.len = conn->read_struct.http_res.content_len;
                        conn->downloader.ops.is_finished = webd_download_is_finished_by_content_len;


                        break;
                }
                case WEBD_HTTP_MODE_CHUNKED: {
                        conn->downloader.content.len = 0;
                        conn->downloader.ops.is_finished = webd_download_is_finished_by_chunked;
                        conn->downloader.ops.filter_buff = webd_download_filter_buff_by_chunked;


                        break;

                }
                case WEBD_HTTP_MODE_CLOSE_CONNECTION: {
                        conn->downloader.content.len = 0;
                        conn->downloader.ops.is_finished = webd_download_is_finished_by_close_conn;


                        break;
                }
        }


        return WEBD_SUCCESS;
}


webd_errno_t webd_conn_read(struct webd_connection *conn)
{
        const char *url;
        webd_http_read_t *read_struct;
        webd_errno_t ret;


        if (!conn || WEBD_OBJ_IS_FREE(*conn))
                return WEBD_FAILURE;


        url = webd_str_data(conn->urlinfo.url);
        read_struct = &conn->read_struct;

        
        switch ((int)conn->read_struct.state) {
                case WEBD_READ_STATE_READ_RESPONSE: {
                        webd_action_t action;


                        ret = webd_conn_read_response(conn);


                        if (ret != WEBD_SUCCESS)
                                return ret;


                        if (webd_http_response_parse(&read_struct->http_res) != WEBD_SUCCESS)
                                webd_uerr_ret(url, WEBD_FAILURE, "Failed to parse HTTP response\n");


                        action = webd_http_action_select(read_struct->http_res.status);


                        if (action == WEBD_ACTION_ERROR)
                                webd_uerr(url, "The HTTP server returned code %d\n", read_struct->http_res.status);

                        
                        return webd_conn_action(conn, webd_http_action_select(read_struct->http_res.status));
                }
                case WEBD_READ_STATE_DOWNLOAD: {
                        if (webd_fdownl_init(&conn->downloader, conn) != WEBD_SUCCESS)
                                webd_uerr_ret(url, WEBD_FAILURE, "Failed to initialize installer structures\n");


                        if (read_struct->rem.len) {
                                ret = webd_fdownl_remainder_read(&conn->downloader);

                                if (ret != WEBD_SUCCESS)
                                        return ret;
                        }


                        ret = webd_download_setting_by_read_mode(conn);

                        if (ret != WEBD_SUCCESS)
                                return ret;


                        ret = conn->ops.download(&conn->downloader);


                        if (ret == WEBD_SUCCESS)
                                return WEBD_COMPLETED;
                        else
                                return ret;


                        if (webd_http_request_header_generate(&conn->write_struct.http_req) != WEBD_SUCCESS)
                                webd_uerr_ret(url, WEBD_FAILURE, "Failed to generate HTTP request\n");


                        conn->state = WEBD_STATE_WRITE;
                        return webd_conn_step(conn);
                }
        }


        return WEBD_SUCCESS;
}


void webd_conn_destroy(struct webd_connection *conn, webd_conn_destroy_flags_t flags)
{
        if (!conn || WEBD_OBJ_IS_FREE(*conn))
                return;

                
        webd_conn_tls_free(conn);


        if (conn->eventfd >= 0)
                close(conn->eventfd);

        if (conn->sockfd >= 0)
                close(conn->sockfd);

        if (conn->fd >= 0) {
                if (flags & WEBD_CONN_DESTROY_DELETE_FILE)
                        webd_delete_file(conn->file_name);
                
                close(conn->fd);
        }

        if (conn->addrinfo)
                freeaddrinfo(conn->addrinfo);

        if (!WEBD_OBJ_IS_FREE(conn->downloader))
                webd_fdownl_destroy(&conn->downloader);

        
        webd_url_destroy(&conn->urlinfo);


        memset(conn, 0, sizeof(struct webd_connection));


        conn->addrinfo = NULL;
        conn->fd = -1;
        conn->sockfd = -1;
        conn->__free = 1;
}