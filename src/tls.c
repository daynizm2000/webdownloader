#include "../include/webd.h"
#include <errno.h>
#include <openssl/err.h>


webd_errno_t webd_tls_init(struct webdownloader *webd)
{
        if (!webd)
                return WEBD_FAILURE;


        webd->ssl_ctx = SSL_CTX_new(TLS_client_method());

        if (!webd->ssl_ctx)
                webd_err_ret(WEBD_FAILURE, "SSL CTX Object create: memory allocation error\n");


        SSL_CTX_set_options(webd->ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
        SSL_CTX_set_min_proto_version(webd->ssl_ctx, TLS1_2_VERSION);
        SSL_CTX_set_default_verify_paths(webd->ssl_ctx);
        SSL_CTX_set_verify(webd->ssl_ctx, SSL_VERIFY_NONE, NULL);


        return WEBD_SUCCESS;
}


void webd_tls_destroy(struct webdownloader *webd)
{
        if (!webd || !webd->ssl_ctx)
                return;


        SSL_CTX_free(webd->ssl_ctx);
}


webd_errno_t webd_conn_tls_init(struct webd_connection *conn)
{
        const char *url = webd_str_data(conn->urlinfo.url);
        char h_full[WEBD_HTTP_HOST_MAXLEN + 1];


        if (!conn || WEBD_OBJ_IS_FREE(*conn) || !conn->tls.ssl_ctx)
                return WEBD_FAILURE;


        conn->tls.ssl = SSL_new(conn->tls.ssl_ctx);

        if (!conn->tls.ssl)
                webd_uerr_ret(url, WEBD_FAILURE, "SSL Object create: memory allocation error\n");


        webd_strncpy_safe(h_full, webd_strview_data(conn->urlinfo.host), webd_strview_len(conn->urlinfo.host));


        SSL_set_tlsext_host_name(conn->tls.ssl, h_full);


        return WEBD_SUCCESS;
}


void webd_conn_tls_free(struct webd_connection *conn)
{
        if (!conn || WEBD_OBJ_IS_FREE(*conn) || !conn->tls.ssl)
                return;


        if (conn->sockfd >= 0)
                SSL_shutdown(conn->tls.ssl);


        SSL_free(conn->tls.ssl);
        conn->tls.ssl = NULL;
}


ssize_t webd_conn_tls_write(struct webd_connection *conn, const void *buffer, size_t count)
{
        size_t written;
        int ret;
        

        ret = SSL_write_ex(conn->tls.ssl, buffer, count, &written);


        if (!ret) {
                ret = SSL_get_error(conn->tls.ssl, ret);

                if (ret == SSL_ERROR_WANT_WRITE || ret == SSL_ERROR_WANT_READ) {
                        errno = EAGAIN;
                        return -1;
                }
                else if ((ret == SSL_ERROR_SYSCALL && !errno) || ret == SSL_ERROR_ZERO_RETURN)
                        return 0;


                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        errno = 0;


                return -1;
        }


        return written;
}


ssize_t webd_conn_tls_read(struct webd_connection *conn, void *buffer, size_t count)
{
        size_t read_bytes;
        int ret;


        ret = SSL_read_ex(conn->tls.ssl, buffer, count, &read_bytes);


        if (!ret) {
                ret = SSL_get_error(conn->tls.ssl, ret);

                if (ret == SSL_ERROR_WANT_WRITE || ret == SSL_ERROR_WANT_READ) {
                        errno = EAGAIN;
                        return -1;
                }
                else if ((ret == SSL_ERROR_SYSCALL && !errno) || ret == SSL_ERROR_ZERO_RETURN)
                        return 0;


                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        errno = 0;


                return -1;
        }


        return read_bytes;
}


webd_errno_t webd_conn_tls_handshake(struct webd_connection *conn)
{
        const char *url = webd_str_data(conn->urlinfo.url);
        int fd;
        int ret;


        fd = SSL_get_fd(conn->tls.ssl);

        if (fd < 0)
                SSL_set_fd(conn->tls.ssl, conn->sockfd);


        ret = SSL_connect(conn->tls.ssl);

        if (ret <= 0) {
                ret = SSL_get_error(conn->tls.ssl, ret);

                if (ret == SSL_ERROR_WANT_READ || ret == SSL_ERROR_WANT_WRITE)
                        return WEBD_BUSY;
                

                webd_uerr_ret(url, WEBD_FAILURE, "TLS handshake failed\n");
        }


        conn->state = WEBD_STATE_WRITE;


        webd_ulog(url, "Successful TLS Handshake\n");


        return WEBD_SUCCESS;
}