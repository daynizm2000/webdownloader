#include "../include/webd.h"


static const struct {
        webd_http_method_t method; // method = index
        webd_literal_t name;
} webd_http_method_types[] = {
        {WEBD_HTTP_GET, {"GET", webd_static_strlen("GET")}},
        {WEBD_HTTP_HEAD, {"HEAD", webd_static_strlen("HEAD")}}
};


void webd_http_request_init(webd_http_request_t *req, const webd_url_t *urlinfo)
{
        if (!req)
                return;

        
        req->method = WEBD_HTTP_GET;

        req->path = urlinfo->file_path;
        req->host = urlinfo->host;

        req->range.start = 0;
        req->range.end = 0;
        req->range.size = 0;

        req->__free = 0;
}


void webd_http_response_init(webd_http_response_t *res)
{
        if (!res)
                return;


        res->content_len = 0;
        res->status = WEBD_HTTP_SUCCESS;
}


webd_errno_t webd_http_request_header_generate(webd_http_request_t *req)
{
        if (!req || WEBD_OBJ_IS_FREE(*req))
                return WEBD_FAILURE;


        req->method = WEBD_HTTP_GET;

        
        req->header.len = snprintf(req->header.data, WEBD_HTTP_HEADER_MAXSIZE - 1,
                                "%s /%.*s HTTP/1.0\r\n"
                                "Host: %.*s\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                        webd_literal_data(webd_http_method_types[req->method].name),
                        (unsigned int)webd_strview_len(req->path), webd_strview_data(req->path),
                        (unsigned int)webd_strview_len(req->host), webd_strview_data(req->host));


        return WEBD_SUCCESS;
}


webd_errno_t webd_http_response_parse(webd_http_response_t *res)
{
        webd_strview_t trans_enc_fmt;


        if (!res)
                return WEBD_FAILURE;


        if (webd_http_parse_status(&res->header, &res->status) != WEBD_SUCCESS)
                return WEBD_FAILURE;

        
        if (res->status == WEBD_HTTP_MOVED_PERMANENTLY) {
                if (webd_http_parse_location(&res->header, &res->location) != WEBD_SUCCESS)
                        return WEBD_FAILURE;
        }

        
        if (webd_http_parse_content_len(&res->header, &res->content_len) == WEBD_SUCCESS) {
                res->read_mode = WEBD_HTTP_MODE_CONTENT_LENGTH;
        }
        else if (webd_http_parse_transfer_encoding(&res->header, &trans_enc_fmt) == WEBD_SUCCESS) {
                int ret = webd_strview_len(trans_enc_fmt) == webd_static_strlen("chunked") &&
                                strncasecmp(webd_strview_data(trans_enc_fmt), "chunked", webd_strview_len(trans_enc_fmt));


                if (ret == 0) {
                        res->read_mode = WEBD_HTTP_MODE_CHUNKED;


                        return WEBD_SUCCESS;
                }
        

                return WEBD_FAILURE;
        }
        else {
                res->read_mode = WEBD_HTTP_MODE_CLOSE_CONNECTION;
        }


        return WEBD_SUCCESS;
}


webd_action_t webd_http_action_select(int status)
{
        switch (status) {
                case WEBD_HTTP_SUCCESS: {
                        return WEBD_ACTION_DOWNLOAD;
                }
                case WEBD_HTTP_MOVED_PERMANENTLY: {
                        return WEBD_ACTION_REDIRECT;
                }
                default: {
                        return WEBD_ACTION_ERROR;
                }
        }
}


// AI-GENERATED
webd_errno_t webd_http_chunked_buff_parse(webd_http_chunked_t *ctx,
        unsigned char *buff, size_t *bufflen)
{
        if (!buff || !ctx)
                return WEBD_FAILURE;


        while (*bufflen > 0) {
                size_t to_copy;
                size_t chunk_len;
                char *endptr;
                size_t header_len;
                unsigned char *end_sep;
                unsigned char *param_sep;

                
                if (*bufflen >= 2 && memcmp(buff, "\r\n", 2) == 0) {
                        memmove(buff, buff + 2, *bufflen - 2);
                        *bufflen -= 2;


                        continue;
                }

                
                end_sep = memmem(buff, *bufflen, "\r\n", 2);

                if (!end_sep) {
                        return WEBD_SUCCESS;
                }


                param_sep = memchr(buff, ';', end_sep - buff);

                if (param_sep) {
                        to_copy = param_sep - buff;
                } else {
                        to_copy = end_sep - buff;
                }


                if (to_copy >= sizeof(ctx->hex_buff) - 1)
                        return WEBD_FAILURE;


                memcpy(ctx->hex_buff, buff, to_copy);
                ctx->hex_buff[to_copy] = '\0';


                chunk_len = strtoull((const char*)ctx->hex_buff, &endptr, 16);

                if (endptr == (char*)ctx->hex_buff)
                        return WEBD_FAILURE;


                header_len = (end_sep - buff) + 2;


                memmove(buff, buff + header_len, *bufflen - header_len);
                *bufflen -= header_len;


                if (chunk_len == 0) {
                        if (*bufflen >= 2 && memcmp(buff, "\r\n", 2) == 0) {
                                memmove(buff, buff + 2, *bufflen - 2);
                                *bufflen -= 2;
                        }


                        return WEBD_COMPLETED;
                }


                if (*bufflen >= chunk_len) {
                        size_t bytes_to_consume = chunk_len;
                        

                        if (*bufflen >= chunk_len + 2 && memcmp(buff + chunk_len, "\r\n", 2) == 0) {
                                bytes_to_consume += 2;
                        }


                        memmove(buff, buff + bytes_to_consume, *bufflen - bytes_to_consume);
                        *bufflen -= bytes_to_consume;
                } else {
                        return WEBD_SUCCESS;
                }
        }

        
        return WEBD_SUCCESS;
}
// AI-GENERATED


void webd_http_request_destroy(webd_http_request_t *req)
{
        if (!req || WEBD_OBJ_IS_FREE(*req))
                return;

        
        webd_strview_reset((req->path));
        req->range.start = 0;
        req->range.end = 0;
        req->__free = 1;
}


void webd_http_response_destroy(webd_http_response_t *res)
{
        if (!res)
                return;

        
        res->content_len = 0;
        res->status = WEBD_HTTP_SUCCESS;
}