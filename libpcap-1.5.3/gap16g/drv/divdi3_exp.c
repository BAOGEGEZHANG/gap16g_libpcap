
#define EXPORT_SYMTAB
#include <linux/module.h>

/*
 * Don't even think about doing modversion here.
 * So, it doesn't matter what the types are.
 */
int __divdi3(void);
EXPORT_SYMBOL(__divdi3);

int __moddi3(void);
EXPORT_SYMBOL(__moddi3);

int __udivdi3(void);
EXPORT_SYMBOL(__udivdi3);

int __umoddi3(void);
EXPORT_SYMBOL(__umoddi3);

MODULE_DESCRIPTION("divdi3 Module");
MODULE_AUTHOR("NAC");
MODULE_LICENSE("GPL");
