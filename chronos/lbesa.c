/* chronos/lbesa.c
 *
 * LBESA Single-Core Scheduler Module for ChronOS
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

struct rt_info* sched_lbesa(struct list_head *head, int flags)
{
	struct rt_info *it, *best_ivd, *last_ivd, *best_dead;

	best_ivd = local_task(head->next);
	best_dead = best_ivd;
	initialize_lists(best_ivd);

	list_for_each_entry(it, head, task_list[LOCAL_LIST]) {
		if(check_task_failure(it, flags))
			return it;

		livd(it, false, flags);
		list_add_before(best_ivd, it, SCHED_LIST1);
		list_add_before(best_dead, it, SCHED_LIST2);

		if(it->local_ivd < best_ivd->local_ivd)
			best_ivd = it;
	}

	quicksort(best_ivd, SCHED_LIST1, SORT_KEY_LVD, 0);
	last_ivd = task_list_entry(best_ivd->task_list[SCHED_LIST1].prev, SCHED_LIST1);

	/* Remove the lowest VD task while the schedule is infeasible */
	do {
		if(list_is_feasible(best_dead, SCHED_LIST2))
			break;

		if(last_ivd == best_dead)
			best_dead = task_list_entry(best_dead->task_list[SCHED_LIST2].next, SCHED_LIST2);

		list_remove(last_ivd, SCHED_LIST2);
		last_ivd = task_list_entry(last_ivd->task_list[SCHED_LIST1].prev, SCHED_LIST1);
	} while (last_ivd != best_ivd);

	return best_dead;
}

struct rt_sched_local lbesa = {
	.base.name = "LBESA",
	.base.id = SCHED_RT_LBESA,
	.flags = 0,
	.schedule = sched_lbesa,
	.base.sort_key = SORT_KEY_DEADLINE,
	.base.list = LIST_HEAD_INIT(lbesa.base.list)
};

static int __init lbesa_init(void)
{
	return add_local_scheduler(&lbesa);
}
module_init(lbesa_init);

static void __exit lbesa_exit(void)
{
	remove_local_scheduler(&lbesa);
}
module_exit(lbesa_exit);

MODULE_DESCRIPTION("LBESA Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

