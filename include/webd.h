#pragma once

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <openssl/ssl.h>
#include <stdbool.h>



#define WEBD_ERR "\033[0;31m[ERR]\033[0m "
#define WEBD_WARN "\033[0;33m[WARN]\033[0m "


#define webd_log(fmt, ...) fprintf(stderr, "[LOG] [%lu] " fmt, time(NULL), ##__VA_ARGS__)
#define webd_warn(fmt, ...) webd_log(WEBD_WARN "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define webd_err(fmt, ...) webd_log(WEBD_ERR "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)


#define webd_ulog(url, fmt, ...) fprintf(stderr, "[LOG] [%lu] [%s] " fmt, time(NULL), url, ##__VA_ARGS__)
#define webd_uwarn(url, fmt, ...) webd_log(WEBD_WARN "[%s:%d] [%s] " fmt, __FILE__, __LINE__, url, ##__VA_ARGS__)
#define webd_uerr(url, fmt, ...) webd_log(WEBD_ERR "[%s:%d] [%s] " fmt, __FILE__, __LINE__, url, ##__VA_ARGS__)


#define __webd_log_action(action, show_info, fmt, ...) \
        do { \
                show_info(fmt, ##__VA_ARGS__); \
                action; \
        } while (0)


#define __webd_log_ret(show_info, ret, fmt, ...) __webd_log_action(return ret, show_info, fmt, ##__VA_ARGS__)
#define __webd_log_goto(show_info, point, fmt, ...) __webd_log_action(goto point, show_info, fmt, ##__VA_ARGS__)


#define webd_log_ret(ret, fmt, ...) __webd_log_ret(webd_log, ret, fmt, ##__VA_ARGS__)
#define webd_log_goto(point, fmt, ...) __webd_log_goto(webd_log, point, fmt, ##__VA_ARGS__)

#define webd_warn_ret(ret, fmt, ...) __webd_log_ret(webd_warn, ret, fmt, ##__VA_ARGS__)
#define webd_warn_goto(point, fmt, ...) __webd_log_goto(webd_warn, point, fmt, ##__VA_ARGS__)

#define webd_err_ret(ret, fmt, ...) __webd_log_ret(webd_err, ret, fmt, ##__VA_ARGS__)
#define webd_err_goto(point, fmt, ...) __webd_log_goto(webd_err, point, fmt, ##__VA_ARGS__)


#define webd_ulog_ret(url, ret, fmt, ...) __webd_log_ret(webd_log, ret, "[%s] " fmt, url, ##__VA_ARGS__)
#define webd_ulog_goto(url, point, fmt, ...) __webd_log_goto(webd_log, point, "[%s] " fmt, url, ##__VA_ARGS__)

#define webd_uerr_ret(url, ret, fmt, ...) __webd_log_ret(webd_err, ret, "[%s] " fmt, url, ##__VA_ARGS__)
#define webd_uerr_goto(url, point, fmt, ...) __webd_log_goto(webd_err, point, "[%s] " fmt, url, ##__VA_ARGS__)

#define webd_uwarn_ret(url, ret, fmt, ...) __webd_log_ret(webd_warn, ret, "[%s] " fmt, url, ##__VA_ARGS__)
#define webd_uwarn_goto(url, point, fmt, ...) __webd_log_goto(webd_warn, point, "[%s] " fmt, url, ##__VA_ARGS__)


#define webd_strview_data(obj) ((obj).data)
#define webd_strview_len(obj) ((obj).len)

#define webd_strview_set(obj, part, length) do {        \
                (obj).data = (part);                    \
                (obj).len = (length);                   \
        } while (0)

#define webd_strview_reset(obj) do {    \
                (obj).data = NULL;      \
                (obj).len = 0;          \
        } while (0)


#define webd_strncpy_safe(dest, src, len) do {          \
                memcpy((dest), (src), (len));           \
                (dest)[(len)] = '\0';                   \
        } while (0)


#define webd_literal_data(obj) ((obj).data)
#define webd_literal_len(obj) ((obj).len)

#define webd_static_strlen(str) (sizeof(str "") - 1)


#define webd_str_is_dynamic(obj) ((obj).__free_callback)
#define webd_str_data(obj) ((obj).data)

#define webd_str_init(obj, str, free) do {              \
                (obj).data = (str);                     \
                (obj).__free_callback = (free);         \
        } while (0)

#define webd_str_reset(obj) do {                        \
                (obj).__free_callback = NULL;           \
                (obj).__free = 1;                       \
        } while (0)

#define webd_str_free(obj) do {                                                         \
                ((obj).__free_callback) ? (obj).__free_callback((obj).data) : 0;        \
                webd_str_reset((obj));                                                  \
        } while (0)


#define webd_queue_wait(queue, condition) do {                                  \
                pthread_mutex_lock(&(queue).mutex);                             \
                                                                                \
                                                                                \
                while (!(condition) && !webd_queue_len((queue)))                \
                        pthread_cond_wait(&(queue).cond, &(queue).mutex);       \
                                                                                \
                                                                                \
                pthread_mutex_unlock(&(queue).mutex);                           \
        } while (0)

