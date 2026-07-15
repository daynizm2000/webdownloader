#include "../include/webd.h"
#include <strings.h>
#include <string.h>
#include <ctype.h>


#define WEBD_HTTP_PORT_STR "80"
#define WEBD_HTTPS_PORT_STR "443"


static webd_errno_t webd_url_parse_protocol(webd_url_t *urlinfo)
{
        const char *end;
        const char *url = webd_str_data(urlinfo->url);
        size_t len;


        end = strstr(url, "://");

        if (!end)
                return WEBD_FAILURE;


        len = end - url;


        if (len == webd_static_strlen("https") && strncasecmp(url, "https", len) == 0)
                urlinfo->protocol = WEBD_PROTO_HTTPS;
        else if (len == webd_static_strlen("http") && strncasecmp(url, "http", len) == 0)
                urlinfo->protocol = WEBD_PROTO_HTTP;
        else
                return WEBD_FAILURE;


        return WEBD_SUCCESS;
}


static webd_errno_t webd_url_parse_host(webd_url_t *urlinfo)
{
        const char *start;
        const char *url = webd_str_data(urlinfo->url);
        size_t len;


        start = strstr(url, "://");

        if (!start)
                return WEBD_FAILURE;


        start += webd_static_strlen("://");
        len = 0;


        while (start[len] && start[len] != '/'
                && start[len] != ':'
                && start[len] != '?'
                && start[len] != '#') {
                        len++;
        }


        if (!len)
                return WEBD_FAILURE;


        webd_strview_set(urlinfo->host, start, len);


        return WEBD_SUCCESS;
}


static webd_errno_t webd_url_parse_file_path(webd_url_t *urlinfo)
{
        const char *start = webd_strview_data(urlinfo->host) + webd_strview_len(urlinfo->host);
        size_t len = 0;


        if (!*start || *start == '#')
                goto not_path;


        if (*start == ':') {
                while (*start && *start != '/' && *start != '?') {
                        if (*start == '#')
                                goto not_path;


                        start++;
                }

                if (!*start)
                        goto not_path;
        }

        
        while (start[len] && start[len] != '#')
                len++;


        if (*start == '/') {
                if (len == 1)
                        goto not_path;


                start++;
                len--;
        }


        webd_strview_set(urlinfo->file_path, start, len);


        return WEBD_SUCCESS;

not_path:

        webd_strview_set(urlinfo->file_path, "", 0);
        return WEBD_SUCCESS;
}


static inline void webd_url_port_set_by_protocol(webd_url_t *urlinfo)
{
        switch ((int)urlinfo->protocol) {
                case WEBD_PROTO_HTTP: {
                        webd_strview_set(urlinfo->port, WEBD_HTTP_PORT_STR,
                                webd_static_strlen(WEBD_HTTP_PORT_STR));

                        break;
                }
                case WEBD_PROTO_HTTPS: {
                        webd_strview_set(urlinfo->port, WEBD_HTTPS_PORT_STR,
                                webd_static_strlen(WEBD_HTTPS_PORT_STR));
                
                        break;
                }
        }
}


static webd_errno_t webd_url_parse_port(webd_url_t *urlinfo)
{
        const char *start = webd_strview_data(urlinfo->host) + webd_strview_len(urlinfo->host);
        size_t len = 0;


        if (*start != ':') {
                webd_url_port_set_by_protocol(urlinfo);
                return WEBD_SUCCESS;
        }


        start++;


        while (start[len] && start[len] != '/'
                && start[len] != '?'
                && start[len] != '#') {
                        if (!isdigit(start[len]))
                                return WEBD_FAILURE;

                        len++;
        }


        if (!len || len > 5) // 65535 max value port, that is, 5 digits
                return WEBD_FAILURE;


        webd_strview_set(urlinfo->port, start, len);


        return WEBD_SUCCESS;
}


webd_errno_t webd_url_init(webd_url_t *urlinfo, webd_str_t *url)
{
        const char *urldata;


        if (!urlinfo || !url)
                return WEBD_FAILURE;

        
        urlinfo->url = *url;
        urldata = webd_str_data(*url);


        if (webd_url_parse_protocol(urlinfo) != WEBD_SUCCESS)
                webd_uerr_goto(urldata, fail, "Invalid URL format\n");


        if (webd_url_parse_host(urlinfo) != WEBD_SUCCESS)
                webd_uerr_goto(urldata, fail, "Invalid URL format\n");


        if (webd_url_parse_port(urlinfo) != WEBD_SUCCESS)
                webd_uerr_goto(urldata, fail, "Invalid URL format\n");


        if (webd_url_parse_file_path(urlinfo) != WEBD_SUCCESS)
                webd_uerr_goto(urldata, fail, "Invalid URL format\n");



        urlinfo->__free = 0;


        return WEBD_SUCCESS;

fail:

        webd_str_reset(urlinfo->url);
        webd_strview_reset(urlinfo->host);
        webd_strview_reset(urlinfo->port);
        webd_strview_reset(urlinfo->file_path);

        urlinfo->__free = 1;


        return WEBD_FAILURE;
}


void webd_url_destroy(webd_url_t *urlinfo)
{
        if (!urlinfo || WEBD_OBJ_IS_FREE(*urlinfo))
                return;


        webd_strview_reset(urlinfo->file_path);
        webd_strview_reset(urlinfo->host);
        webd_strview_reset(urlinfo->port);
        
        webd_str_free(urlinfo->url);

        urlinfo->__free = 1;
}