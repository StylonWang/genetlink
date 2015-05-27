
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
#include <stdint.h>
#include <sys/select.h>
#include <time.h>

#include <linux/genetlink.h>

#include "eh_cmd.h"

//TODO: move these to eh_user.h

// TODO: move EH_* to eh_user.c

#define EH_DBG(fmt, args...) do { fprintf(stderr, "[%s:%d] "fmt, __FUNCTION__, __LINE__, ## args); } while(0)
//#define EH_DBG(fmt, args...) do { } while(0)

/*
 * Generic macros for dealing with netlink sockets. Might be duplicated
 * elsewhere. It is recommended that commercial grade applications use
 * libnl or libnetlink and use the interfaces provided by the library
 */
#define GENLMSG_DATA(glh) ((void *)(NLMSG_DATA(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(glh) (NLMSG_PAYLOAD(glh, 0) - GENL_HDRLEN)
#define NLA_DATA(na) ((void *)((char*)(na) + NLA_HDRLEN))

//#define EH_FAMILY_NAME "NOTIFIER_HUB"
#define EH_FAMILY_NAME "EVENT_HUB"

struct EH_user_object {
    int handle;

    int nl_sd; // socket
    int family_id; 
};


static int create_nl_socket(int protocol, int groups)
{
    socklen_t addr_len;
    int fd;
    struct sockaddr_nl local;

    fd = socket(AF_NETLINK, SOCK_RAW, protocol);
    if (fd < 0) {
        EH_DBG("socket error %s\n", strerror(errno));
        return -1;
    }

    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = groups;
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        EH_DBG("bind error %s\n", strerror(errno)); 
        goto error;
    }

    return fd;
 error:
    close(fd);
    return -1;
}

/*
 * Send netlink message to kernel
 */
int sendto_fd(int s, const char *buf, int bufLen)
{
    struct sockaddr_nl nladdr;
    int r;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;

    while ((r = sendto(s, buf, bufLen, 0, (struct sockaddr *)&nladdr,
                       sizeof(nladdr))) < bufLen) {
        if (r > 0) {
            buf += r;
            bufLen -= r;
        } else if (errno != EAGAIN)
             return -1;
    }
    return 0;
}

/*
 * Probe the controller in genetlink to find the family id
 * for the CONTROL_EXMPL family
 */
static int get_family_id(int sd)
{
    struct {
            struct nlmsghdr n;
            struct genlmsghdr g;
            char buf[256];
    } family_req;

    struct {
            struct nlmsghdr n;
            struct genlmsghdr g;
            char buf[256];
    } ans;

    int id;
    struct nlattr *na;
    int rep_len;

    /* Get family name */
    family_req.n.nlmsg_type = GENL_ID_CTRL;
    family_req.n.nlmsg_flags = NLM_F_REQUEST;
    family_req.n.nlmsg_seq = 0;
    family_req.n.nlmsg_pid = getpid();
    family_req.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    family_req.g.cmd = CTRL_CMD_GETFAMILY;
    family_req.g.version = 0x1;

    na = (struct nlattr *)GENLMSG_DATA(&family_req);
    na->nla_type = CTRL_ATTR_FAMILY_NAME;
    /*------change here--------*/
    na->nla_len = strlen(EH_FAMILY_NAME) + 1 + NLA_HDRLEN;
    strcpy((char *)NLA_DATA(na), EH_FAMILY_NAME);

    family_req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

    if (sendto_fd(sd, (char *)&family_req, family_req.n.nlmsg_len) < 0)
            return -1;

    rep_len = recv(sd, &ans, sizeof(ans), 0);
    if (rep_len < 0) {
            EH_DBG("recv error %s\n", strerror(errno));
            return -1;
    }

    /* Validate response message */
    if (!NLMSG_OK((&ans.n), rep_len)) {
            EH_DBG("invalid reply message\n");
            return -1;
    }

    if (ans.n.nlmsg_type == NLMSG_ERROR) {  /* error */
            EH_DBG("received error\n");
            return -1;
    }

    na = (struct nlattr *)GENLMSG_DATA(&ans);
    na = (struct nlattr *)((char *)na + NLA_ALIGN(na->nla_len));
    if (na->nla_type == CTRL_ATTR_FAMILY_ID) {
            id = *(__u16 *) NLA_DATA(na);
    }
    return id;
}

/*
int EH_user_init(struct EH_user_object *obj)
{
   memset(obj, 0, sizeof(*obj));

   obj->nl_sd = create_nl_socket(NETLINK_GENERIC, 0);
   if (obj->nl_sd < 0) {
           EH_DBG("create failure\n");
           return -1;
   }
   obj->family_id = get_family_id(obj->nl_sd);
   if(obj->family_id <0) {
        EH_DBG("unable to get family id\n");
        close(obj->nl_sd);
        return -1;
   }

   return 0;
}

void EH_user_exit(struct EH_user_object *obj)
{
    close(obj->nl_sd);
    memset(obj, 0, sizeof(*obj));
}
*/

