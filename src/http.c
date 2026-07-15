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


webd_errno_t webd_http_request_header_generate(webd_http_request_t *req, uint8_t flags)
{
        char range[56];


        if (!req || WEBD_OBJ_IS_FREE(*req))
                return WEBD_FAILURE;


        if (flags & WEBD_HTTP_GEN_REQ_RANGE)
                sprintf(range, "Range: bytes=%zu-%zu\r\n",
                        req->range.start, req->range.end);


        req->method = WEBD_HTTP_GET;

        
        req->header.len = snprintf(req->header.data, WEBD_HTTP_HEADER_MAXSIZE - 1,
                                "%s /%.*s HTTP/1.1\r\n"
                                "Host: %.*s\r\n"
                                "%s"
                                "Connection: close\r\n"
                                "\r\n",
                        webd_literal_data(webd_http_method_types[req->method].name),
                        (unsigned int)webd_strview_len(req->path), webd_strview_data(req->path),
                        (unsigned int)webd_strview_len(req->host), webd_strview_data(req->host),
                        (flags & WEBD_HTTP_GEN_REQ_RANGE) ? range : "");


        return WEBD_SUCCESS;
}


webd_errno_t webd_http_response_parse(webd_http_response_t *res)
{
        if (!res)
                return WEBD_FAILURE;


        if (webd_http_parse_status(&res->header, &res->status) != WEBD_SUCCESS)
                return WEBD_FAILURE;

        
        if (res->status == WEBD_HTTP_MOVED_PERMANENTLY) {
                if (webd_http_parse_location(&res->header, &res->location) != WEBD_SUCCESS)
                        return WEBD_FAILURE;
        }

        if (res->status == WEBD_HTTP_SUCCESS || res->status == WEBD_HTTP_PARTIAL_CONTENT) {
                if (webd_http_parse_content_len(&res->header, &res->content_len) != WEBD_SUCCESS)
                        return WEBD_FAILURE;
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