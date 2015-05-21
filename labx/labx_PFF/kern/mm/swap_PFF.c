#include <defs.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_PFF.h>
#include <list.h>



list_entry_t pra_list_head; //这是链表头，按调入内存的时间顺序来加入链表

static int
_PFF_init_mm(struct mm_struct *mm)
{
     list_init(&pra_list_head);
     mm->sm_priv = &pra_list_head; //原来sm_priv是这样用的！
     macount = 0;
     mm->lastPF = 0;
     mm->timeCount = 0;
     mm->T = 3;
     return 0;
}

static int
_PFF_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
	//cprintf("macount: %d \n", macount);
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
    pte_t* pte = NULL;

    assert(entry != NULL && head != NULL);

    list_add(head,entry);  //链表的构建和FIFO一样，用原来的方法就行



    return 0;
}


/**
 * 此时要置换出去若干个页，在vmm.c中，先调用swapin，swapin 申请一个页，并把磁盘上相应内容填到这个页里
 * 之后 alloc page 置换出去一个页
 * 然后vmm.c中调用PFF_map_swappable，将刚申请的页加入
 * 这里我们进行判断，如果不需要换出，则返回-3 那么就是只加不换出
 * 返回非零值，说明是在此函数里完成了换出操作，且返回值为换出的页数
 * in_tick为0时，按fifo置换出一页,返回0
 */
