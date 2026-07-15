#define _GNU_SOURCE

#include "../include/webd.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>


webd_errno_t webd_fdownl_init(struct webd_file_downloader *webdf, struct webd_connection *conn)
{
        if (!webdf || WEBD_OBJ_IS_FREE(*webdf))
                return WEBD_FAILURE;

        
        webdf->pipefds[0] = -1;
        webdf->pipefds[1] = -1;

        
        webdf->conn = conn;
        webdf->content.pos = 0;
        webdf->content.len = 0;
        webdf->chunk.read_bytes = 0;
        webdf->chunk.written = 0;
        webdf->__free = 0;


        return WEBD_SUCCESS;
}


webd_errno_t webd_fdownl_remainder_read(struct webd_file_downloader *webdf)
{
        webd_http_response_remainder_t *rem;
        const char *url;


        if (!webdf || WEBD_OBJ_IS_FREE(*webdf))
                return WEBD_FAILURE;


        rem = &webdf->conn->read_struct.rem;
        url = webd_str_data(webdf->conn->urlinfo.url);


        //webd_ulog(url, "Starting to write server data to file...\n");


        while (webdf->content.pos < rem->len) {
                ssize_t written = write(webdf->conn->fd, rem->data + webdf->content.pos, rem->len - webdf->content.pos);


                if (written <= 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                return WEBD_BUSY; //webd_ulog_ret(url, WEBD_BUSY, "Writing a data chunk to the file has been deferred\n");

                        webd_uerr_ret(url, WEBD_FAILURE, "Error writing data to file\n");
                }


                webdf->content.pos += written;
        }


        rem->len = 0;


        //webd_ulog(url, "Data chunk successfully written to file\n");


        return WEBD_SUCCESS;
}


static webd_errno_t webd_fdownload_read_buff(struct webd_file_downloader *webdf)
{
        const char *url = webd_str_data(webdf->conn->urlinfo.url);


        while (webdf->chunk.written < webdf->chunk.read_bytes) {
                ssize_t tmp = write(webdf->conn->fd,
                        webdf->buffer + webdf->chunk.written,
                        webdf->chunk.read_bytes - webdf->chunk.written);


                if (tmp <= 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                return WEBD_BUSY; //webd_ulog_ret(url, WEBD_BUSY, "Writing a data chunk to the file has been deferred\n");


                        webd_uerr_ret(url, WEBD_FAILURE, "Error writing data to file\n");
                }


                webdf->chunk.written += tmp;
                webdf->content.pos += tmp;
        }


        webdf->chunk.read_bytes = 0;
        webdf->chunk.written = 0;


        return WEBD_SUCCESS;
}


webd_errno_t webd_fdownload(struct webd_file_downloader *webdf)
{
        const char *url;
        webd_errno_t ret;
        

        if (!webdf || WEBD_OBJ_IS_FREE(*webdf) || !webdf->conn)
                return WEBD_FAILURE;


        url = webd_str_data(webdf->conn->urlinfo.url);


        if (webdf->chunk.read_bytes) {
                ret = webd_fdownload_read_buff(webdf);


                if (ret != WEBD_SUCCESS)
                        return ret;
        }


        //webd_ulog(url, "Starting to write server data to file...\n");


        while (webdf->content.pos < webdf->content.len) {
                ssize_t tmp;
                
                
                tmp = webdf->conn->ops.read(webdf->conn, webdf->buffer, WEBD_FDOWNL_BUFSIZE);


                if (tmp < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                return WEBD_BUSY; //webd_ulog_ret(url, WEBD_BUSY, "Reading a data chunk from the server has been deferred\n");


                        webd_uerr_ret(url, WEBD_FAILURE, "Error reading data from server\n");
                }
                else if (!tmp) {
                        webd_ulog_ret(url, WEBD_RECONNECT, "The server socket closed the connection\n");
                }


                webdf->chunk.read_bytes = tmp;


                ret = webd_fdownload_read_buff(webdf);


                if (ret != WEBD_SUCCESS)
                        return ret;
        }


        //webd_ulog(url, "Data chunk successfully written to file\n");


        return WEBD_SUCCESS;
}


static webd_errno_t webd_fdownl_pipe_read(struct webd_file_downloader *webdf)
{
        ssize_t written;

        do {
                written = splice(webdf->pipefds[0], NULL, webdf->conn->fd, (off64_t*)&webdf->fpos,
                        webdf->chunk.read_bytes, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);


                if (written <= 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                return WEBD_BUSY;

                        webd_uerr_ret(webd_str_data(webdf->conn->urlinfo.url), WEBD_FAILURE, "Error writing data to file\n");
                }

                webdf->chunk.read_bytes -= written;
        } while (webdf->chunk.read_bytes && written);


        return WEBD_SUCCESS;
}


webd_errno_t webd_fdownload_splice(struct webd_file_downloader *webdf)
{
        const char *url;
        webd_errno_t ret;


        if (!webdf || WEBD_OBJ_IS_FREE(*webdf) || !webdf->conn)
                return WEBD_FAILURE;


        url = webd_str_data(webdf->conn->urlinfo.url);


        if (webdf->chunk.read_bytes) {
                ret = webd_fdownl_pipe_read(webdf);

                if (ret != WEBD_SUCCESS)
                        return ret;
        }

        if (webdf->pipefds[0] < 0 || webdf->pipefds[1] < 0)
                if (pipe(webdf->pipefds) < 0)
                        webd_uerr_ret(webd_str_data(webdf->conn->urlinfo.url), WEBD_FAILURE, "Failed to pipes create: %s\n", strerror(errno));


        //webd_ulog(url, "Starting to write server data to file...\n");


        while (webdf->content.pos < webdf->content.len) {
                ssize_t tmp;
                

                tmp = splice(webdf->conn->sockfd, NULL, webdf->pipefds[1], NULL,
                        webdf->content.len - webdf->content.pos,
                        SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

                
                if (tmp < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                return WEBD_BUSY; //webd_ulog_ret(url, WEBD_BUSY, "Writing a data chunk to the file has been deferred\n");

                        webd_uerr_ret(url, WEBD_FAILURE, "Error reading server socket\n");
                }
                else if (!tmp) {
                        webd_ulog_ret(url, WEBD_RECONNECT, "The server closed the connection\n");
                }


                webdf->chunk.read_bytes = tmp;
                webdf->content.pos += tmp;
                

                ret = webd_fdownl_pipe_read(webdf);

                if (ret != WEBD_SUCCESS)
                        return ret;


                //webd_ulog(url, "Data written from the chunk: %zu bytes\n", tmp);
        }


        //webd_ulog(url, "Data chunk successfully written to file\n");


        return WEBD_SUCCESS;
}


void webd_fdownl_destroy(struct webd_file_downloader *webdf)
{
        if (!webdf || WEBD_OBJ_IS_FREE(*webdf))
                return;

        
        if (webdf->pipefds[0] >= 0) {
                close(webdf->pipefds[0]);
                webdf->pipefds[0] = -1;
        }
        
        if (webdf->pipefds[1] >= 0) {
                close(webdf->pipefds[1]);
                webdf->pipefds[1] = -1;
        }


        webdf->content.len = 0;
        webdf->conn = NULL;
        webdf->chunk.read_bytes = 0;
        webdf->chunk.written = 0;


        webdf->__free = 1;
}