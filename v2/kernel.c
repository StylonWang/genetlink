#include <net/genetlink.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

#include "eh_cmd.h"
#include "eh_debug.h"

#define EH_GROUP_MEMBER_MAX 32
struct EH_group {
    int group_id;

    struct EH_group_member {

        u32 pid;
        char who[EH_NAME_MAX];
        int param[EH_PARAM_MAX];
//#ifdef CONFIG_NET_NS
        struct net *        _net;
//#endif

    } members[EH_GROUP_MEMBER_MAX];
};

#define EH_HANDLE(group, member) (((group&0xEF)<<8) | (member&0xFF))
#define EH_HANDLE_GROUP(handle) ((handle>>8)&0xEF)
#define EH_HANDLE_MEMBER(handle) (handle&0xFF)

#define EH_GROUP_MAX 32
static struct EH_context {
    struct mutex lock;

    struct EH_group groups[EH_GROUP_MAX];

} g_context;


/* attribute policy: defines which attribute has which type (e.g int, char * etc)
 * possible values defined in net/netlink.h 
 */
static struct nla_policy EH_genl_policy[EH_ATTR_MAX + 1] = {
	[EH_ATTR_REGISTER] = { .type = NLA_BINARY, .len = sizeof(struct EH_message_register) },
	[EH_ATTR_UNREGISTER] = { .type = NLA_BINARY, .len = sizeof(struct EH_message_unregister) },
	[EH_ATTR_EVENT] = { .type = NLA_BINARY, .len = sizeof(struct EH_message_event) },
	[EH_ATTR_RESULT] = { .type = NLA_U32, },
};

#define VERSION_NR 1
/* family definition */
static struct genl_family EH_gnl_family = {
	.id = GENL_ID_GENERATE,         //genetlink should generate an id
	.hdrsize = 0,
	.name = "EVENT_HUB",        //the name of this family, used by userspace application
	.version = VERSION_NR,                   //version number  
	.maxattr = EH_ATTR_MAX,
};

static int EH_get_group_index(int group_id)
{
    int i;
    for(i=0; i<EH_GROUP_MAX; ++i) {
        if(0==g_context.groups[i].group_id) continue; //skip un-used slot

        if(g_context.groups[i].group_id == group_id) return i; // found slot with same group id
    }

    return -1; // not found
}

static int EH_get_member_index(int group, u32 pid)
{
    int j;
    for(j=0; j<EH_GROUP_MEMBER_MAX; ++j) {
        if(0 == g_context.groups[group].members[j].pid) continue; // skip un-used slot
        if(g_context.groups[group].members[j].pid == pid) return j; // found slot with the same pid
    }

    return -1;
}

static int EH_register(const struct EH_message_register *msg, struct genl_info *info)
{
    int group=-1, member=-1;
    int rc=0;
    int i;

    PRINT_FLOW("+\n");

    group = EH_get_group_index(msg->group_id);
    if(-1==group) {
        // allocate a slot for new group
        for(i=0; i<EH_GROUP_MAX; ++i) {
            if(0==g_context.groups[i].group_id) {
                group = i;
                break; // found new slot
            }
        }
    }
    if(-1==group) { // no available slots
        PRINT_ERR("max number of group reached.\n");
        rc = -ENOMEM;
        goto out;
    }

    member = EH_get_member_index(group, info->snd_pid);
    if(-1==member) {
        // allocate a slot for new member
        for(i=0; i<EH_GROUP_MEMBER_MAX; ++i) {
            if(0==g_context.groups[group].members[i].pid) {
                member = i;
                break; // found new slot
            }
        }
    }
    if(-1==member) {
        PRINT_ERR("max number of group members reached.\n");
        rc = -ENOMEM;
        goto out;
    }

    // initialize group
    g_context.groups[group].group_id = msg->group_id;
    // initialize group member
    g_context.groups[group].members[member].pid = info->snd_pid;
    memcpy(&g_context.groups[group].members[member].who, 
           &msg->who, sizeof(g_context.groups[group].members[member].who));
    memcpy(&g_context.groups[group].members[member].param, 
           &msg->param, sizeof(g_context.groups[group].members[member].param));

#ifdef CONFIG_NET_NS
        g_context.groups[group].members[member]._net = genl_info_net(info);
#else
        g_context.groups[group].members[member]._net =  &init_net;
#endif

    rc = EH_HANDLE(group, member); //success
    
out:
    PRINT_FLOW("- rc=0x%x\n", rc);
    return rc;
}

