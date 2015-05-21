LabX Clock置换算法
===
1 小组成员
---
2012011367 石伟男  
2012011364 矣晓沅  
2012011369 王轩  

2 设计思路
---

直接用现有的swamp_manager基本能支持extended clock算法的实现，但是还有许多需要修改的地方。新建swap_clock.h和swap_clock.c，在swap_clock.c进行实现，并定义变量 swap_manager swap_manager_clock绑定结构。在swap.c的swap_init函数中，通过下面两个语句的切换进行置换算法的选择：

     sm = &swap_manager_fifo;
     //sm = &swap_manager_clock;
     
思路就按照课上讲的改进的clock算法的相关知识。沿用原来的mm_struct结构，用其成员变量sm_priv链接其加入内存的各个页。由于原本sm_priv是一个头尾相接的双向链表，本身已经构成一个环，所以_clock_map_swappable函数可以不做改动。另外存了一个全局的list_entry_t指针listptr，用于保存每次查找时，在环形链表中移动的指针。另外，在mmu.h中定义了页表项的各个标志位：

    #define PTE_P           0x001                   // Present
    #define PTE_W           0x002                   // Writeable
    #define PTE_U           0x004                   // User
    #define PTE_PWT         0x008                   // Write-Through
    #define PTE_PCD         0x010                   // Cache-Disable
    #define PTE_A           0x020                   // Accessed
    #define PTE_D           0x040                   // Dirty
    #define PTE_PS          0x080                   // Page Size
    #define PTE_MBZ         0x180                   // Bits must be zero
    #define PTE_AVAIL       0xE00                   // Available for software use
    
 另外，实验指导书中指出，
 
    swap_entry_t
    -------------------------
    | offset | reserved | 0 |
    -------------------------
    24 bits 7 bits 1 bit
    
访问位PTE_A和修改位PTE_D都在低8位里，恰好可以用来保存页的访问和修改信息。另外，课上老师讲的是，实现clock算法时，一个页调入内存后，应该访问位置为1，修改位置为0。因此，对swap.c中的swap_in函数略作修改，当获取页表项后，把这两位做相应的修改。

2.clock_swap_out_victim实现
---

获取之前保存的全局指针listptr，从该指针处环形遍历mm结构体中以sm_priv为头的链表。依次检查内存中每个页的访问位与标志位组合，若为11，则修改为00；若为01或10则修改为00；或为00，则退出循环，置换该页。另外，按照课上讲的，选定置换页后，需要把listptr指针指向链表中被置换页的下一页。

*3.*测试

该算法实现起来比较简单，令人捉急的是如何进行测试。swap.c中的check_swap函数，主要是通过check_content_set的写操作，将4个4k的page调入内存，四个页的起始地址分别为0x1000，0x2000，0x3000，0x4000，然后在_fifo_check_swap通过相应的写操作进行测试。但是clock算法就比较捉急，因为需要记录对每个页的读写操作，而此时又没有用户态程序，页无从区别读和写。我的做法是，首先，在swap.c中新增了两个函数，setFlag和check_content_set_clock。测试clock算法时，修改check_swap函数，使其调用check_content_set_clock而非check_content_set进行设置。同样还是通过写0x1000，0x2000，0x3000，0x4000来将四个page调入内存，但在check_content_set_clock中写这四个page时，通过调用setFlag函数，强行改写其页表项的标志位，来模拟读写操作，使得四次操作为读，写，读，写。同样，在swap_clock.c中，页添加setFlag函数。然后，在swap_clock.c的_clock_check_swap函数中，进行多次写操作，但利用setFlag强行置位，模拟对不同页的读和写。测试用例如下：

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


打印程序运行中的相关信息，得到的输出如下，观察其跟踪的过程信息，和上述假设一直，能够验证算法实现的正确性：

    page fault at 0x00001000: K/W [no page found].
    page fault at 0x00002000: K/W [no page found].
    page fault at 0x00003000: K/W [no page found].
    page fault at 0x00004000: K/W [no page found].
    set up init env for check_swap over!
    write Virt Page c in fifo_check_swap
    read Virt Page c in fifo_check_swap
    page fault at 0x00005000: K/W [no page found].
    search, list page addr4000,
    this page Access:32, Dirty:0
    search, list page addr3000,
    this page Access:32, Dirty:64
    search, list page addr2000,
    this page Access:32, Dirty:64
    search, list page addr1000,
    this page Access:32, Dirty:64
    search, list page addr4000,
    this page Access:0, Dirty:0
    swap out addr:4000
    swap_out: i 0, store page in vaddr 0x4000 to disk swap entry 5
    read Virt Page c in fifo_check_swap
    page fault at 0x00004000: K/W [no page found].
    search, list page addr3000,
    this page Access:0, Dirty:64
    search, list page addr2000,
    this page Access:0, Dirty:64
    search, list page addr1000,
    this page Access:0, Dirty:64
    search, list page addr5000,
    this page Access:32, Dirty:0
    search, list page addr3000,
    this page Access:0, Dirty:0
    swap out addr:3000
    swap_out: i 0, store page in vaddr 0x3000 to disk swap entry 4
    swap_in: load disk swap entry 5 with swap_page in vadr 0x4000
    read Virt Page c in fifo_check_swap
    read Virt Page c in fifo_check_swap
    page fault at 0x00003000: K/W [no page found].
    search, list page addr2000,
    this page Access:32, Dirty:0
    search, list page addr1000,
    this page Access:0, Dirty:0
    swap out addr:1000
    swap_out: i 0, store page in vaddr 0x1000 to disk swap entry 2