#define webd_queue_len(queue) ((queue).in - (queue).out)
#define webd_queue_index(queue, num) ((num) & ((queue).bufsize - 1))
#define webd_queue_available(queue) ((queue).bufsize - webd_queue_len((queue)))



#define WEBD_EPOLL_MAX_EVENTS 1024
#define WEBD_EPOLL_EVENT_FLAGS (EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR | EPOLLHUP)

#define WEBD_HTTP_HOST_MAXLEN 253
#define WEBD_HTTP_PORT_MAXLEN 5
#define WEBD_HTTP_HEADER_MAXSIZE 8192

#define WEBD_LINUX_FILE_MAXLEN 255
#define WEBD_LINUX_FILE_PATH_MAXLEN 4096


#define WEBD_HTTP_GEN_REQ_RANGE (1 << 0)


#define WEBD_OBJ_IS_FREE(obj) ((obj).__free)


#define WEBD_HTTP_SUCCESS 200
#define WEBD_HTTP_PARTIAL_CONTENT 206
#define WEBD_HTTP_MOVED_PERMANENTLY 301


#define WEBD_FDOWNL_BUFSIZE 4096
#define WEBD_DNS_QUEUE_BUFSIZE 512



typedef enum {
        WEBD_SUCCESS,
        WEBD_BUSY,
        WEBD_RECONNECT,
        WEBD_REDIRECT,
        WEBD_EPOLL_ADD,
        WEBD_EPOLL_ADD_BUSY,
        WEBD_FAILURE
} webd_errno_t;

typedef enum {
        WEBD_PROTO_HTTP,
        WEBD_PROTO_HTTPS,
} webd_proto_t;

typedef enum {
        WEBD_STATE_DNS,
        WEBD_STATE_CONNECT,
        WEBD_STATE_TLS_HANDSHAKE,
        WEBD_STATE_WRITE,
        WEBD_STATE_READ,
} webd_state_t;

typedef enum {
        WEBD_WRITE_STATE_SEND_REQUEST,
} webd_write_state_t;

typedef enum {
        WEBD_READ_STATE_READ_RESPONSE,
        WEBD_READ_STATE_DOWNLOAD
} webd_read_state_t;

typedef enum {
        WEBD_HTTP_GET,
        WEBD_HTTP_HEAD
} webd_http_method_t;

typedef enum {
        WEBD_ACTION_REDIRECT,
        WEBD_ACTION_DOWNLOAD,
        WEBD_ACTION_ERROR
} webd_action_t;

typedef enum {
        WEBD_HTTP_MODE_CHUNKED,
        WEBD_HTTP_MODE_CONTENT_LENGTH,
        WEBD_HTTP_MODE_CONTENT_RANGE,
        WEBD_HTTP_MODE_CLOSE_CONNECTION
} webd_http_read_mode_t;


typedef struct {
        char *data;

        void (*__free_callback)(void*);
        int __free;
} webd_str_t;

typedef struct {
        const char *data;
        size_t len;
} webd_strview_t;

typedef struct {
        const char *data;
        size_t len;
} webd_literal_t;


typedef struct {
        size_t start;
        size_t end;
        size_t size;
} webd_http_range_t;

typedef struct {
        char data[WEBD_HTTP_HEADER_MAXSIZE];
        size_t len;
} webd_http_header_t;

typedef struct {
        webd_http_header_t header; // result


        // input
        webd_http_method_t method;
        webd_strview_t path;
        webd_strview_t host;


        // optional
        webd_http_range_t range;


        int __free;
} webd_http_request_t;

typedef struct {
        // input
        webd_http_header_t header;


        // output
        int status;

        size_t content_len;
        webd_http_range_t content_range;

        webd_str_t location;

        webd_http_read_mode_t read_mode;
} webd_http_response_t;

typedef struct {
        void *data;
        size_t len;
} webd_http_response_remainder_t;

typedef struct {
        size_t pos;

        webd_http_request_t http_req;
        webd_write_state_t state;
} webd_http_write_t;

typedef struct {
        size_t pos;

        webd_http_response_t http_res;
        webd_read_state_t state;

        webd_http_response_remainder_t rem;
} webd_http_read_t;



typedef struct {
        webd_str_t url;

        webd_strview_t host;
        webd_strview_t port;

        webd_strview_t file_path;

        webd_proto_t protocol;

        int __free;
} webd_url_t;


typedef struct {
        SSL *ssl;
        SSL_CTX *ssl_ctx;
} webd_conn_tls_t;


typedef struct {
        void *buffer;
        size_t bufsize;

        unsigned int in;
        unsigned int out;


        pthread_mutex_t mutex;
        pthread_cond_t  cond;
} webd_queue_t;


typedef struct {
        webd_str_t url;
        
        SSL_CTX *ssl_ctx;
        
        unsigned int index;
        
        webd_queue_t *dns_queue;
} webd_conn_init_ctx;



