#include <net/genetlink.h>
#include <linux/module.h>
#include <linux/kernel.h>

/* attributes (variables): the index in this enum is used as a reference for the type,
 *             userspace application has to indicate the corresponding type
 *             the policy is used for security considerations 
 */
enum {
	N_ATTR_UNSPECIFIED,
	N_ATTR_MSG1,
//	N_ATTR_MSG2,
//	N_ATTR_MSG3,
    __N_ATTR_MAX,
};
#define N_ATTR_MAX (__N_ATTR_MAX - 1)

/* attribute policy: defines which attribute has which type (e.g int, char * etc)
 * possible values defined in net/netlink.h 
 */
static struct nla_policy N_genl_policy[N_ATTR_MAX + 1] = {
	[N_ATTR_MSG1] = { .type = NLA_NUL_STRING },
//	[N_ATTR_MSG2] = { .type = NLA_U16 },
//	[N_ATTR_MSG3] = { .type = NLA_U32 },
};

#define VERSION_NR 1
/* family definition */
static struct genl_family N_gnl_family = {
	.id = GENL_ID_GENERATE,         //genetlink should generate an id
	.hdrsize = 0,
	.name = "NOTIFIER_HUB",        //the name of this family, used by userspace application
	.version = VERSION_NR,                   //version number  
	.maxattr = N_ATTR_MAX,
};

/* commands: enumeration of all commands (functions), 
 * used by userspace application to identify command to be ececuted
 */
enum {
	N_CMD_UNSPEC,
	N_CMD_ECHO,
	__N_CMD_MAX,
};
#define N_CMD_MAX (__N_CMD_MAX - 1)

/* an echo command, receives a message, prints it and sends another message back */
int doc_exmpl_echo(struct sk_buff *skb_2, struct genl_info *info)
{
    struct nlattr *na;
    struct sk_buff *skb;
    int rc;
	void *msg_head;
	char * mydata;

    printk("%s +\n", __FUNCTION__);
	
    if (info == NULL)
            goto out;

    /*for each attribute there is an index in info->attrs which points to a nlattr structure
     *in this structure the data is given
     */
    na = info->attrs[N_ATTR_MSG1];
   	if (na) {
        mydata = (char *)nla_data(na);
        if (mydata == NULL)
            printk("error while receiving data\n");
        else
            printk("received: %s\n", mydata);
	}
    else
	    printk("no info->attrs %i\n", N_ATTR_MSG1);

    /* send a message back*/
    /* allocate some memory, since the size is not yet known use NLMSG_GOODSIZE*/	
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (skb == NULL)
        goto out;

    /* create the message headers */
    /* arguments of genlmsg_put: 
       struct sk_buff *, 
       int (sending) pid, 
       int sequence number, 
       struct genl_family *, 
       int flags, 
       u8 command index (why do we need this?)
    */
   	msg_head = genlmsg_put(skb, 0, info->snd_seq+1, &N_gnl_family, 0, N_CMD_ECHO);
    if (msg_head == NULL) {
        rc = -ENOMEM;
        goto out;
    }
    /* add a DOC_EXMPL_A_MSG attribute (actual value to be sent) */
    rc = nla_put_string(skb, N_ATTR_MSG1, "hello world from kernel space");
    if (rc != 0)
        goto out;

    /* finalize the message */
    genlmsg_end(skb, msg_head);

    /* send the message back */
    rc = genlmsg_unicast(genl_info_net(info), skb,info->snd_pid );
    if (rc != 0)
        goto out;
    return 0;

 out:
    printk("an error occured in doc_exmpl_echo:\n");
  
    return 0;
}
/* commands: mapping between the command enumeration and the actual function*/
struct genl_ops N_gnl_ops_echo = {
	.cmd = N_CMD_ECHO,
	.flags = 0,
	.policy = N_genl_policy,
	.doit = doc_exmpl_echo,
	.dumpit = NULL,
};

static int __init kernel_init(void)
{
	int rc;
    printk("INIT genetlink\n");
        
    /*register new family*/
	rc = genl_register_family(&N_gnl_family);
	if (rc != 0)
		goto failure;

    /*register functions (commands) of the new family*/
	rc = genl_register_ops(&N_gnl_family, &N_gnl_ops_echo);
	if (rc != 0){
        printk("register ops: %i\n",rc);
        genl_unregister_family(&N_gnl_family);
		goto failure;
    }

	return 0;
	
failure:
    printk("an error occured while inserting the generic netlink example module\n");
	return -1;
}

static void __exit kernel_exit(void)
{
    int ret;
    printk("EXIT genetlink\n");
    /*unregister the functions*/
	ret = genl_unregister_ops(&N_gnl_family, &N_gnl_ops_echo);
	if(ret != 0){
        printk("unregister ops: %i\n",ret);
        return;
    }
    /*unregister the family*/
	ret = genl_unregister_family(&N_gnl_family);
	if(ret !=0){
        printk("unregister family %i\n",ret);
    }
}


module_init(kernel_init);
module_exit(kernel_exit);
MODULE_LICENSE("GPL");

     