int EH_user_register(struct EH_user_object *obj, const char *who, int group_id, int *param, int param_len)
{
    struct EH_message_register msg;

    struct {
            struct nlmsghdr n;
            struct genlmsghdr g;
            char buf[256];
    } req, ans;
    struct nlattr *na;

   memset(obj, 0, sizeof(*obj));

   obj->nl_sd = create_nl_socket(NETLINK_GENERIC, 0);
   if (obj->nl_sd < 0) {
           EH_DBG("create failure\n");
           return -1;
   }
   obj->family_id = get_family_id(obj->nl_sd);
   if(obj->family_id <0) {
        EH_DBG("unable to get family id\n");
        close(obj->nl_sd);
        return -1;
   }

    memset(&msg, 0, sizeof(msg));
    strncpy(msg.who, who, sizeof(msg.who));
    msg.group_id = group_id;
    memcpy(msg.param, param, (param_len<EH_PARAM_MAX)?  param_len : EH_PARAM_MAX );

   /* Send command needed */
   req.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
   req.n.nlmsg_type = obj->family_id;
   req.n.nlmsg_flags = NLM_F_REQUEST;
   req.n.nlmsg_seq = 60;
   req.n.nlmsg_pid = getpid();
   req.g.cmd = EH_CMD_REGISTER;      

   /*compose message */
   na = (struct nlattr *)GENLMSG_DATA(&req);
   na->nla_type = EH_ATTR_REGISTER;
   int mlength = sizeof(msg);
   na->nla_len = mlength + NLA_HDRLEN; //message length
   memcpy((void *)NLA_DATA(na), &msg, mlength);
   req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

   /*send message */
   struct sockaddr_nl nladdr;
   int r;

   memset(&nladdr, 0, sizeof(nladdr));
   nladdr.nl_family = AF_NETLINK;

   r = sendto(obj->nl_sd, (char *)&req, req.n.nlmsg_len, 0,
              (struct sockaddr *)&nladdr, sizeof(nladdr));

   int rep_len = recv(obj->nl_sd, &ans, sizeof(ans), 0);
   /* Validate response message */
   if (ans.n.nlmsg_type == NLMSG_ERROR) {  /* error */
       struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(&ans);      
       EH_DBG("error received NACK, error=%d msg type=%d seq=%d, pid=%d - leaving \n", 
       err->error, err->msg.nlmsg_type, err->msg.nlmsg_seq, err->msg.nlmsg_pid);
       EH_DBG("my pid=%d\n", getpid());
       return -1;
   }
   if (rep_len < 0) {
       EH_DBG("error receiving reply message via Netlink \n");
       return -1;
   }
   if (!NLMSG_OK((&ans.n), rep_len)) {
           EH_DBG("invalid reply message received via Netlink\n");
           return -1;
   }

   rep_len = GENLMSG_PAYLOAD(&ans.n);
   /*parse reply message */
   na = (struct nlattr *)GENLMSG_DATA(&ans);
   //char *result = (char *)NLA_DATA(na);
   uint32_t *result = (uint32_t *)NLA_DATA(na);
   int handle = ((int)(long)*result);

   if(handle<0) {
       EH_DBG("failed to get handle\n");
       return handle;
   }
   else {
       obj->handle = handle;
       return 0;
   }
}

void EH_user_unregister(struct EH_user_object *obj)
{
    struct EH_message_unregister msg;

    struct {
            struct nlmsghdr n;
            struct genlmsghdr g;
            char buf[256];
    } req, ans;
    struct nlattr *na;

    memset(&msg, 0, sizeof(msg));
    msg.handle = obj->handle;

    /* Send command needed */
    req.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    req.n.nlmsg_type = obj->family_id;
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_seq = 60;
    req.n.nlmsg_pid = getpid();
    req.g.cmd = EH_CMD_UNREGISTER;      

    /*compose message */
    na = (struct nlattr *)GENLMSG_DATA(&req);
    na->nla_type = EH_ATTR_UNREGISTER;
    int mlength = sizeof(msg);
    na->nla_len = mlength + NLA_HDRLEN; //message length
    memcpy((void *)NLA_DATA(na), &msg, mlength);
    req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

    /*send message */
    struct sockaddr_nl nladdr;
    int r;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;

    r = sendto(obj->nl_sd, (char *)&req, req.n.nlmsg_len, 0,
              (struct sockaddr *)&nladdr, sizeof(nladdr));

    int rep_len = recv(obj->nl_sd, &ans, sizeof(ans), 0);
    /* Validate response message */
    if (ans.n.nlmsg_type == NLMSG_ERROR) {  /* error */
       struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(&ans);      
       EH_DBG("error received NACK, error=%d msg type=%d seq=%d, pid=%d - leaving \n", 
       err->error, err->msg.nlmsg_type, err->msg.nlmsg_seq, err->msg.nlmsg_pid);
       EH_DBG("my pid=%d\n", getpid());
       return; // -1;
    }
    if (rep_len < 0) {
           EH_DBG("error receiving reply message via Netlink \n");
           return; // -1;
    }
    if (!NLMSG_OK((&ans.n), rep_len)) {
           EH_DBG("invalid reply message received via Netlink\n");
           return; // -1;
    }

    rep_len = GENLMSG_PAYLOAD(&ans.n);
    /*parse reply message */
    na = (struct nlattr *)GENLMSG_DATA(&ans);
    //char *result = (char *)NLA_DATA(na);
    uint32_t *result = (uint32_t *)NLA_DATA(na);

    close(obj->nl_sd);
    memset(obj, 0, sizeof(*obj));

    return ; //((int)(long)*result);
}