static int
_PFF_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
	 cprintf("macount: %d， in_tick:%d  \n", macount, in_tick);
	 //cprintf("mm pgdir %0x \n", mm->pgdir);
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
     assert(head != NULL);



     //获取上次缺页时间和此次缺页时间
     uint32_t lastPFtime = mm->lastPF;
     uint32_t nowPFtime = macount;
     //uint32_t nowPFtime = mm->timeCount;
     mm->lastPF = nowPFtime;





     //cprintf("enter swapout\n");

     pte_t* pte = NULL;
     struct Page* page = NULL;

     list_entry_t* le = NULL;

     if (in_tick == 0) {

         /*le = list_next(head);
          cprintf("all list  fifo0____________________________________- \n");
          while ( le != head) {
         	 page  = le2page(le,pra_page_link);
         	 pte = get_pte(mm->pgdir,page->pra_vaddr,1);
         	 cprintf("page: %0x,  access: %d  present: %d  \n", page->pra_vaddr, (*pte)&PTE_A,  (*pte)&PTE_P );
         	 le = list_next(le);

          }
          cprintf("all list  fifo0 end____________________________________- \n");*/


    	 le = list_prev(head);
    	 assert(le != head);
         page = le2page(le,pra_page_link);
         list_del(le);
        *ptr_page = page;

        le = list_next(head);
         /*cprintf("all list  fifo1____________________________________- \n");
         while ( le != head) {
        	 page  = le2page(le,pra_page_link);
        	 pte = get_pte(mm->pgdir,page->pra_vaddr,1);
        	 cprintf("page: %0x,  access: %d  present: %d  \n", page->pra_vaddr, (*pte)&PTE_A,  (*pte)&PTE_P );
        	 le = list_next(le);

         }
         cprintf("all list  fifo1 end____________________________________- \n");*/


         le = list_next(head);
         while (le != head) {

        	 page  = le2page(le,pra_page_link);
        	 pte = get_pte(mm->pgdir,page->pra_vaddr,1); //获取pte
        	 pte_t temppte = *pte;
        	 //将使用位置为0
        	if (  (temppte & PTE_P) != 0 ) {

        		 *pte &=    (0xffffffff & (~PTE_A));
        	}
        	 le = list_next(le);
         }


        /* le = list_next(head);
          cprintf("all list  fifo2____________________________________- \n");
          while ( le != head) {
         	 page  = le2page(le,pra_page_link);
         	 pte = get_pte(mm->pgdir,page->pra_vaddr,1);
         	 cprintf("page: %0x,  access: %d  present: %d \n", page->pra_vaddr, (*pte)&PTE_A,  (*pte)&PTE_P);
         	 le = list_next(le);

          }
          cprintf("all list  fifo2 end____________________________________- \n");*/

         return 0;

     }






     if (  (nowPFtime - lastPFtime <= mm->T) ) { //缺页率过高 不减少常驻集
    	 *ptr_page = NULL;
    	 return -3;
     }

     cprintf("lastPFtime:%d\n", lastPFtime);
     cprintf("nowPFtime:%d\n", nowPFtime);

     /*le = list_next(head);
      cprintf("all list  after fifo____________________________________- \n");
      while ( le != head) {
     	 page  = le2page(le,pra_page_link);
     	 pte = get_pte(mm->pgdir,page->pra_vaddr,1);
     	 cprintf("page: %0x,  access: %d  present: %d  \n", page->pra_vaddr, (*pte)&PTE_A,  (*pte)&PTE_P );
     	 le = list_next(le);

      }
      cprintf("all list  after fifo end____________________________________- \n");

    // cprintf("read to swap out \n");*/

     //否则，置换所有在该段时间内没有引用的页，即该段时间内所用引用位为0的页
     le = list_next(head);
     list_entry_t *nextle = NULL;
     uint32_t tempcount = 0;
     uintptr_t v = NULL;

     int swapcount = 0;





     while (le != head) { //逐一检查
    	 page  = le2page(le,pra_page_link);
    	 pte = get_pte(mm->pgdir,page->pra_vaddr,1);
    	 //cprintf("check pff 0x%0x \n", page->pra_vaddr);
    	 tempcount++;
    	 pte_t temppte = *pte;

    	 nextle = list_next(le);

    	 //cprintf("hahaha\n");

    	 if (   ((temppte)&PTE_A) ==0   &&  (temppte & PTE_P) != 0  ) { //如果访问位为0,则替换出去

    		 //cprintf("hahaha\n");

    		 list_del(le);
    		 assert(page != NULL);
    	     v = page->pra_vaddr;  //这是这一页的线性地址

    	     if (swapfs_write( (page->pra_vaddr/PGSIZE+1)<<8, page) != 0) {
    	    	 cprintf("SWAP: failed to save\n");
    	    	 _PFF_map_swappable(mm, v, page, 0); //设为不可换出
    	    	 le = nextle;
    	    	 continue;
    	     }
    	     else {
    	    	 cprintf("PFF swap_out, store page in vaddr 0x%x to disk swap entry %d \n", v, page->pra_vaddr/PGSIZE+1 );
    	    	 *pte = (page->pra_vaddr/PGSIZE+1)<<8; //线性地址按页对齐，即是页帧号，左移八位作为页表项
    	    	 free_page(page);
    	    	 swapcount++;
    	     }
    	     tlb_invalidate(mm->pgdir, v);

    	 }
         le = nextle;
     }

     cprintf("\n");

     //cprintf("read to swap out 1\n");

     //将所有在内存中的页的引用位置为0
     //此处因为要获取pte，会影响macount的值，之后要减去
     le = list_next(head);
     while (le != head) {

    	 page  = le2page(le,pra_page_link);
    	 pte = get_pte(mm->pgdir,page->pra_vaddr,1); //获取pte
    	 tempcount++;
    	 pte_t temppte = *pte;
    	 //将使用位置为0
    	if (  (temppte & PTE_P) != 0 ) {

    		 *pte &=    (0xffffffff & (~PTE_A));
    	}
    	 le = list_next(le);
     }

     /*le = list_next(head);
      cprintf("all list  after reset____________________________________- \n");
      while ( le != head) {
     	 page  = le2page(le,pra_page_link);
     	 pte = get_pte(mm->pgdir,page->pra_vaddr,1);
     	 cprintf("page: %0x,  access: %d  present: %d  the value is %0x\n", page->pra_vaddr, (*pte)&PTE_A,  (*pte)&PTE_P,  ((*pte)&PTE_P) == 1?* (unsigned char *)page->pra_vaddr:-1 );
     	 le = list_next(le);

      }
      cprintf("all list  after reset end____________________________________- \n");*/

     assert(swapcount != 0);


     //cprintf("read to swap out 2\n");
     return swapcount;
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
_PFF_check_swap(void) {

	//* (unsigned char *)0x2000 = 0x0b;

	//* (unsigned char *)0x3000 = 0x0c;

	//初始化部分

	* (unsigned char *)0x1000 = 0x0a;

	* (unsigned char *)0x4000 = 0x0d;

	* (unsigned char *)0x5000 = 0x0e;

	//现在内存中有5个页

	 *(unsigned char *)0x6000 = 0x0f;

	 *(unsigned char *)0x7000 = 0x10;

	 macount = 0;

	 //实际测试部分
	//测试用例的访问序列为[c,c,d,b,b,e,c,e,a,d] 其中b，c的地址为0x2000,0x3000

	 int temp = 0;

	 cprintf("Access 0x%0x \n", 0x3000);
	temp = * (unsigned char *)0x3000;
	setFlagClock(0x3000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x3000);
	temp = * (unsigned char *)0x3000;
	setFlagClock(0x3000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x4000);
	temp = * (unsigned char *)0x4000;
	setFlagClock(0x4000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x2000);
	temp = * (unsigned char *)0x2000;
	setFlagClock(0x2000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x2000);
	temp = * (unsigned char *)0x2000;
	setFlagClock(0x2000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x5000);
	temp = * (unsigned char *)0x5000;
	setFlagClock(0x5000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x3000);
	temp = * (unsigned char *)0x3000;
	setFlagClock(0x3000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x5000);
	temp = * (unsigned char *)0x5000;
	setFlagClock(0x5000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x1000);
	temp = * (unsigned char *)0x1000;
	setFlagClock(0x1000,0);
	macount++;

	 cprintf("Access 0x%0x \n", 0x4000);
	temp = * (unsigned char *)0x4000;
	setFlagClock(0x4000,0);
	macount++;


    return 0;
}


static int
_PFF_init(void)
{
    return 0;
}

static int
_PFF_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int _PFF_tick_event(struct mm_struct *mm) {

	//cprintf("timer\n");
	mm->timeCount++;
	if (mm->timeCount < mm->lastPF) {
		uint32_t temp = 4294967295 - mm->lastPF;
		mm->timeCount += temp;
		mm->lastPF = 0;
	}
	return 0;

}


struct swap_manager swap_manager_PFF =
{
     .name            = "PFF swap manager",
     .init            = &_PFF_init,
     .init_mm         = &_PFF_init_mm,
     .tick_event      = &_PFF_tick_event,
     .map_swappable   = &_PFF_map_swappable,
     .set_unswappable = &_PFF_set_unswappable,
     .swap_out_victim = &_PFF_swap_out_victim,
     .check_swap      = &_PFF_check_swap,
};
