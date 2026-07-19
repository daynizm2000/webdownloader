#include "../include/webd.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

webd_errno_t webd_http_parse_status(webd_http_header_t *head, int *dest)
{
        int cap;
        char *endptr;
        const char *start;
        int status;


        if (!head)
                return WEBD_FAILURE;

        if (!dest)
                dest = &cap;


        start = strchr(head->data, ' ');

        if (!start)
                return WEBD_FAILURE;

        
        while (*start && isspace(*start))
                start++;


        status = strtol(start, &endptr, 10);

        if (start == endptr)
                return WEBD_FAILURE;


        *dest = status;


        return WEBD_SUCCESS;
}


webd_errno_t webd_http_parse_content_len(webd_http_header_t *head, size_t *dest)
{
        size_t cap;
        const char *start;
        char *endptr;
        size_t cont_len;


        if (!head)
                return WEBD_FAILURE;

        if (!dest)
                dest = &cap;


        start = strcasestr(head->data, "\r\ncontent-length:");
        
        if (!start)
                return WEBD_FAILURE;


        start += webd_static_strlen("\r\ncontent-length:");


        while (*start && isspace(*start))
                start++;


        cont_len = strtoull(start, &endptr, 10);
        
        if (endptr == start)
                return WEBD_FAILURE;


        *dest = cont_len;


        return WEBD_SUCCESS;
}


webd_errno_t webd_http_parse_location(webd_http_header_t *head, webd_str_t *dest)
{
        webd_str_t cap;
        char *data;
        char *start;
        size_t len;
        

        if (!head)
                return WEBD_FAILURE;

        if (!dest)
                dest = &cap;


        len = 0;

        
        start = strcasestr(head->data, "\r\nlocation:");
        
        if (!start)
                return WEBD_FAILURE;


        start += webd_static_strlen("\r\nlocation:");


        while (*start && isspace(*start))
                start++;


        while (start[len] && !isspace(start[len]))
                len++;


        if (!len)
                return WEBD_FAILURE;


        if (dest == &cap)
                return WEBD_SUCCESS;


        data = malloc(len + 1);

        if (!data)
                webd_err_ret(WEBD_FAILURE, "Parser: memory allocation error\n");


        webd_strncpy_safe(data, start, len);

        
        webd_str_init(*dest, data, free);


        return WEBD_SUCCESS;
}


webd_errno_t webd_http_parse_transfer_encoding(webd_http_header_t *head, webd_strview_t *dest)
{
        const char *start;
        size_t len = 0;


        if (!head)
                return WEBD_FAILURE;


        start = strcasestr(head->data, "\r\ntransfer-encoding:");

        if (!start)
                return WEBD_FAILURE;


        start += webd_static_strlen("\r\ntransfer-encoding:");


        while (*start && isspace(*start))
                start++;


        while (start[len] && start[len] != '\r')
                len++;


        len--;


        if (!len)
                return WEBD_FAILURE;


        if (!dest)
                return WEBD_SUCCESS;


        webd_strview_set(*dest, start, len);


        return WEBD_SUCCESS;
}