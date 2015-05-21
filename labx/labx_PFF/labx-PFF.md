LabX 缺页率置换算法
===
1 小组成员
---
2012011367 石伟男  
2012011364 矣晓沅  
2012011369 王轩  

2 总体设计思路
---

采用缺页率置换算法，思路和课上讲的一样，调节常驻集大小。缺页时，若缺页率较低，则置换出一部分页；若缺页率较高，则增加缺失的页到工作集中。看到labX的统计，有同学在做工作集置换算法，他们提出的问题是时间无法精确统计。我的做法是利用tick_event函数，每个时钟中断时调用此函数，在mm结构体里维护一个时间计数变量timecount，每次调用tick_event函数就把该变量加一。这样每次缺页就能直到缺页时的时间，然后再在mm结构体中增加一个记录上次缺页时间的变量lastPF，如此则能计算出两次缺页的时间间隔。理论上可行，但实际操作起来发现了很多问题，具体在下面进行进一步的分析。

3 关键数据结构与函数
---

*1.* 数据结构

主要修改了vmm.h中的mm_struct，如下：

    struct mm_struct {
        list_entry_t mmap_list;        // linear list link which sorted by start addr of vma 链接了所有属于同一页目录表的虚拟内存空间，即链接了各个vam
        struct vma_struct *mmap_cache; // current accessed vma, used for speed purpose
        pde_t *pgdir;                  // the PDT of these vma pgdir 所指向的就是 mm_struct数据结构所维护的页表 通过访问pgdir可以查找某虚拟地址对应的页表项是否存在以及页表项的属性等
        int map_count;                 // the count of these vma
        void *sm_priv;                 // the private data for swap manager
        int mm_count;                  // the number ofprocess which shared the mm
        semaphore_t mm_sem;            // mutex for using dup_mmap fun to duplicat the mm 
        int locked_by;                 // the lock owner process's pid
        uint32_t  timeCount;
        uint32_t lastPF; //上次缺页时的时间
        uint32_t T;  //缺页率阈值，即窗口大小
    };
    
其中增加的是记录时间的timeCount，记录上次缺页时间的timeCount以及窗口大小T，缺页时，在窗口T内未访问过的页都置换出去。

*2.* 相关函数

    static int _PFF_tick_event(struct mm_struct *mm) {
    
        	mm->timeCount++;
        	if (mm->timeCount < mm->lastPF) {
        		uint32_t temp = 4294967295 - mm->lastPF;
        		mm->timeCount += temp;
        		mm->lastPF = 0;
        	}
        	return 0;
    
    }
    
这是相应时间的函数。直接不断累加的话会存在timeCount的值溢出的问题，由于采用的是32位无符号整数，溢出后值减小。如果不溢出，那么timeCount++之后直一定大于上次缺页的时间，所以这里通过比较这两个值进行是否溢出的判断。溢出后，将lastPF移到0处，然后增加timeCount保证两种间隔不变。

    static int _PFF_init_mm(struct mm_struct *mm)
    static int _PFF_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
    
这两个函数和FIFO没有太大区别，swappable时也按FIFO进行插入即可，init时需要对新增的变量设初始值。

    static int _PFF_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
    
这是算法实现的主要函数。在说明该函数的思路之前，需要考虑两个问题。一是原本FIFO的框架中，是在alloc_page函数中，每次申请一个新的页，若申请失败，则调用swap_out函数将一些页换出。具体的函数调用链是do_pgfault->swap_in->alloc_page->swap_out,即缺页，调用一个页，内存不够，替换出去一个页。这是局部置换算法，对PFF则需要每次缺页时进行判断。为此，在vmm.c的do_pgfault函数中，先执行swap_out函数，再执行swap_in。第二个问题是内存容量，如果内存满了 ，但此时不满足缺页率算法置换的条件，即缺页率不是太小，那么此时也需要将一个页置换出来。

基于以上两个考虑，利用_swap_out_victim的in_tick参数(这个参数之前实际没用)，该参数为0，表示内存已满，需要置换一页出来；为1，表示内存未满，按照缺页率算法进行置换。_PFF_swap_out_victim的实现中，当in_tick为0，则采用FIFO算法选出一个置换页然后返回；当当in_tick为1，继续进行PFF流程。在pmm.c的alloc_pages函数中，若申请页失败，则以in_tick=0 调用swap_out;在vmm.c的do_pgfault函数中，以in_tick=1 调用swap_out；

