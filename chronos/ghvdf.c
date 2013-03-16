/* chronos/ghvdf.c
 *
 * Global HVDF Scheduler Module for ChronOS
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/module.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>
#include <linux/list.h>

struct rt_info * sched_ghvdf(struct list_head *head, struct global_sched_domain *g)
{
	int count = 0, cpus = count_global_cpus(g);
	struct rt_info *it, *n, *best_ivd = NULL;

	list_for_each_entry_safe(it, n, head, task_list[GLOBAL_LIST]) {
		if(check_task_failure(it, SCHED_FLAG_NONE)) {
			_remove_task_global(it, g);
			continue;
		}

		INIT_LIST_HEAD(&it->task_list[SCHED_LIST1]);
		livd(it, false, SCHED_FLAG_NONE);
		count++;

		if(!best_ivd)
			best_ivd = it;
		else if(insert_on_list(it, best_ivd, SCHED_LIST1, SORT_KEY_LVD, 0))
			best_ivd = it;

		if(count > cpus) {
			list_remove(task_list_entry(best_ivd->task_list[SCHED_LIST1].prev, SCHED_LIST1), SCHED_LIST1);
			count--;
		}
	}

	return best_ivd;
}

struct rt_sched_global ghvdf = {
	.base.name = "GHVDF",
	.base.id = SCHED_RT_GHVDF,
	.schedule = sched_ghvdf,
	.preschedule = presched_abort_generic,
	.arch = &rt_sched_arch_stw,
	.local = SCHED_RT_FIFO,
	.base.sort_key = SORT_KEY_NONE,
	.base.list = LIST_HEAD_INIT(ghvdf.base.list)
};

static int __init ghvdf_init(void)
{
	return add_global_scheduler(&ghvdf);
}
module_init(ghvdf_init);

static void __exit ghvdf_exit(void)
{
	remove_global_scheduler(&ghvdf);
}
module_exit(ghvdf_exit);

MODULE_DESCRIPTION("Global HVDF Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

