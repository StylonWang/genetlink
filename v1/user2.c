#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#include <linux/genetlink.h>

#include "cmd.h"

/*
 * Generic macros for dealing with netlink sockets. Might be duplicated
 * elsewhere. It is recommended that commercial grade applications use
 * libnl or libnetlink and use the interfaces provided by the library
 */
#define GENLMSG_DATA(glh) ((void *)(NLMSG_DATA(glh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(glh) (NLMSG_PAYLOAD(glh, 0) - GENL_HDRLEN)
#define NLA_DATA(na) ((void *)((char*)(na) + NLA_HDRLEN))
//#define NLA_PAYLOAD(len) (len - NLA_HDRLEN)

int nl_sd;                      /*the socket */
/*
 * Create a raw netlink socket and bind
 */
static int create_nl_socket(int protocol, int groups)
{
    socklen_t addr_len;
    int fd;
    struct sockaddr_nl local;

    fd = socket(AF_NETLINK, SOCK_RAW, protocol);
    if (fd < 0) {
            perror("socket");
            return -1;
    }

    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = groups;
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0)
            goto error;

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
int get_family_id(int sd)
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
    na->nla_len = strlen("NOTIFIER_HUB") + 1 + NLA_HDRLEN;
    strcpy(NLA_DATA(na), "NOTIFIER_HUB");

    family_req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

    if (sendto_fd(sd, (char *)&family_req, family_req.n.nlmsg_len) < 0)
            return -1;

    rep_len = recv(sd, &ans, sizeof(ans), 0);
    if (rep_len < 0) {
            perror("recv");
            return -1;
    }

    /* Validate response message */
    if (!NLMSG_OK((&ans.n), rep_len)) {
            fprintf(stderr, "invalid reply message\n");
            return -1;
    }

    if (ans.n.nlmsg_type == NLMSG_ERROR) {  /* error */
            fprintf(stderr, "received error\n");
            return -1;
    }

    na = (struct nlattr *)GENLMSG_DATA(&ans);
    na = (struct nlattr *)((char *)na + NLA_ALIGN(na->nla_len));
    if (na->nla_type == CTRL_ATTR_FAMILY_ID) {
            id = *(__u16 *) NLA_DATA(na);
    }
    return id;
}

int main()
{
   nl_sd = create_nl_socket(NETLINK_GENERIC, 0);
   if (nl_sd < 0) {
           printf("create failure\n");
           return 0;
   }
   int id = get_family_id(nl_sd);
   struct {
           struct nlmsghdr n;
           struct genlmsghdr g;
           char buf[256];
   } ans;

   struct {
           struct nlmsghdr n;
           struct genlmsghdr g;
           char buf[256];
   } req;
   struct nlattr *na;

   printf("family id is %d\n", id);
   printf("total header size=%d\n", sizeof(struct nlmsghdr)+sizeof(struct genlmsghdr));

   /* Send command needed */
   req.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
   req.n.nlmsg_type = id;
   req.n.nlmsg_flags = NLM_F_REQUEST;
   req.n.nlmsg_seq = 60;
   req.n.nlmsg_pid = getpid();
   req.g.cmd = N_CMD_ECHO_BIN;          //N_CMD_ECHO_BIN;

   /*compose message */
   na = (struct nlattr *)GENLMSG_DATA(&req);
   na->nla_type = N_ATTR_MSG2;       //N_ATTR_MSG2
   struct N_message nmsg;
   memset(&nmsg, 0, sizeof(nmsg));
   snprintf(nmsg.sender, sizeof(nmsg.sender), "user2");
   nmsg.event = 3;
   nmsg.param[N_PARAM_MAX-1] = 4;
   int mlength = sizeof(nmsg);
   na->nla_len = mlength + NLA_HDRLEN; //message length
   memcpy(NLA_DATA(na), &nmsg, mlength);
   req.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

   /*send message */
   struct sockaddr_nl nladdr;
   int r;

   memset(&nladdr, 0, sizeof(nladdr));
   nladdr.nl_family = AF_NETLINK;

   r = sendto(nl_sd, (char *)&req, req.n.nlmsg_len, 0,
              (struct sockaddr *)&nladdr, sizeof(nladdr));

   int rep_len = recv(nl_sd, &ans, sizeof(ans), 0);
   /* Validate response message */
   if (ans.n.nlmsg_type == NLMSG_ERROR) {  /* error */
       struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(&ans);      
       printf("error received NACK, error=%d msg type=%d seq=%d, pid=%d - leaving \n", 
       err->error, err->msg.nlmsg_type, err->msg.nlmsg_seq, err->msg.nlmsg_pid);
       printf("my pid=%d\n", getpid());
       return -1;
   }
   if (rep_len < 0) {
           printf("error receiving reply message via Netlink \n");
           return -1;
   }
   if (!NLMSG_OK((&ans.n), rep_len)) {
           printf("invalid reply message received via Netlink\n");
           return -1;
   }

   rep_len = GENLMSG_PAYLOAD(&ans.n);
   /*parse reply message */
   na = (struct nlattr *)GENLMSG_DATA(&ans);
   //char *result = (char *)NLA_DATA(na);
   struct N_message *rmsg = (struct N_message *)NLA_DATA(na);
   printf("kernel says: %s, %d, %d\n", rmsg->sender, rmsg->event, rmsg->param[N_PARAM_MAX-1]);

   close(nl_sd);
}