static void EH_cleanup_group(int group)
{
    int j=0;

    // if there are still members left, abort.
    for(j=0; j<EH_GROUP_MEMBER_MAX; ++j) {
        if(0 != g_context.groups[group].members[j].pid) return;
    }

    // no members in this group, remove this group
    PRINT_FLOW("no members left, clean up group %d\n", g_context.groups[group].group_id);
    g_context.groups[group].group_id = 0;

    return;
}

static int EH_unregister(const struct EH_message_unregister *msg, struct genl_info *info)
{
    int group=-1, member=-1;
    int rc=0;

    PRINT_FLOW("+\n");

    #if 0
    group = EH_get_group_index(msg->group_id);
    if(-1==group) { 
        PRINT_ERR("no such group.\n");
        rc = -EINVAL;
        goto out;
    }

    member = EH_get_member_index(group, info->snd_pid);
    if(-1==member) {
        PRINT_ERR("not in this group\n");
        rc = -EINVAL;
        goto out;
    }
    #else
    group = EH_HANDLE_GROUP(msg->handle);
    member = EH_HANDLE_MEMBER(msg->handle);
    if(group>=EH_GROUP_MAX || member>=EH_GROUP_MEMBER_MAX) {
        PRINT_ERR("invalid group %d member %d\n", group, member);
        rc = -EINVAL;
        goto out;
    }
    if(g_context.groups[group].members[member].pid != info->snd_pid) {
        PRINT_ERR("pid %d try to unregister pid %d\n", info->snd_pid, g_context.groups[group].members[member].pid);
        rc = -EPERM;
        goto out;
    }
    #endif

    // free member ship
    g_context.groups[group].members[member].pid = 0;
    memset(&g_context.groups[group].members[member].who, 
           0, sizeof(g_context.groups[group].members[member].who));
    memset(&g_context.groups[group].members[member].param, 
           0, sizeof(g_context.groups[group].members[member].param));

    // if all members left, remove this group
    EH_cleanup_group(group);

    rc = 0; //success
    
out:
    PRINT_FLOW("- rc=%d\n", rc);
    return rc;
}

