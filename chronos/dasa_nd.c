/* chronos/dasa_nd.c
 *
 * DASA_ND Single-Core Scheduler Module for ChronOS
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/module.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>
#include <linux/chronos_util.h>
#include <linux/list.h>

struct rt_info* sched_dasa_nd(struct list_head *head, int flags)
{
	struct rt_info *it, *best_ivd, *best_dead = NULL, *best_dead_old;

	best_ivd = local_task(head->next);

	list_for_each_entry(it, head, task_list[LOCAL_LIST]) {
		if(check_task_failure(it, flags))
			return it;

		initialize_lists(it);
		livd(it, false, flags);
		list_add_before(best_ivd, it, SCHED_LIST1);

		if(it->local_ivd < best_ivd->local_ivd)
			best_ivd = it;
	}

	quicksort(best_ivd, SCHED_LIST1, SORT_KEY_LVD, 0);
	it = best_ivd;

	do {
		/* Remember the original head of the list */
		best_dead_old = best_dead;

		/* insert each task in the EDF ordered list */
		if(best_dead) {
			if(insert_on_list(it, best_dead, SCHED_LIST2, SORT_KEY_DEADLINE, 0) == 1)
				best_dead = it;
		} else
			best_dead = it;

		if(!list_is_feasible(best_dead, SCHED_LIST2)) {
			list_remove(it, SCHED_LIST2);
			best_dead = best_dead_old;
		}

		it = task_list_entry(it->task_list[SCHED_LIST1].next, SCHED_LIST1);
	} while(it != best_ivd);

	return best_dead ? best_dead : best_ivd;
}

struct rt_sched_local dasa_nd = {
	.base.name = "DASA_ND",
	.base.id = SCHED_RT_DASA_ND,
	.flags = 0,
	.schedule = sched_dasa_nd,
	.base.sort_key = SORT_KEY_DEADLINE,
	.base.list = LIST_HEAD_INIT(dasa_nd.base.list)
};

static int __init dasa_nd_init(void)
{
	return add_local_scheduler(&dasa_nd);
}
module_init(dasa_nd_init);

static void __exit dasa_nd_exit(void)
{
	remove_local_scheduler(&dasa_nd);
}
module_exit(dasa_nd_exit);

MODULE_DESCRIPTION("DASA_ND Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

