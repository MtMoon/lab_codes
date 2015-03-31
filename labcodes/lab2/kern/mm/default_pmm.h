#ifndef __KERN_MM_DEFAULT_PMM_H__
#define  __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager default_pmm_manager;
extern const struct pmm_manager buddy_pmm_manager;
unsigned* buddy; //用于存放buddy信息的数组
#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */

