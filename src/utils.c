#include "../include/webd.h"
#include <fcntl.h>
#include <unistd.h>


typedef unsigned char uchar;


void webd_fd_set_nonblock(int fd)
{
        int old_flags = fcntl(fd, F_GETFL);
        
        if (!(old_flags & O_NONBLOCK))
                fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
}


void webd_delete_file(const char *path)
{
        if (!path)
                return;

        
        unlink(path);
}


webd_download_ops_status_t webd_download_is_finished_by_content_len(struct webd_file_downloader *webdf, ssize_t *last_read_res)
{
        if (webdf->content.pos >= webdf->content.len || (last_read_res && !*last_read_res))
                return WEBD_DOWNLOAD_LOOP_BREAK;


        (void)last_read_res;

        
        return WEBD_DOWNLOAD_LOOP_PROCESS;
}


webd_download_ops_status_t webd_download_is_finished_by_close_conn(struct webd_file_downloader *webdf, ssize_t *last_read_res)
{
        if (!*last_read_res)
                return WEBD_DOWNLOAD_LOOP_BREAK;


        (void)webdf;

        
        return WEBD_DOWNLOAD_LOOP_PROCESS;
}


webd_download_ops_status_t webd_download_is_finished_by_chunked(struct webd_file_downloader *webdf, ssize_t *last_read_res)
{
        webd_errno_t ret;


        if (*last_read_res <= 0)
                return WEBD_DOWNLOAD_LOOP_PROCESS;


        ret = webd_http_chunked_buff_parse(&webdf->ctx.chunked, (unsigned char*)webdf->buffer,
                (size_t*)last_read_res);


        if (ret == WEBD_COMPLETED || ret == WEBD_FAILURE)
                return WEBD_DOWNLOAD_LOOP_BREAK;


        if (!*last_read_res)
                return WEBD_DOWNLOAD_LOOP_CONTINUE;


        return WEBD_DOWNLOAD_LOOP_PROCESS;
}


webd_download_ops_status_t webd_download_filter_buff_by_chunked(struct webd_file_downloader *webdf, void *arg)
{
        webd_http_response_remainder_t *rem;


        if (!arg)
                return WEBD_DOWNLOAD_OPS_FAILURE;


        rem = arg;


        if (webdf->content.pos)
                return WEBD_DOWNLOAD_OPS_SUCCESS;
        

        if (webd_http_chunked_buff_parse(&webdf->ctx.chunked, rem->data, &rem->len) != WEBD_SUCCESS)
                return WEBD_DOWNLOAD_OPS_FAILURE;


        return WEBD_DOWNLOAD_OPS_SUCCESS;
}