接下来是PFF算法的执行流程，若mm->timeCount - mm->lastPF <= T,不需要置换，直接返回； 否则，变量内存链表，将其中所有访问位为0并且存在位为1的页从链表中删除并调用swapfs_write函数换出。因为PFF算法每次要换出的是在T内没有访问过的页，所以在调用调用swapfs_write函数换出后，再将所有剩下的页的访问位置为0.

另外还需要考虑_PFF_swap_out_victim返回值的问题，为了尽量兼容原来的框架，即调用其他的算法UCore页也应该能正确运行。对FIFO，_PFF_swap_out_victim执行成功时返回0. 对_PFF_swap_out_victim，若是内存满执行FIFO时，返回0，要置换的页存在ptr_page中，具体置换交由swap.c中swap_out后半部分代码(即FIFO时原本的换出代码)进行;若不需要换出，返回-3，在swap_out函数中直接跳出循环结束函数；若利用PFF逻辑换出，则在_PFF_swap_out_victim执行换出并返回换出的页数。

*4.* 执行与测试说明

按照上述方法，编程并在lab8的框架下执行，sh，ls，hello，priority等函数都成功运行无误，在一定程度上可说明算法实现的正确性。但是最好还是向check FIFO一样能够有手动设置的可验证的测例。但是这就比较麻烦，详见下面的算法测试与结果。

4 算法测试与结果
---

上述算法的实现依赖于时钟中断，但手动设置的check 函数_PFF_check_swap以及swap.c中的check_swap是在swap_init()中就调用了，在init.c中可以看到，中断是在最后才开启的。如果将clock_init和intr_enable移到swap_init之前，感觉会不安全。好吧，实际我确实移了，然后发现两个时钟中断之间间隔太长，足够做很多次访存，这样要手动加测例就会很捉急，也不容易控制。这里采取一个简易模拟的方法来进行模拟测试：建一个全局变量macount，测例中每次访存时将macount加一，然后用两次缺页间的访存次数来模拟时间间隔(实际上这也是缺页率原本的定义。)

测试用例如下，窗口T值设为2

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
        	
测试用例说明：	setFlagClock(0x1000,0)是一个将对应地址页表项的访问位强行置为0的函数。本来执行读操作是，机器应该会将访问位置为，之前做clock算法的同学也是这么说。但我实际打印查看，发现有的地址读了值了访问位并没有被置位，不知道是不是qemu的问题，所以手动置位来模拟。按照上述访问序列第一次访问c时，缺页，内存满，应该调用FIFO将a换出；之后访问c，d无操作；访问b时，同样应将4用fifo换出。然后一直访问到a，缺页，in_tick为1，走PFF线，此时nowPFtime - lastPFtime = 5>2, f和9在这段时间段里没有访问过，应该换出。最后访问d，缺页，但是间隔小，只把d加入而不换出。程序输出如下，与上述说明一致：

    nr_free:5 
    page fault at 0x00001000: K/W [no page found].
    do page fault at 0x00001000
    page fault at 0x00004000: K/W [no page found].
    do page fault at 0x00004000
    page fault at 0x00005000: K/W [no page found].
    do page fault at 0x00005000
    page fault at 0x00006000: K/W [no page found].
    do page fault at 0x00006000
    page fault at 0x00007000: K/W [no page found].
    do page fault at 0x00007000
    Access 0x3000 
    page fault at 0x00003000: K/R [no page found].
    do page fault at 0x00003000
    macount: 0， in_tick:0  
    FIFO swap_out: i 0, store page in vaddr 0x1000 to disk swap entry 2 
    
    Access 0x3000 
    Access 0x4000 
    Access 0x2000 
    page fault at 0x00002000: K/R [no page found].
    do page fault at 0x00002000
    macount: 3， in_tick:0  
    FIFO swap_out: i 0, store page in vaddr 0x4000 to disk swap entry 5 
    
    Access 0x2000 
    Access 0x5000 
    Access 0x3000 
    Access 0x5000 
    Access 0x1000 
    page fault at 0x00001000: K/R [no page found].
    do page fault at 0x00001000
    macount: 8， in_tick:1  
    lastPFtime:3
    nowPFtime:8
    PFF swap_out, store page in vaddr 0x7000 to disk swap entry 8 
    PFF swap_out, store page in vaddr 0x6000 to disk swap entry 7 
    
    swap_in: load disk swap entry 2 with swap_page in vadr 0x1000
    Access 0x4000 
    page fault at 0x00004000: K/R [no page found].
    do page fault at 0x00004000
    macount: 9， in_tick:1  
    page fault < T, no page swap out 
    swap_in: load disk swap entry 5 with swap_page in vadr 0x4000
    count is 0, total is 6
    check_swap() succeeded!