static int EH_send_event_to(int group, int member, const struct EH_message_event *msg, struct net *net)
{
    struct sk_buff *skb;
    int rc=0;
	void *msg_head;

    /* send a message back*/
    /* allocate some memory, since the size is not yet known use NLMSG_GOODSIZE*/	
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (skb == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    /* create the message headers */
    /* arguments of genlmsg_put: 
       struct sk_buff *, 
       int (sending) pid, 
       int sequence number, 
       struct genl_family *, 
       int flags, 
       u8 command index (why do we need this?)
    */
   	msg_head = genlmsg_put(skb, 0, 0 /*info->snd_seq+1*/, &EH_gnl_family, 0, EH_CMD_EVENT);
    if (msg_head == NULL) {
        rc = -ENOMEM;
        goto out;
    }
    rc = nla_put(skb, EH_ATTR_EVENT, sizeof(*msg), msg);
    if (rc != 0) {
        goto out;
    }

    /* finalize the message */
    genlmsg_end(skb, msg_head);

    /* send the message back */
    rc = genlmsg_unicast(net /*genl_info_net(info)*/, skb, g_context.groups[group].members[member].pid);
    if (rc != 0) {
       goto out;
    }

out:
    return rc;
}

// TODO: make 2 versions: one for user-space sender and another for kernel-space sender
static int EH_user_send_event_to_group(const struct EH_message_event *msg /*struct genl_info *info*/)
{
    int group=-1, member=-1;
    int rc=0;
    int i;

    PRINT_FLOW("+\n");

#if 0
    group = EH_get_group_index(msg->to_group_id);
    if(-1==group) { 
        PRINT_ERR("no such group.\n");
        rc = -EINVAL;
        goto out;
    }

    member = EH_get_member_index(group, pid);
    if(-1==member) {
        PRINT_ERR("not in this group\n");
        rc = -EINVAL;
        goto out;
    }
#else
    group = EH_HANDLE_GROUP(msg->handle);
    member = EH_HANDLE_MEMBER(msg->handle);
    if(group>=EH_GROUP_MAX || member>=EH_GROUP_MEMBER_MAX) {
        rc = -EINVAL;
        goto out;
    }
#endif

    for(i=0; i<EH_GROUP_MEMBER_MAX; ++i) {
        if(i==member) continue; // do not send to myself

        rc = EH_send_event_to(group, member, msg, g_context.groups[group].members[member]._net);
        if(rc) {
            PRINT_ERR("cannot send event to %s %ld\n", 
                      g_context.groups[group].members[member].who,
                      (unsigned long)g_context.groups[group].members[member].pid
                      );
        }
    }

    rc = 0; //success
    
out:
    PRINT_FLOW("- rc=%d\n", rc);
    return rc;
}

static int do_cmd_register(struct sk_buff *skb_2, struct genl_info *info)
{
    struct nlattr *na;
    struct sk_buff *skb;
    int rc=0;
    u32 result;
	void *msg_head;
    struct EH_message_register *msg;

    if (info == NULL) {
        PRINT_ERR("info is null\n"); 
        goto out;
    }
    
    //printk("cmd=%d\n", info->genlhdr->cmd);

    /*for each attribute there is an index in info->attrs which points to a nlattr structure
     *in this structure the data is given
     */
    na = info->attrs[EH_ATTR_REGISTER];
   	if (na) {
        msg = (struct EH_message_register *)nla_data(na);
        if (msg == NULL) {
            PRINT_ERR("error while receiving data\n");
            rc = -EINVAL;
            goto out;
        }
        if ( msg->group_id <=0 ) {
            rc = -EINVAL;
            goto out; 
        }
	}
    else {
	    PRINT_ERR("'register' no info->attrs\n");
        goto out;
    }

    result = EH_register(msg, info);

    /* send a message back*/
    /* allocate some memory, since the size is not yet known use NLMSG_GOODSIZE*/	
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (skb == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    /* create the message headers */
    /* arguments of genlmsg_put: 
       struct sk_buff *, 
       int (sending) pid, 
       int sequence number, 
       struct genl_family *, 
       int flags, 
       u8 command index (why do we need this?)
    */
   	msg_head = genlmsg_put(skb, 0, info->snd_seq+1, &EH_gnl_family, 0, EH_CMD_REGISTER);
    if (msg_head == NULL) {
        rc = -ENOMEM;
        goto out;
    }
    rc = nla_put_u32(skb, EH_ATTR_RESULT, result);
    if (rc != 0) {
        goto out;
    }

    /* finalize the message */
    genlmsg_end(skb, msg_head);

    /* send the message back */
    rc = genlmsg_unicast(genl_info_net(info), skb, info->snd_pid );
    if (rc != 0) {
        goto out;
    }
    return 0;

out:
    PRINT_FLOW("- rc=%d\n", rc);
    return rc;
}

static int do_cmd_unregister(struct sk_buff *skb_2, struct genl_info *info)
{
    struct nlattr *na;
    struct sk_buff *skb;
    int rc = 0;
    u32 result;
	void *msg_head;
    struct EH_message_unregister *msg;

    PRINT_FLOW("+\n");

    if (info == NULL) {
        PRINT_ERR("info is null\n"); 
        goto out;
    }
    
    //printk("cmd=%d\n", info->genlhdr->cmd);

    /*for each attribute there is an index in info->attrs which points to a nlattr structure
     *in this structure the data is given
     */
    na = info->attrs[EH_ATTR_UNREGISTER];
   	if (na) {
        msg = (struct EH_message_unregister *)nla_data(na);
        if (msg == NULL) {
            PRINT_ERR("error while receiving data\n");
            rc = -EINVAL;
            goto out;
        }
	}
    else {
	    PRINT_ERR("'register' no info->attrs\n");
        goto out;
    }

    result = EH_unregister(msg, info);

    /* send a message back*/
    /* allocate some memory, since the size is not yet known use NLMSG_GOODSIZE*/	
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (skb == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    /* create the message headers */
    /* arguments of genlmsg_put: 
       struct sk_buff *, 
       int (sending) pid, 
       int sequence number, 
       struct genl_family *, 
       int flags, 
       u8 command index (why do we need this?)
    */
   	msg_head = genlmsg_put(skb, 0, info->snd_seq+1, &EH_gnl_family, 0, EH_CMD_UNREGISTER);
    if (msg_head == NULL) {
        rc = -ENOMEM;
        goto out;
    }
    rc = nla_put_u32(skb, EH_ATTR_RESULT, result);
    if (rc != 0) {
        goto out;
    }

    /* finalize the message */
    genlmsg_end(skb, msg_head);

    /* send the message back */
    rc = genlmsg_unicast(genl_info_net(info), skb, info->snd_pid );
    if (rc != 0) {
        goto out;
    }
    return 0;
out:
    PRINT_FLOW("- rc=%d\n", rc);
    return rc;
}

static int do_cmd_event(struct sk_buff *skb_2, struct genl_info *info)
{
    struct nlattr *na;
    struct sk_buff *skb;
    int rc = 0;
    u32 result;
	void *msg_head;
    struct EH_message_event *msg;

    PRINT_FLOW("+\n");

    if (info == NULL) {
        PRINT_ERR("info is null\n"); 
        goto out;
    }
    
    //printk("cmd=%d\n", info->genlhdr->cmd);

    /*for each attribute there is an index in info->attrs which points to a nlattr structure
     *in this structure the data is given
     */
    na = info->attrs[EH_ATTR_EVENT];
   	if (na) {
        msg = (struct EH_message_event *)nla_data(na);
        if (msg == NULL) {
            PRINT_ERR("error while receiving data\n");
            rc = -EINVAL;
            goto out;
        }
        if ( msg->to_group_id <=0 ) {
            rc = -EINVAL;
            goto out; 
        }
	}
    else {
	    PRINT_ERR("'register' no info->attrs\n");
        goto out;
    }

    result = EH_user_send_event_to_group(msg);

    /* send a message back*/
    /* allocate some memory, since the size is not yet known use NLMSG_GOODSIZE*/	
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (skb == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    /* create the message headers */
    /* arguments of genlmsg_put: 
       struct sk_buff *, 
       int (sending) pid, 
       int sequence number, 
       struct genl_family *, 
       int flags, 
       u8 command index (why do we need this?)
    */
   	msg_head = genlmsg_put(skb, 0, info->snd_seq+1, &EH_gnl_family, 0, EH_CMD_EVENT);
    if (msg_head == NULL) {
        rc = -ENOMEM;
        goto out;
    }
    rc = nla_put_u32(skb, EH_ATTR_RESULT, result);
    if (rc != 0) {
        goto out;
    }

    /* finalize the message */
    genlmsg_end(skb, msg_head);

    /* send the message back */
    rc = genlmsg_unicast(genl_info_net(info), skb, info->snd_pid );
    if (rc != 0) {
        goto out;
    }
    return 0;
out:
    PRINT_FLOW("- rc=%d\n", rc);
    return rc;
}


/* commands: mapping between the command enumeration and the actual function*/

struct genl_ops EH_gnl_ops[] = {
    {
        .cmd = EH_CMD_REGISTER,
        .flags = 0,
        .policy = EH_genl_policy,
        .doit = do_cmd_register,
        .dumpit = NULL,
    },

    {
        .cmd = EH_CMD_UNREGISTER,
        .flags = 0,
        .policy = EH_genl_policy,
        .doit = do_cmd_unregister,
        .dumpit = NULL,
    },

    {
        .cmd = EH_CMD_EVENT,
        .flags = 0,
        .policy = EH_genl_policy,
        .doit = do_cmd_event,
        .dumpit = NULL,
    },
};

static int __init EH_kernel_init(void)
{
	int rc;

    PRINT_FLOW("+\n");

    /*register functions (commands) of the new family*/
    rc = genl_register_family_with_ops(&EH_gnl_family, EH_gnl_ops, sizeof(EH_gnl_ops)/sizeof(EH_gnl_ops[0]));
	if (rc != 0){
        PRINT_ERR("EH register ops failed: %i\n",rc);
        genl_unregister_family(&EH_gnl_family);
		goto failure;
    }

    PRINT_FLOW("- event hub driver loaded\n");
	return 0;
	
failure:
	return -1;
}

static void __exit EH_kernel_exit(void)
{
    int ret;
    size_t s;

    PRINT_FLOW("+\n");

    for(s=0; s<sizeof(EH_gnl_ops)/sizeof(EH_gnl_ops[0]); ++s) {
        ret = genl_unregister_ops(&EH_gnl_family, &EH_gnl_ops[s]);
        if(ret != 0){
            PRINT_ERR("EH unregister ops %d failed: %i\n", (int)s, ret);
        }
    }

    /*unregister the family*/
	ret = genl_unregister_family(&EH_gnl_family);
	if(ret !=0){
        PRINT_ERR("EH unregister family failed: %i\n",ret);
    }
    PRINT_FLOW("- event hub unloaded\n");
}

int EH_kernel_send_event(const char *from_who, int group_id, const struct EH_message_event *event)
{
    //TODO
}

EXPORT_SYMBOL(EH_kernel_send_event);

module_init(EH_kernel_init);
module_exit(EH_kernel_exit);
MODULE_LICENSE("GPL");

     
