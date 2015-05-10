#ifndef __EH_DEBUG_H__
#define __EH_DEBUG_H__


#ifdef __KERNEL__
    // debug macros for kernel space 
    #define PRINT_FLOW(fmt, args...) \
        do { \
            printk(KERN_INFO"[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args); \    
        }

    #define PRINT_ERR(fmt, args...) \
        do { \
            printk(KERN_ERR"[%s:%d][Error]" fmt, __FUNCTION__, __LINE__, ##args); \
        }

#else
    // debug macros for user space 
    #define PRINT_FLOW(fmt, args...) \
        do { \
            fprintf(stderr, "[%s:%d]" fmt, __FUNCTION__, __LINE__, ##args); \    
        }

    #define PRINT_ERR(fmt, args...) \
        do { \
            fprintf(stderr, "[%s:%d][Error]" fmt, __FUNCTION__, __LINE__, ##args); \
        }

#endif //__KERNEL

#endif // __EH_DEBUG_H__
