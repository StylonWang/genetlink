
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#include <linux/genetlink.h>

#include "eh_cmd.h"

// TODO: move EH_* to eh_user.[ch]

struct EH_user_object {
    int handle;

    int nl_sd; // socket
};

int EH_user_init(struct EH_user_object *obj)
{

}

void EH_user_exit(struct EH_user_object *obj)
{

}

int EH_user_register(struct EH_user_object *obj, const char *who, int group_id, int *param, int param_len)
{
    struct EH_message_register msg;

    memset(&msg, 0, sizeof(msg));
    strncpy(msg->who, who, sizeof(msg->who));
    msg->group_id = group_id;
    memcpy(msg->param, param, (param_len<EH_PARAM_AMX)?  param_len : EH_PARAM_MAX );
}

void EH_user_unregister(struct EH_user_object *obj)
{

}

int EH_user_recv_event(struct EH_user_object *obj, struct EH_message_event *event, int timeout_milisec)
{
}

int EH_user_send_event(struct EH_user_object *obj, struct EH_message_event *event, int timeout_milisec)
{

}

char who[EH_NAME_MAX] = "user";
int group = 254;
int handle = 0;

void signal_handler(int signo)
{
    EH_user_unregister(handle);
}

int main(int argc, char **argv)
{
    int rc;

    // parsing command line arguments
    while(1) {
        int c;

        if( (c=getopt(argc, argv, "hg:w:")) == -1) break;

        switch(c) {
        default:
        case 'h':
        case '?':
            usage(argc, argv);
            break;

        case 'w':
            strncpy(who, optarg, sizeof(who));
            break;

        case 'g':
            group = atoi(optarg);
            break;
        }
    }

    handle = EH_user_register(who, group, NULL, 0);
    if(handle<0) {
        fprintf(stderr, "EH register failed: %s\n", strerror(handle*-1));
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while(1) {
        struct EH_message_event event;

        rc = EH_user_recv_event(handle, &event, 1500);
        if(rc<0 && rc==-EAGAIN) {
            continue;
        }
        else if(rc<0) {
            fprintf(stderr, "failed to recv event: %s\n", strerror(rc*-1));
            break;
        }
        
    } // end of while

    return 0;
}

