#include "../include/webd.h"
#include <pthread.h>


webd_errno_t webd_queue_init(webd_queue_t *queue, void *buffer, size_t bufsize)
{
        if (!queue || !buffer)
                return WEBD_FAILURE;

        if ((bufsize & (bufsize - 1)))
                return WEBD_FAILURE;


        queue->buffer = buffer;
        queue->bufsize = bufsize;
        
        pthread_cond_init(&queue->cond, NULL);
        pthread_mutex_init(&queue->mutex, NULL);

        queue->in = 0;
        queue->out = 0;


        return WEBD_SUCCESS;
}


webd_errno_t webd_queue_in(webd_queue_t *queue, void *el, size_t count)
{
        size_t in_idx;
        size_t available;


        if (!queue || !el)
                return WEBD_FAILURE;

        if (!count)
                return WEBD_SUCCESS;


        pthread_mutex_lock(&queue->mutex);


        if (count > webd_queue_available(*queue)) {
                pthread_mutex_unlock(&queue->mutex);
                return WEBD_BUSY;
        }


        in_idx = webd_queue_index(*queue, queue->in);
        available = queue->bufsize - in_idx;


        if (count <= available) {
                memcpy((unsigned char*)queue->buffer + in_idx, el, count);
        }
        else {
                memcpy((unsigned char*)queue->buffer + in_idx, el, available);
                memcpy(queue->buffer, (unsigned char*)el + available, count - available);
        }


        queue->in += count;


        pthread_cond_signal(&queue->cond);

        pthread_mutex_unlock(&queue->mutex);


        return WEBD_SUCCESS;
}


webd_errno_t webd_queue_out(webd_queue_t *queue, void *buff, size_t count)
{
        size_t out_idx;
        size_t available;


        if (!queue || !buff)
                return WEBD_FAILURE;

        if (!count)
                return WEBD_SUCCESS;


        pthread_mutex_lock(&queue->mutex);


        if (count > webd_queue_len(*queue)) {
                pthread_mutex_unlock(&queue->mutex);
                return WEBD_BUSY;
        }


        out_idx = webd_queue_index(*queue, queue->out);
        available = queue->bufsize - out_idx;


        if (count <= available) {
                memcpy(buff, (unsigned char*)queue->buffer + out_idx, count);
        }
        else {
                memcpy(buff, (unsigned char*)queue->buffer + out_idx, available);
                memcpy((unsigned char*)buff + available, queue->buffer, count - available);
        }
        

        queue->out += count;


        pthread_cond_signal(&queue->cond);

        pthread_mutex_unlock(&queue->mutex);


        return WEBD_SUCCESS;
}


void webd_queue_destroy(webd_queue_t *queue)
{
        if (!queue)
                return;


        pthread_mutex_lock(&queue->mutex);


        pthread_cond_broadcast(&queue->cond);


        pthread_mutex_unlock(&queue->mutex);


        pthread_cond_destroy(&queue->cond);
        pthread_mutex_destroy(&queue->mutex);


        memset(queue, 0, sizeof(webd_queue_t));
}