// @param timeout_milisec   >0 for waiting with timeout, <0 for blocking, ==0 for polling(no wait)
// @return <0 is error code, ==0 means no data to recv, 1 means event received
int EH_user_recv_event(struct EH_user_object *obj, struct EH_message_event *event, int timeout_milisec)
{
    struct {
            struct nlmsghdr n;
            struct genlmsghdr g;
            char buf[256];
    } ans;
    int rep_len; 
    int rc;
    fd_set rset;
    struct timeval tv;
    struct nlattr *na;

    FD_ZERO(&rset);
    FD_SET(obj->nl_sd, &rset);

    if(timeout_milisec>0) { // wait for timeout

        if(timeout_milisec>1000) {
            tv.tv_sec = timeout_milisec/1000;
        }
        tv.tv_usec = (timeout_milisec-(tv.tv_sec*1000)) *1000;

        EH_DBG("select timeout %d:%ld\n", (int)tv.tv_sec, (long)tv.tv_usec);
        rc = select(obj->nl_sd+1, &rset, NULL, NULL, &tv);

    } else if(timeout_milisec<0) { // wait indefinitely if no data

        EH_DBG("select no timeout \n");
        rc = select(obj->nl_sd+1, &rset, NULL, NULL, NULL);

    } else { // return immediately if no data
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        EH_DBG("select timeout %d:%ld\n", (int)tv.tv_sec, (long)tv.tv_usec);
        rc = select(obj->nl_sd+1, &rset, NULL, NULL, &tv);
    }

    if(rc<0) {
        EH_DBG("select failed: %s\n", strerror(errno));
        return -EIO;
    }
    else if(!FD_ISSET(obj->nl_sd, &rset) || rc==0){
        return 0;
    }
    
    rep_len = recv(obj->nl_sd, &ans, sizeof(ans), 0);
    /* Validate response message */
    if (ans.n.nlmsg_type == NLMSG_ERROR) {  /* error */
       struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(&ans);      
       EH_DBG("error received NACK, error=%d msg type=%d seq=%d, pid=%d - leaving \n", 
       err->error, err->msg.nlmsg_type, err->msg.nlmsg_seq, err->msg.nlmsg_pid);
       EH_DBG("my pid=%d\n", getpid());
       return -EIO;
    }
    if (rep_len <= 0) {
           EH_DBG("error receiving reply message via Netlink, rep_len=%d \n", rep_len);
           return -EIO;
    }
    if (!NLMSG_OK((&ans.n), rep_len)) {
           EH_DBG("invalid reply message received via Netlink\n");
           return -EIO;
    }

    rep_len = GENLMSG_PAYLOAD(&ans.n);
    /*parse reply message */
    na = (struct nlattr *)GENLMSG_DATA(&ans);
    uint32_t *result = (uint32_t *)NLA_DATA(na);
    struct EH_message_event *revent = (struct EH_message_event *)NLA_DATA(na);

    if(revent->handle != obj->handle) {
        return -EFAULT;
    }

    memcpy(event, revent, sizeof(*event));

    return 1;
}

