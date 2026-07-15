#include "../include/webd.h"
#include <fcntl.h>


void webd_fd_set_nonblock(int fd)
{
        int old_flags = fcntl(fd, F_GETFL);
        
        if (!(old_flags & O_NONBLOCK))
                fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
}