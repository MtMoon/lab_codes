#include <swap.h>
#include <swapfs.h>
#include <swap_fifo.h>
#include <swap_PFF.h>
#include <stdio.h>
#include <string.h>
#include <memlayout.h>
#include <pmm.h>
#include <mmu.h>
#include <default_pmm.h>
#include <kdebug.h>

// the valid vaddr for check is between 0~CHECK_VALID_VADDR-1
#define CHECK_VALID_VIR_PAGE_NUM 7
#define BEING_CHECK_VALID_VADDR 0X1000
#define CHECK_VALID_VADDR (CHECK_VALID_VIR_PAGE_NUM+1)*0x1000
// the max number of valid physical page for check
#define CHECK_VALID_PHY_PAGE_NUM 5
// the max access seq number
#define MAX_SEQ_NO 10

static struct swap_manager *sm;
size_t max_swap_offset;

volatile int swap_init_ok = 0;

unsigned int swap_page[CHECK_VALID_VIR_PAGE_NUM];

unsigned int swap_in_seq_no[MAX_SEQ_NO],swap_out_seq_no[MAX_SEQ_NO];

static void check_swap(void);

int
swap_init(void)
{
     swapfs_init();

     if (!(1024 <= max_swap_offset && max_swap_offset < MAX_SWAP_OFFSET_LIMIT))
     {
          panic("bad max_swap_offset %08x.\n", max_swap_offset);
     }
     

     //sm = &swap_manager_fifo;
     sm = & swap_manager_PFF;
     //sm = &swap_manager_clock;
     int r = sm->init();
     
     if (r == 0)
     {
          swap_init_ok = 1;
          cprintf("SWAP: manager = %s\n", sm->name);
          check_swap();
     }

     return r;
}

int
swap_init_mm(struct mm_struct *mm)
{
     return sm->init_mm(mm);
}

int
swap_tick_event(struct mm_struct *mm)
{
	 //cprintf("enter swap tick event_________________________________________ \n");
     return sm->tick_event(mm);
}

int
swap_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
     return sm->map_swappable(mm, addr, page, swap_in);
}

int
swap_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
     return sm->set_unswappable(mm, addr);
}

volatile unsigned int swap_out_num=0;

int
swap_out(struct mm_struct *mm, int n, int in_tick)  //把in_tick作为强行置换页的标记
{
	//每次alloc page时会调用swap_out
     int i;
     int r = 0;
     for (i = 0; i != n; ++ i)
     {
          uintptr_t v;
          //struct Page **ptr_page=NULL;
          struct Page *page;
          // cprintf("i %d, SWAP: call swap_out_victim\n",i);
          int tempr = sm->swap_out_victim(mm, &page, in_tick);

          //cprintf("r:%d\n",r);

          if (tempr == -3) {
                  cprintf("page fault < T, no page swap out \n",i);
                  continue;
          }else if (tempr > 0) {
        	  //cprintf("pages swap out \n",i);
        	  r += tempr;
        	  if (r>=n) {
        		  break;
        	  } else {
        		  continue;
        	  }
          } else if (tempr != 0) {

        	  break;
          }
          //assert(!PageReserved(page));

          //cprintf("SWAP: choose victim page 0x%08x\n", page);
          //cprintf("lalalal2.9\n");
          assert(page != NULL);
          //cprintf("swap out la :0x%08x\n", page->pra_vaddr);
          v=page->pra_vaddr;  //这是这一页的线性地址
          //cprintf("lalalal2.10\n");
          pte_t *ptep = get_pte(mm->pgdir, v, 0);
          //macount--;

          assert((*ptep & PTE_P) != 0);

          if (swapfs_write( (page->pra_vaddr/PGSIZE+1)<<8, page) != 0) {
                    cprintf("SWAP: failed to save\n");
                    sm->map_swappable(mm, v, page, 0); //设为不可换出
                    continue;
          }
          else {
                    cprintf("FIFO swap_out: i %d, store page in vaddr 0x%x to disk swap entry %d \n", i, v, page->pra_vaddr/PGSIZE+1);
                    cprintf("\n");
                    *ptep = (page->pra_vaddr/PGSIZE+1)<<8; //线性地址按页对齐，即是页帧号，左移八位作为页表项
                    free_page(page);
          }
          
          tlb_invalidate(mm->pgdir, v);
     }
     return i;
}

int
swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result)
{
	//cprintf("nanananana\n");
     struct Page *result = alloc_page(); //缺页，申请一个page
     assert(result!=NULL);

     pte_t *ptep = get_pte(mm->pgdir, addr, 0);
     // cprintf("SWAP: load ptep %x swap entry %d to vaddr 0x%08x, page %x, No %d\n", ptep, (*ptep)>>8, addr, result, (result-pages));
    
     int r;
     if ((r = swapfs_read((*ptep), result)) != 0)
     {
        assert(r!=0);
     }
     cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n", (*ptep)>>8, addr);
     *ptr_result=result;
     return 0;
}



