#ifndef __EH_CMD_H__
#define __EH_CMD_H__

/* attributes (variables): the index in this enum is used as a reference for the type,
 *             userspace application has to indicate the corresponding type
 *             the policy is used for security considerations 
 */
enum {
	EH_ATTR_UNSPECIFIED,
	EH_ATTR_REGISTER,
	EH_ATTR_UNREGISTER,
	EH_ATTR_EVENT,
	EH_ATTR_RESULT,
    __EH_ATTR_MAX,
};
#define EH_ATTR_MAX (__EH_ATTR_MAX - 1)


/* commands: enumeration of all commands (functions), 
 * used by userspace application to identify command to be ececuted
 */
enum {
	EH_CMD_UNSPEC,
	EH_CMD_REGISTER,
    EH_CMD_UNREGISTER,
    EH_CMD_EVENT,
	__EH_CMD_MAX,
};
#define EH_CMD_MAX (__EH_CMD_MAX - 1)

#define EH_NAME_MAX (32)
#define EH_PARAM_MAX (6)

struct EH_message_register {
    char who[EH_NAME_MAX];
    int group_id;
    int param[EH_PARAM_MAX];
};

struct EH_message_unregister {
    int handle;
};

struct EH_message_event {
    int handle;

    char from_who[EH_NAME_MAX];
    int to_group_id;
    int event_id;
    int param[EH_PARAM_MAX];
};

#endif //__EH_CMD_H__