//TODO: implement timeout in waiting for return message
int EH_user_send_event(struct EH_user_object *obj, struct EH_message_event *event, int timeout_milisec)
{
    struct {
        struct nlmsghdr n;
        struct genlmsghdr g;
        char buf[256];
    } req, ans;
    struct nlattr *na;

    event->handle = obj->handle;

    /* Send command needed */
    req.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    req.n.nlmsg_type = obj->family_id;
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.n.nlmsg_seq = 60;
    req.n.nlmsg_pid = getpid();
    req.g.cmd = EH_CMD_EVENT;      

    /*compose message */
    na = (struct nlattr *)GENLMSG_DATA(&req);
    na->nla_type = EH_ATTR_EVENT;
    int mlength = sizeof(*event);
    na->nla_len = mlength + NLA_HDRLEN; //message length
    memcpy((void *)NLA_DATA(na), event, mlength);
    req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

    /*send message */
    struct sockaddr_nl nladdr;
    int r;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;

    r = sendto(obj->nl_sd, (char *)&req, req.n.nlmsg_len, 0,
              (struct sockaddr *)&nladdr, sizeof(nladdr));

    int rep_len = recv(obj->nl_sd, &ans, sizeof(ans), 0);
    /* Validate response message */
    if (ans.n.nlmsg_type == NLMSG_ERROR) {  /* error */
       struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(&ans);      
       EH_DBG("error received NACK, error=%d msg type=%d seq=%d, pid=%d - leaving \n", 
       err->error, err->msg.nlmsg_type, err->msg.nlmsg_seq, err->msg.nlmsg_pid);
       EH_DBG("my pid=%d\n", getpid());
       return -1;
    }
    if (rep_len < 0) {
        EH_DBG("error receiving reply message via Netlink \n");
        return -1;
    }
    if (!NLMSG_OK((&ans.n), rep_len)) {
        EH_DBG("invalid reply message received via Netlink\n");
        return -1;
    }

    rep_len = GENLMSG_PAYLOAD(&ans.n);
    /*parse reply message */
    na = (struct nlattr *)GENLMSG_DATA(&ans);
    //char *result = (char *)NLA_DATA(na);
    uint32_t *result = (uint32_t *)NLA_DATA(na);
    r = ((int)(long)*result);

    EH_DBG("- r=%d\n", r);
    return r;
}

char who[EH_NAME_MAX] = "user";
int group = 254;
struct EH_user_object ehobj;
//int handle;

void signal_handler(int signo)
{
    fprintf(stderr, "signal %d received\n", signo);

    EH_user_unregister(&ehobj);
//    EH_user_exit(&ehobj);

    fprintf(stderr, "exit\n");
    exit(0);
}

int main(int argc, char **argv)
{
    int rc;
    int sender = 1;

    // parsing command line arguments
    while(1) {
        int c;

        if( (c=getopt(argc, argv, "hg:w:r")) == -1) break;

        switch(c) {
        default:
        case 'h':
        case '?':
            fprintf(stderr, "%s [-h] [-g group_id] [-w who] [-r]\n\n", argv[0]);
            exit(0);
            break;

        case 'w':
            strncpy(who, optarg, sizeof(who));
            break;

        case 'g':
            group = atoi(optarg);
            break;

        case 'r':
            sender = 0;
        }
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

/*
    rc = EH_user_init(&ehobj);
    if(rc<0) {
        fprintf(stderr, "EH failed to init: %d\n", rc);
        return -1;
    }
    fprintf(stderr, "EH inited\n");
*/

    fprintf(stderr, "PID %d register group=%d who=%s\n", getpid(), group, who);

    rc = EH_user_register(&ehobj, who, group, NULL, 0);
    if(rc<0) {
        fprintf(stderr, "EH register failed: %d\n", rc);
        return -1;
    }

    fprintf(stderr, "EH registered, handle=%x\n", ehobj.handle);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    srand(time(NULL));

    fprintf(stderr, "start %s event \n", sender? "sending" : "receiving");

/*
    struct EH_message_event sevent;
    sevent.event_id = rand() % 100;
    fprintf(stderr, "sending event %d\n", sevent.event_id);
    rc = EH_user_send_event(&ehobj, &sevent, 500);
    if(rc<0 && rc==-EAGAIN) {
    }
    else if(rc<0) {
        fprintf(stderr, "failed to send event: %s\n", strerror(rc*-1));
    }
*/

    while(1) {
        struct EH_message_event revent, sevent;
        int count=0;

        if(sender) { //sender

            sevent.event_id = rand() % 100;
            fprintf(stderr, "sending event %d\n", sevent.event_id);
            rc = EH_user_send_event(&ehobj, &sevent, 500);
            if(rc<0 && rc==-EAGAIN) {
                continue;
            }
            else if(rc<0) {
                fprintf(stderr, "failed to send event: %s\n", strerror(rc*-1));
            }

            sleep(rand()%5);
        }
        else { //receiver

            rc = EH_user_recv_event(&ehobj, &revent, 1500);
            if(rc<0 && rc==-EAGAIN) {
                continue;
            }
            else if(rc<0) {
                fprintf(stderr, "failed to recv event: %s\n", strerror(rc*-1));
                break;
            }

        }

        if(0==(count++ %4)) {
        }
        
    } // end of while

    EH_user_unregister(&ehobj);

    return 0;
}

