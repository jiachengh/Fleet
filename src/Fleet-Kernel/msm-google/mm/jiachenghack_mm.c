#include <asm/mmu_context.h>

#include "jiachenghack_mm.h"

#include "internal.h"

static unsigned long shrink_page(
	struct page *page, 
	struct pglist_data *pgdat, 
	struct jiacheng_scan_control *sc, 
	enum ttu_flags ttu_flags,
	unsigned long *ret_nr_dirty,
	unsigned long *ret_nr_unqueued_dirty,
	unsigned long *ret_nr_congested,
	unsigned long *ret_nr_writeback,
	unsigned long *ret_nr_immediate,
	bool force_reclaim,
	struct list_head *ret_list) {
	
	int reclaimed;
	LIST_HEAD(page_list);
	list_add(&page->lru, &page_list);
	reclaimed = jiacheng_shrink_page_list(
		&page_list,
		pgdat, 
		sc,
		ttu_flags,
		ret_nr_dirty,
		ret_nr_unqueued_dirty,
		ret_nr_congested,
		ret_nr_writeback,
		ret_nr_immediate,
		force_reclaim
	);
	if (!reclaimed) {
		list_splice(&page_list, ret_list);
	}
	return reclaimed;
}

static unsigned long cold_pages_from_list(struct list_head *page_list) {
	struct jiacheng_scan_control sc = {
		.gfp_mask = GFP_KERNEL,
		.priority = DEF_PRIORITY,
		.may_unmap = 1,
		.may_swap = 1,
	};
	LIST_HEAD(ret_list);
	struct page *page;
	unsigned long dummy1, dummy2, dummy3, dummy4, dummy5;
	unsigned long nr_reclaimed = 0;
	
	while (!list_empty(page_list)) {
		page = lru_to_page(page_list);
		list_del(&page->lru);
		ClearPageActive(page);
		nr_reclaimed += shrink_page(
			page, 
			page_pgdat(page), 
			&sc, 
			TTU_UNMAP|TTU_IGNORE_ACCESS, 
			&dummy1, 
			&dummy2, 
			&dummy3, 
			&dummy4,
			&dummy5,
			true, 
			&ret_list);
	}

	while (!list_empty(&ret_list)) {
		page = lru_to_page(&ret_list);
		list_del(&page->lru);
		putback_lru_page(page);
	}
	return nr_reclaimed;
}

static int cold_pte_range(pmd_t *pmd, unsigned long addr, unsigned long end, struct mm_walk *walk) { // require --> down_read(&mm->mmap_sem);
	struct vm_area_struct *vma = walk->vma;
	pte_t *pte, ptent;
	spinlock_t *ptl;
	struct page *page;
	LIST_HEAD(page_list);
	int isolated;
repeat:
	isolated = 0;
	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	for (; addr != end; pte++, addr += PAGE_SIZE) {
		ptent = *pte;
		if (!pte_present(ptent)) {
			continue;
		}
		page = vm_normal_page(vma, addr, ptent);
		if (!page) {
			continue;
		}
		if (isolate_lru_page(page)) {
			continue;
		}
		list_add(&page->lru, &page_list);
		isolated++;
		if (isolated >= SWAP_CLUSTER_MAX) {
			break;
		}
	}
	pte_unmap_unlock(pte - 1, ptl);
	cold_pages_from_list(&page_list);
	if (addr != end) {
		goto repeat;
	}
	return 0;
}


// madvise调用
void jiacheng_page_range_cold(struct vm_area_struct *vma, unsigned long start, unsigned long size)
{
	struct task_struct *task;
	struct mm_struct *mm;
	
	printk("jiachenghack_mm.c jiacheng_page_range_cold start=%lu size=%lu", start, size);
	rcu_read_lock();
	task = current;
	get_task_struct(task);
	rcu_read_unlock();

	mm = get_task_mm(task);
	if (mm) {
		struct mm_walk cold_walk = {
			.pmd_entry = cold_pte_range,
			.mm = mm,
			.vma = vma,
		};
		down_read(&mm->mmap_sem);
		walk_page_range(start, start + size, &cold_walk);
		flush_tlb_mm(mm);
		up_read(&mm->mmap_sem);
		mmput(mm);
	}
	put_task_struct(task);
} 