struct webd_file_downloader {
        union {
                int pipefds[2];
                char buffer[WEBD_FDOWNL_BUFSIZE];
        };

        struct webd_connection *conn;

        struct {
                size_t len;
                size_t pos;
        } content;

        size_t fpos;

        struct {
                size_t read_bytes;
                size_t written;
        } chunk;


        int __free;
};

struct webd_connection {
        int sockfd;
        int fd;

        webd_url_t urlinfo;

        webd_state_t state;

        webd_http_write_t write_struct;
        webd_http_read_t read_struct;

        struct addrinfo *addrinfo;
        struct addrinfo *ai_curr;

        struct webd_file_downloader downloader;


        struct {
                ssize_t (*read)(struct webd_connection *conn, void *buffer, size_t count);
                ssize_t (*write)(struct webd_connection *conn, const void *buffer, size_t count);
                webd_errno_t (*download)(struct webd_file_downloader *webdf);
        } ops;


        int eventfd;


        webd_conn_tls_t tls;


        int __free;
        webd_state_t __resume_state;
        unsigned int __index;
};

struct webdownloader {
        int epfd;

        struct webd_connection *conns;
        size_t count;


        SSL_CTX *ssl_ctx;


        struct {
                pthread_t worker;
                webd_queue_t queue;
                int __buffer[WEBD_DNS_QUEUE_BUFSIZE];

                volatile int running;
        } dns;
};



extern volatile sig_atomic_t g_webd_event_loop_running;



// webd.c
webd_errno_t webd_init(struct webdownloader *webd, const char **urls, size_t count);
webd_errno_t webd_event_loop(struct webdownloader *webd);
void webd_destroy(struct webdownloader *webd);



// connection.c
webd_errno_t webd_conn_init(struct webd_connection *conn, webd_conn_init_ctx *init_ctx);
webd_errno_t webd_conn_reconnect(struct webdownloader *webd, struct webd_connection *conn);
webd_errno_t webd_conn_step(struct webd_connection *conn);
webd_errno_t webd_conn_write(struct webd_connection *conn);
webd_errno_t webd_conn_read(struct webd_connection *conn);
void webd_conn_destroy(struct webd_connection *conn);



// url.c
webd_errno_t webd_url_init(webd_url_t *urlinfo, webd_str_t *url);
void webd_url_destroy(webd_url_t *urlinfo);



// http.c
void webd_http_request_init(webd_http_request_t *req, const webd_url_t *urlinfo);
void webd_http_response_init(webd_http_response_t *res);
void webd_http_request_destroy(webd_http_request_t *req);
void webd_http_response_destroy(webd_http_response_t *res);

webd_errno_t webd_http_request_header_generate(webd_http_request_t *req, uint8_t flags);
webd_errno_t webd_http_response_parse(webd_http_response_t *res);
webd_action_t webd_http_action_select(int status);



// parser.c
webd_errno_t webd_http_parse_status(webd_http_header_t *head, int *dest);
webd_errno_t webd_http_parse_content_len(webd_http_header_t *head, size_t *dest);
webd_errno_t webd_http_parse_content_range(webd_http_header_t *head, webd_http_range_t *dest);
webd_errno_t webd_http_parse_location(webd_http_header_t *head, webd_str_t *dest);



// downloader.c
webd_errno_t webd_fdownl_init(struct webd_file_downloader *webdf, struct webd_connection *conn);
webd_errno_t webd_fdownl_remainder_read(struct webd_file_downloader *webdf);
webd_errno_t webd_fdownload(struct webd_file_downloader *webdf);
webd_errno_t webd_fdownload_splice(struct webd_file_downloader *webdf);
void webd_fdownl_destroy(struct webd_file_downloader *webdf);



// tls.c
webd_errno_t webd_tls_init(struct webdownloader *webd);
void webd_tls_destroy(struct webdownloader *webd);
webd_errno_t webd_conn_tls_init(struct webd_connection *conn);
void webd_conn_tls_free(struct webd_connection *conn);
webd_errno_t webd_conn_tls_handshake(struct webd_connection *conn);
ssize_t webd_conn_tls_write(struct webd_connection *conn, const void *buffer, size_t count);
ssize_t webd_conn_tls_read(struct webd_connection *conn, void *buffer, size_t count);



// queue.c
webd_errno_t webd_queue_init(webd_queue_t *queue, void *buffer, size_t bufsize);
webd_errno_t webd_queue_in(webd_queue_t *queue, void *el, size_t count);
webd_errno_t webd_queue_out(webd_queue_t *queue, void *buff, size_t count);
void webd_queue_destroy(webd_queue_t *queue);



// dns.c
webd_errno_t webd_dns_add_conn_to_queue(webd_queue_t *queue, struct webd_connection *conn);
webd_errno_t webd_dns_init(struct webdownloader *webd);
void webd_dns_destroy(struct webdownloader *webd);



// utils.c
void webd_fd_set_nonblock(int fd);