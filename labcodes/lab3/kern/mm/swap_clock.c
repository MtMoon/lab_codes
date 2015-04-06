#include <defs.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_fifo.h>
#include <list.h>



list_entry_t pra_list_head; //这是链表头，按调入内存的时间顺序来加入链表
list_entry_t* listptr; //clock链表环的指针

static int
_clock_init_mm(struct mm_struct *mm)
{
     list_init(&pra_list_head);
     mm->sm_priv = &pra_list_head; //原来sm_priv是这样用的！
     listptr = &pra_list_head;
     return 0;
}

static int
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);

    assert(entry != NULL && head != NULL);

    list_add(head,entry);  //链表的构建和FIFO一样，用原来的方法就行，只是查找的时候需要注意
    //因为换出的时候，把表项低八位都清零了，所以换入的时候，低八位为0，但是do_pgfault会调用get_pte，get_pte中会把
    //| PTE_U | PTE_W | PTE_P这些位都置为1 在mmu.h中定义了各个位，其中的Access和Dirty位可以用
    return 0;
}


static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
     assert(head != NULL);
     assert(in_tick==0);
     //cprintf("enter swapout\n");

     list_entry_t* le = listptr;
     pte_t* pte = NULL;
     struct Page* page = NULL;
     //unsigned count = 0;



     while (1) {
    	 if (le == head) {
    		 le = list_next(le);
    		 continue;
    	 }
    	 assert(le != head);
    	 page  = le2page(le,pra_page_link);
    	 pte = get_pte(mm->pgdir,page->pra_vaddr,1); //获取pte
    	//cprintf("search, list page addr%x,\n", page->pra_vaddr);
    	//cprintf("this page Access:%d, Dirty:%d\n",(*pte)&PTE_A,(*pte)&PTE_D);

    	 pte_t temppte = *pte;
    	 //cprintf("ok1\n");
    	 if (  !((temppte)&PTE_A)  &&  !((temppte)&PTE_D )  ) { //如果访问位和修改位都为0,则替换出去
    		 break;
    	 } else if  ((( temppte)&PTE_A)  &&  (( temppte)&PTE_D ) ) {
    		 //将使用位置为0
    		 *pte &= (0xffffffff & (~PTE_A));
    	 } else if ( !(( temppte)&PTE_A)  &&  (( temppte)&PTE_D ) ){ //将修改位置0
    		 *pte &= (0xffffffff & (~PTE_D));
    	 } else if ( (( temppte)&PTE_A)  &&  !(( temppte)&PTE_D ) ) { //将使用位置为0
    		 *pte &= (0xffffffff & (~PTE_A));
    	 }
    	 le = list_next(le);
     }
     listptr = list_next(le);
     list_del(le);
     page = le2page(le,pra_page_link);
    //cprintf("swap out addr:%x\n",page->pra_vaddr);
     //注意，因为FIFO链表是用pra_page_link成员串起来的，根据le2page的原理，member应该选pra_page_link
     *ptr_page = page;
     return 0;
}

//设置页表项中的相应位，type为0是读，type为1是写
static void setFlagClock(uintptr_t la, int type) {
	pte_t* pte = get_pte(boot_pgdir,la,1);
	//设置访问位为1
	*pte |= (PTE_A);
	*pte &= (0xffffffff&(~PTE_D));
	if (type == 1) {
		*pte |= (PTE_D); //设置修改位为1
	}
}

static int
_clock_check_swap(void) {

	//进行swap out 时，是按照0x4000到0x1000的顺序遍历链表的
	//设Flag = 访问位修改位
	//初始时，0x4000 Flag = 10 0x3000 Flag = 11 0x2000 Flag = 10 0x1000 Flag = 11

	//写0x2000，Flag变为11，不缺页
	cprintf("write Virt Page c in fifo_check_swap\n");
   * (unsigned char *)0x2000 = 0x0c;
   setFlagClock(0x2000,1);
   assert(pgfault_num==4);

   //读0x5000，缺页，此时按照Clock算法，应该将0x4000处的页替换出去
   cprintf("read Virt Page c in fifo_check_swap\n");
    * (unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==5);
    setFlagClock(0x5000,0);
    //被替换出去的是0x4000，那么再读0x4000还会缺页
    cprintf("read Virt Page c in fifo_check_swap\n");
    * (unsigned char *)0x4000 = 0x04;
    assert(pgfault_num==6);
    setFlagClock(0x4000,0);
    //按照算法，读0x4000缺页，替换出去的是0x3000,再读0x2000不缺页
    cprintf("read Virt Page c in fifo_check_swap\n");
     * (unsigned char *)0x2000 = 0x02;
    assert(pgfault_num==6);
     setFlagClock(0x2000,0);

     //此时读0x3000会缺页，替换出去的应该是0x1000
     cprintf("read Virt Page c in fifo_check_swap\n");
    * (unsigned char *)0x3000 = 0x03;
     assert(pgfault_num==7);
     setFlagClock(0x3000,0);

    return 0;
}


static int
_clock_init(void)
{
    return 0;
}

static int
_clock_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_clock_tick_event(struct mm_struct *mm)
{ return 0; }


struct swap_manager swap_manager_clock =
{
     .name            = "clock swap manager",
     .init            = &_clock_init,
     .init_mm         = &_clock_init_mm,
     .tick_event      = &_clock_tick_event,
     .map_swappable   = &_clock_map_swappable,
     .set_unswappable = &_clock_set_unswappable,
     .swap_out_victim = &_clock_swap_out_victim,
     .check_swap      = &_clock_check_swap,
};