static inline void
check_content_set(void)
{

     *(unsigned char *)0x1000 = 0x0a;
     assert(pgfault_num==1);
     *(unsigned char *)0x1010 = 0x0a;
     assert(pgfault_num==1);
     *(unsigned char *)0x4000 = 0x0d;
     assert(pgfault_num==2);
     *(unsigned char *)0x4010 = 0x0d;
     assert(pgfault_num==2);
     *(unsigned char *)0x5000 = 0x0e;
     assert(pgfault_num==3);
     *(unsigned char *)0x5010 = 0x0e;
     assert(pgfault_num==3);


}


static inline int
check_content_access(void)
{
    int ret = sm->check_swap();
    return ret;
}

struct Page * check_rp[CHECK_VALID_PHY_PAGE_NUM];
pte_t * check_ptep[CHECK_VALID_PHY_PAGE_NUM];
unsigned int check_swap_addr[CHECK_VALID_VIR_PAGE_NUM];

extern free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
check_swap(void)
{
    //backup mem env
     int ret = 0, count = 0, total = 0, i;
     list_entry_t *le = &free_list;
     while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
     }
     assert(total == nr_free_pages());
     cprintf("BEGIN check_swap: count %d, total %d\n",count,total);
     
     //now we set the phy pages env     
     struct mm_struct *mm = mm_create();
     assert(mm != NULL);

     extern struct mm_struct *check_mm_struct;
     assert(check_mm_struct == NULL);

     check_mm_struct = mm;

     pde_t *pgdir = mm->pgdir = boot_pgdir;
     assert(pgdir[0] == 0);

     struct vma_struct *vma = vma_create(BEING_CHECK_VALID_VADDR, CHECK_VALID_VADDR, VM_WRITE | VM_READ);
     assert(vma != NULL);

     insert_vma_struct(mm, vma);

     //setup the temp Page Table vaddr 0~4MB
     cprintf("setup Page Table for vaddr 0X1000, so alloc a page\n");
     pte_t *temp_ptep=NULL;
     temp_ptep = get_pte(mm->pgdir, BEING_CHECK_VALID_VADDR, 1);
     assert(temp_ptep!= NULL);
     cprintf("setup Page Table vaddr 0~4MB OVER!\n");
     
     for (i=0;i<CHECK_VALID_PHY_PAGE_NUM;i++) {
          check_rp[i] = alloc_page();
          assert(check_rp[i] != NULL );
          assert(!PageProperty(check_rp[i]));
     }
     list_entry_t free_list_store = free_list;
     list_init(&free_list);
     assert(list_empty(&free_list));
     
     //assert(alloc_page() == NULL);
     
     unsigned int nr_free_store = nr_free;
     nr_free = 0;
     for (i=0;i<CHECK_VALID_PHY_PAGE_NUM;i++) {
        free_pages(check_rp[i],1);
     }
    // assert(nr_free==CHECK_VALID_PHY_PAGE_NUM);

     cprintf("set up init env for check_swap begin!\n");
     //setup initial vir_page<->phy_page environment for page relpacement algorithm

     
     pgfault_num=0;
     cprintf("nr_free 1:%d \n", nr_free);
    //nr_free = 20;
     //check_content_set(); //设置初始内存页数
     //get_pte(mm->pgdir, 0x2000, 1);
     //get_pte(mm->pgdir, 0x3000, 1);


     cprintf("nr_free2:%d \n", nr_free);

     cprintf("set up init env for check_swap over!\n");
     // now access the virt pages to test  page relpacement algorithm
     cprintf("nr_free:%d \n", nr_free);
     ret = check_content_access();




     assert(ret==0);
     
     //restore kernel mem env
     /*for (i=0;i<CHECK_VALID_PHY_PAGE_NUM;i++) {
         free_pages(check_rp[i],1);
     }*/

     //free_page(pte2page(*temp_ptep));
    free_page(pa2page(pgdir[0]));
     pgdir[0] = 0;
     mm->pgdir = NULL;
     mm_destroy(mm);
     check_mm_struct = NULL;

     nr_free = nr_free_store;
     free_list = free_list_store;


     le = &free_list;
     while ((le = list_next(le)) != &free_list) {
         struct Page *p = le2page(le, page_link);
         count --, total -= p->property;
     }
     cprintf("count is %d, total is %d\n",count,total);
     //assert(count == 0);

     cprintf("check_swap() succeeded!\n");
}
