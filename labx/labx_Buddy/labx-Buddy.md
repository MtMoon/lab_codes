LabX Buddy算法
===
1 小组成员
---
2012011367 石伟男  
2012011364 矣晓沅  
2012011369 王轩  

2. 设计思路
---

(1) buddy分配的管理结构及存储

在实现上，借鉴了实验指导书中提供的关于buddy system一个极简实现的博文。博文中采用了一个结构体来存储相应的信息，结构体中包含一个变量记录总空间大小，以及一个数组来记录二叉树的信息。我直接在default_pmm.c中定义了一个unsigned数组指针buddy，然后用该数组存储二叉树的相应信息。总空间大小在buddy_pmm.c中用一个全局变量进行存储。参照那篇博文，用该unsigned数组存储一个二叉树。二叉树的每个节点对应不断二分的空间信息，即每个数组元素存储的是该节点对应的内存块的大小。buddy system要求总空间以及每个块的大小都是2的整数次幂，根据前面的实验结果，能看到实际可利用的空间大小为32324页，所以在实现buddy system时，只取了其中的16384页(2^14)。而根据二叉树节点数目的关系，用于存储二叉树的数组开为总空间的两倍。在原来的lab2代码中，我们可以看到有如下代码：

    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE); 
     for (i = 0; i < npage; i ++) {
        SetPageReserved(pages + i);  //将用来存内存管理结构pages的空间设为已用
    }
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * npage);
    
即从Ucore结束后的地址开始，先存储用来管理的Page结构体，再从freemem开始是实际使用的空间。加入buddy system后，需要空间存储二叉树。于是从原来的pages结束处开始，存储buddy数组，再从buddy数组结束的地方开始作为实际可用的内存空间，如下：

    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE); 
     for (i = 0; i < npage; i ++) {
        SetPageReserved(pages + i);  //将用来存内存管理结构pages的空间设为已用
    }
    buddy = (unsigned *)ROUNDUP((void *)((uintptr_t)pages + sizeof(struct Page) * npage), PGSIZE);
    uintptr_t freemem= PADDR((uintptr_t)buddy + sizeof(unsigned) * npage*2);
    
(2) 初始化buddy_init_memmap

初始化时，和实现first-fit时类似，都需要对相关的page进行flags清零，标志位的设置等等。区别在于，在buddy system的实现中，不再需要原来的空闲块链表，此外，需要对buddy数组进行初始化：

    for (i = 1; i < len; i++) {
        int i1 = i+1;
        if (IS_POWER_OF_2(i1)) {
        	   size /= 2;
        }
        buddy[i] = size;
    }
    
即将二叉树按照从上到下，从左到右的顺序，把节点所对应的块大小依次填入buddy数组中。此时还需要注意，之后的alloc函数分配空间后，返回的是块的首个Page的指针，为了保持接口不变，分配与释放的函数参数及返回值都不应该修改。原来分配时，直接通过空闲块链表来获取相应的Page并返回。buddy system中，为了能在alloc获取对应的Page，在buddy_pmm.c中定义了一个全局Page结构体指针pagebase，并在buddy_init_memmap函数中，将空闲空间的起始地址base存在pagebase里，之后分配时，只需要算出pagebase的rank即可获取。

(3) 分配buddy_alloc_pages

buddy system要求分配的空间必须为2的整数次幂，首先要检查要求分配的大小是否是2的整数次幂，不是的话，要向上舍入到最近的2的整数幂大小，如要求分配3，则应分配4的大小。在buddy_pmm.c中，定义了取左子节点index，右子节点index，父节点index，判断是否是2的幂次，向上舍入到2的幂次等功能的函数和宏。确定了大小之后，则从二叉树的根节点进行搜索，找到满足要求的块并分配，搜索代码如下：

    for(node_size =  TOTAL_PAGE_NUM; node_size != n; node_size /= 2 ) {
    	   if (buddy[LEFT_LEAF(index)]  >= n) {
    	       index = LEFT_LEAF(index);
    	   }	else {
    	      index = RIGHT_LEAF(index);
    	   }
    }
    
