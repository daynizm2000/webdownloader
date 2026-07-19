#include "../include/webd.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>


webd_errno_t webd_fdownl_init(struct webd_file_downloader *webdf, struct webd_connection *conn)
{
        if (!webdf || WEBD_OBJ_IS_FREE(*webdf))
                return WEBD_FAILURE;

        
        webdf->conn = conn;
        webdf->content.pos = 0;
        webdf->content.len = 0;
        webdf->chunk.read_bytes = 0;
        webdf->chunk.written = 0;
        webdf->__free = 0;

        memset(&webdf->ctx, 0, sizeof(webdf->ctx));


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


        if (webdf->ops.filter_buff)
                if (webdf->ops.filter_buff(webdf, rem) != WEBD_DOWNLOAD_OPS_SUCCESS)
                        webd_uerr_ret(url, WEBD_FAILURE, "Failed to prepare the buffer for writing to the file\n");


        while (webdf->content.pos < rem->len) {
                ssize_t written;
                
                
                written = write(webdf->conn->fd, rem->data + webdf->content.pos, rem->len - webdf->content.pos);


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


        while (1) {
                ssize_t tmp;
                webd_download_ops_status_t status;
                
                
                tmp = webdf->conn->ops.read(webdf->conn, webdf->buffer, WEBD_FDOWNL_BUFSIZE);


                status = webdf->ops.is_finished(webdf, &tmp);


                if (status == WEBD_DOWNLOAD_LOOP_CONTINUE)
                        continue;
                else if (status == WEBD_DOWNLOAD_LOOP_BREAK)
                        break;


                if (tmp < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                return WEBD_BUSY; //webd_ulog_ret(url, WEBD_BUSY, "Reading a data chunk from the server has been deferred\n");


                        webd_uerr_ret(url, WEBD_FAILURE, "Error reading data from server\n");
                }
                else if (!tmp) {
                        if (webdf->ops.is_finished(webdf, &tmp) == WEBD_DOWNLOAD_LOOP_BREAK)
                                break;

                        webd_uerr_ret(url, WEBD_FAILURE, "The server socket closed the connection\n");
                }


                webdf->chunk.read_bytes = tmp;


                ret = webd_fdownload_read_buff(webdf);


                if (ret != WEBD_SUCCESS)
                        return ret;
        }


        webd_ulog(url, "Data chunk successfully written to file\n");


        return WEBD_SUCCESS;
}


void webd_fdownl_destroy(struct webd_file_downloader *webdf)
{
        if (!webdf || WEBD_OBJ_IS_FREE(*webdf))
                return;


        webdf->content.len = 0;
        webdf->conn = NULL;
        webdf->chunk.read_bytes = 0;
        webdf->chunk.written = 0;


        webdf->__free = 1;
}