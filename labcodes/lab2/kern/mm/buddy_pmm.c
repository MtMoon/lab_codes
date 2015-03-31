#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)


//判断是否是2的幂的宏
#define  IS_POWER_OF_2(n) ((  ((n & ((~n) + 1))) ==n ) || (n==1) )
#define LEFT_LEAF(index) ((index<<1)+1)
#define RIGHT_LEAF(index) ((index+1)<<1)
#define PARENT(index) ((index-1)>>1)

//记录可用页数的初始值
unsigned TOTAL_PAGE_NUM = 0;

//实际空闲的物理区域数组
struct Page* pagebase = NULL;



//power向下取幂
static size_t getPower(size_t n) {
	unsigned mask = 0xfffffffe;
	while (!IS_POWER_OF_2(n)) {
		n = n & mask;
		mask = mask << 1;
	}
	return n;

}

//向上取幂
static size_t getPower2(size_t n) {
	unsigned mask = 0xfffffffe;
	n = n << 1;
	while (!IS_POWER_OF_2(n)) {
		n = n & mask;
		mask = mask << 1;
	}
	return n;

}

static unsigned MAX(unsigned a, unsigned b) {
	if (a>b) {
		return a;
	} else {
		return b;
	}
}

static void
buddy_init(void) { //这里直接用吧，貌似木有什么需要改的
    list_init(&free_list);
    nr_free = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    //cprintf("pages addr1: %08llx \n", pages);
    //cprintf("  init memmap debug size begin: %d \n", n);
    if (!IS_POWER_OF_2(n)) {
    	n = getPower(n);
    }
   //cprintf("  init memmap debug size after: %d \n", n);

    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        ClearPageProperty(p); //中间页的property位置为0
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base); //首页的property位置为0
    nr_free += n;
    TOTAL_PAGE_NUM += n;

    //初始化buddy数组
    unsigned size = n;
    int i = 0;
    int len = 2*size-1;
   // size = size << 1;
    buddy[0] = size;
   // size = size << 1;
    //cprintf("1 is power of 2:%d\n",IS_POWER_OF_2(1) );
    for (i = 1; i < len; i++) {
    	int i1 = i+1;
        if (IS_POWER_OF_2(i1)) {
        	//cprintf("lala\n");
        	size /= 2;
        }
        buddy[i] = size;
        /*if (i<100) {
        	cprintf("buddy[%d]: %d\n",i,buddy[i]);
        }*/

     }
    pagebase = base;
    //cprintf("buddy addr1: %08llx \n", buddy);



}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
    	cprintf("alloc NULL point 0\n");
        return NULL;
    }
    //cprintf("alloc  buddy[0]:%d\n", buddy[0]);
    //确保分配的块大小是2的幂次
    if (n <= 0) {
    	n = 1;
    } else if (!IS_POWER_OF_2(n)) {
    	n = getPower2(n);
    }
    //cprintf("alloc size  after:%d\n", n);

    //超过最大大小
    if (buddy[0] < n) {
    	cprintf("alloc NULL point 1\n");
    	cprintf("needed n:%d \n",n);
    	cprintf("nr_free n:%d \n",nr_free);
    	cprintf("buddy[0] :%d \n", buddy[0]);
    	return NULL;
    }

    unsigned index = 0;
    unsigned node_size = 0;

    //深度优点搜索
    for(node_size =  TOTAL_PAGE_NUM; node_size != n; node_size /= 2 ) {
    	if (buddy[LEFT_LEAF(index)]  >= n) {
    		//cprintf("");
    		index = LEFT_LEAF(index);
    	}	else {
        	index = RIGHT_LEAF(index);
    	}
    }

    //cprintf("index:%d\n",index);
    buddy[index] = 0;
    unsigned offset = (index + 1) * node_size - TOTAL_PAGE_NUM;
    assert(node_size == n);

    //向上回溯更新父节点大小
    while (index) {
    	index = PARENT(index);
        buddy[index] = MAX(buddy[LEFT_LEAF(index)], buddy[RIGHT_LEAF(index)]);
      }

    struct Page* page = &pagebase[offset];
    page->offset = offset;
    page->property = n; //其中记录块大小
    ClearPageProperty(page);
    SetPageReserved(page);
    nr_free -= n;
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(base != NULL); //释放的时候直接释放块，就不需要再输入块大小
    assert(base->offset >= 0 && base->offset <TOTAL_PAGE_NUM);
    assert(IS_POWER_OF_2(base->property));
    struct Page *p = base;

    unsigned node_size, index = 0;
    unsigned left_size, right_size;

    node_size = p->property;
    index = (p->offset + TOTAL_PAGE_NUM) / p->property - 1; //最末节点
    //cprintf("index in free:%d\n",index);
   /* for (; buddy[index]  != 0; index = PARENT(index)) { //向上一直找到已经被分配出去的节点
    	node_size *= 2;
        if (index == 0)
        	return;
    }*/

    buddy[index] = node_size;
    nr_free += node_size;

    while (index)  { //向上回溯合并节点
    	index = PARENT(index);
        node_size *= 2;

        left_size = buddy[LEFT_LEAF(index)];
        right_size = buddy[RIGHT_LEAF(index)];
        //cprintf("free leftsize:%d\n",left_size);
        //cprintf("free rightsize:%d\n",right_size);
        //cprintf("free nodesize:%d\n",node_size);

        if (left_size + right_size == node_size) {
              buddy[index] = node_size;
        } else {
              buddy[index] = MAX(left_size, right_size);
        }
    }

    p->flags = 0;
    set_page_ref(p, 0);
    SetPageProperty(p);
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}


//针对buddy算法的测试
static void buddy_check(void) {
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



}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};

