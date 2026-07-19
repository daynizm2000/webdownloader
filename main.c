#include "include/webd.h"


int main(int argc, const char **argv)
{
        struct webdownloader webd;
        webd_errno_t ret;


        if (argc < 2)
                webd_err_ret(1, "Url not found. Use: %s <url/s>\n", *argv);

        
        if (webd_init(&webd, argv + 1, argc - 1) != WEBD_SUCCESS)
                return 1;

        
        webd_log("Webdownloader successfully initialized\n");


        ret = webd_event_loop(&webd);


        webd_destroy(&webd);

        webd_log("Webdownloader was destroyed\n");


        return (ret == WEBD_SUCCESS) ? 0 : 1;
}