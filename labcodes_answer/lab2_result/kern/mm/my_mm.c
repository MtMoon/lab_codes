#include <pmm.h>
#include <list.h>
#include <string.h>

extern const struct pmm_manager my_pmm_manager;

free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void my_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void my_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        p->flags = 0;
        p->property = 0;
        list_add_before(&free_list, &(p->page_link));
    }
    nr_free += n;
    //first block
    base->property = n;
}

//最先匹配算法
static struct Page * my_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    list_entry_t *le, *len;
    le = &free_list;

    while ((le=list_next(le)) != &free_list) {
    	struct Page *p = le2page(le, page_link); //一个空闲分区由多个page合在一起

    	if (p->property >= n) {
    		int i;
    		for (i=0; i<n; i++) {
    			len = list_next(le);
    			struct Page *pp = le2page(le, page_link);
    			pp->property = 0; //SetPageReserved
    			list_del(le);
    			le = len; //退出后le变成head后的第一个了
    		}

    		if(p->property>n){
    			(le2page(le,page_link))->property = p->property - n; //如果恰好等于n，则该空闲分区被完全个分出去，未产生碎片
    		}

    		p->property = 0;
    		nr_free -= n;
    		return p;
    	}
    }
    return NULL;
}

static void my_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(base->property == 0);

    list_entry_t *le = &free_list;
    struct Page * p;
    while ((le=list_next(le)) != &free_list) {
    	p = le2page(le, page_link);
    	if(p>base){
    		break;
    	}
    } //退出循环后，le指向base的后一个空闲page

    //list_add_before(le, base->page_link);
    for (p=base; p<base+n; p++) {
    	list_add_before(le, &(p->page_link)); //把被释放的page插回
    }

    base->flags = 0;
    base->property = n;

    //合并相邻空闲块
    p = le2page(le,page_link) ;
    if ( base+n == p ) { //若释放后的空闲块与le开始的空闲块相邻则合并，le在base后
      base->property += p->property;
      p->property = 0;
    }
    le = list_prev(&(base->page_link));
    p = le2page(le, page_link); //le指向base的前一个page

    if (le!=&free_list && p==base-1) {
      while(le!=&free_list){

    	  if(p->property) { //找到base前的相邻空闲块
    		  p->property += base->property;
    		  base->property = 0;
    		  break;
    	  }
    	  le = list_prev(le);
    	  p = le2page(le,page_link);
      }
    }

    nr_free += n; //总可用空闲块增加
    return ;
}

static size_t my_nr_free_pages(void) {
    return nr_free;
}


const struct pmm_manager my_pmm_manager = {
    .name = "my_pmm_manager",
    .init = my_init,
    .init_memmap = my_init_memmap,
    .alloc_pages = my_alloc_pages,
    .free_pages = my_free_pages,
    .nr_free_pages = my_nr_free_pages,
    .check = NULL,
};