这实际上是一个深度优先搜索，如此则可确保分配时，是按照地址从低到高，找到第一个满足要求的块，获取其index（在buddy数组中的位置），计算出是第几个page，然后返回pagebase中的相应元素即可。找到相应的块后，还需要对父节点的大小进行更新，更新代码如下：

    while (index) {
    	   index = PARENT(index);
       buddy[index] = MAX(buddy[LEFT_LEAF(index)], buddy[RIGHT_LEAF(index)]);
    }

另外需要说明的是，在之后的free函数中，由于接口保持不变，传入参数的仅仅是Page结构体和相应的大小，在buddy system的free中，需要确定该块在二叉树中节点的位置index，即buddy数组中的位置。在alloc时，通过index可以计算出块的首page在pagebase数组中的rank，反之，只要有该rank，也可计算出index。因此对Page结构体略作修改，在其中增加了一个unsigned类型的变量offset，用于保存该块首Page在pagebase数组中的位置。

(4) 释放buddy_free_pages

释放时，传入Page结构体和相应大小，由于buddy system的实现，这里要求块大小和传入的参数大小应该一致，即不能传入一个4大小的块却只释放3个page。根据传入page中的offset变量值，计算出该节点在buddy数组中的index，将该数组元素的值再次回复，然后从该节点向上再次逐一更新父节点。这里的实现与那篇博文中的略有区别，那篇博文是从叶子节点向上更新，然后遇到分配出去的节点就重置，退出循环。感觉这样的实现是有bug的，就没按照那个，而是直接定位到需要释放的节点。

3 测试用例
---

重新写了测试函数，主要测试了buddy system的几个特性是否满足：若要求分配的块大小不是2的幂，则应向上舍入到临近的2的幂次；释放后，若相邻的两个空闲块大小和为2的幂次，则合并，大小和不为2的幂次则不和并；分配时，分配的块应该从低地址到高地址，按满足要求的大小分配等。具体测试用例如下：

        //初始空间大小应为2的幂
        	assert(IS_POWER_OF_2(nr_free) );
        	//cprintf("initial page num: %d \n", nr_free);
        	struct Page* p0 = NULL;
        	struct Page* p1 = NULL;
        	struct Page* p2 = NULL;
        	//只能分配2的整数幂，分配3，得到的是4大小
        	p0 = alloc_pages(3);
        	assert(p0->property == 4);
        	//分配的块，应该是按地址顺序查找不小于所需的2的整数幂的块
        	p1 = alloc_pages(8);
        	assert(p1->property == 8);
        	assert(p0->offset + 8 == p1->offset);
        	assert(p0+8 == p1);
        
        	p2 = alloc_pages(4);
        	assert(p0+4 == p2);
        	assert(p2+4 == p1);
        
        	//释放后，会合并相邻块
        	free_pages(p0,4);
        	free_pages(p2,4);
        	p0 = alloc_pages(8);
        	assert(p0->property == 8);
        	assert(p0+8 == p1);
        
        	//全部释放完毕
        	free_pages(p1,8);
        	free_pages(p0,8);
        	assert(nr_free == TOTAL_PAGE_NUM);
        
        	p0 = alloc_pages(2);
        	assert(p0->property == 2);
        	assert(p0->offset == 0);
        
        	p1 = alloc_pages(2);
        	assert(p1->property == 2);
        	assert(p0+2 == p1);
        
        	p2 = alloc_pages(4);
        	assert(p2->property == 4);
        	assert(p1+2 == p2);
        
        	free_pages(p1,2);
        	free_pages(p2,4);
        
        	p1 = alloc_pages(4);
        	//虽然释放了相邻两块，但是相邻两块大小加起来不为2的幂时就不会合并
        	assert(p1->property == 4);
        	assert(p0+2 != p1);
        	assert(p0+4 == p1); //这个可以看出没有合并，分配的还是之前那个4大小的块
        	
上述测试均能通过