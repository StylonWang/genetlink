#ifndef __CMD_H__
#define __CMD_H__

/* attributes (variables): the index in this enum is used as a reference for the type,
 *             userspace application has to indicate the corresponding type
 *             the policy is used for security considerations 
 */
enum {
	N_ATTR_UNSPECIFIED,
	N_ATTR_MSG1,
	N_ATTR_MSG2,
	N_ATTR_MSG3,
    __N_ATTR_MAX,
};
#define N_ATTR_MAX (__N_ATTR_MAX - 1)


/* commands: enumeration of all commands (functions), 
 * used by userspace application to identify command to be ececuted
 */
enum {
	N_CMD_UNSPEC,
	N_CMD_ECHO,
    N_CMD_ECHO_BIN,
	__N_CMD_MAX,
};
#define N_CMD_MAX (__N_CMD_MAX - 1)

#define N_NAME_MAX (32)
#define N_PARAM_MAX (6)
struct N_message {
    char sender[N_NAME_MAX];
    int event;
    int param[N_PARAM_MAX];
};

#endif //__CMD_H__
