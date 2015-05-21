#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>

//pre define
struct mm_struct;

// the virtual continuous memory area(vma), [vm_start, vm_end), 
// addr belong to a vma means  vma.vm_start<= addr <vma.vm_end 
struct vma_struct {
    struct mm_struct *vm_mm; // the set of vma using the same PDT 
    uintptr_t vm_start;      // start addr of vma      start和end这两个值应该是PGSIZE对齐的
    uintptr_t vm_end;        // end addr of vma, not include the vm_end itself
    uint32_t vm_flags;       // flags of vma
    list_entry_t list_link;  // linear list link which sorted by start addr of vma vma之间的地址空间无交集，链表表示的地址空间由小到大
};

#define le2vma(le, member)                  \
    to_struct((le), struct vma_struct, member)

//以下分别为只读/可读写/可执行，是上述vma_struct 结构中vm_flags的属性取值
#define VM_READ                 0x00000001
#define VM_WRITE                0x00000002
#define VM_EXEC                 0x00000004

// the control struct for a set of vma using the same PDT
struct mm_struct {
    list_entry_t mmap_list;        // linear list link which sorted by start addr of vma 链接了所有属于同一页目录表的虚拟内存空间，即链接了各个vam
    struct vma_struct *mmap_cache; // current accessed vma, used for speed purpose
    pde_t *pgdir;                  // the PDT of these vma pgdir 所指向的就是 mm_struct数据结构所维护的页表 通过访问pgdir可以查找某虚拟地址对应的页表项是否存在以及页表项的属性等
    int map_count;                 // the count of these vma
    void *sm_priv;                   // the private data for swap manager  链接了按先进先出顺序排好的page，用于FIFO算法
};

struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma);

struct mm_struct *mm_create(void);
void mm_destroy(struct mm_struct *mm);

void vmm_init(void);

int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

extern volatile unsigned int pgfault_num;
extern struct mm_struct *check_mm_struct;
#endif /* !__KERN_MM_VMM_H__ */

