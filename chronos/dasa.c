/* chronos/dasa.c
 *
 * DASA Single-Core Scheduler Module for ChronOS
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

/*FIXME: finish implementation sometime */

struct rt_info* sched_dasa(struct list_head *head, int flags)
{
	struct list_head *curr;
	struct rt_info *it = list_entry(head->next, struct rt_info, task_list[LOCAL_LIST]);
	struct rt_info *best_ivd = NULL, *last_ivd = NULL;
	struct rt_info *best_dead = NULL, *best_dead_old = NULL;
	long civd;

	list_for_each(curr, head) {
		it = list_entry(curr, struct rt_info, task_list[LOCAL_LIST]);
		initialize_lists(it);

		if(check_task_failure(it, flags))
			return it;

		civd = livd(it, true, false);

		/* There are four possible cases:
		 * Either the list is empty, our item should be first,
		 * our item should be last, or none of the above, in
		 * which case we place it as second and let the sort take
		 * care of it.
		 */
		if(best_ivd == NULL)
			best_ivd = it;
		else if(civd < best_ivd->local_ivd) {
			list_add_after(last_ivd, it, SCHED_LIST1);
			best_ivd = it;
		} else if (civd > last_ivd->local_ivd) {
			list_add_after(last_ivd, it, SCHED_LIST1);
		} else
			list_add_after(best_ivd, it, SCHED_LIST1);
		last_ivd = task_list_entry(best_ivd->task_list[SCHED_LIST1].prev, SCHED_LIST1);
	}

	/* Check if there are at least four item in the list before sorting */
	if(best_ivd != NULL && last_ivd != NULL) {
		if(best_ivd->task_list[SCHED_LIST1].next != last_ivd->task_list[SCHED_LIST1].prev)
			quicksort(best_ivd, SCHED_LIST1, SORT_KEY_LVD, 0);
	} else
		return it;

	it = best_ivd;

	do {
		/* Remember the original head of the list */
		best_dead_old = best_dead;

		/* Insert each task in the EDF ordered list
		 * We do an insert_after here because we are inserting the tasks
		 * in IVD order, so if there is already a task in the list at
		 * this point, it will be preferable.
		 */
		if(best_dead == NULL)
			best_dead = it;
		else {
			if(insert_on_list(it, best_dead, SCHED_LIST2, SORT_KEY_DEADLINE, 0) == 1)
				best_dead = it;
		}

		/* If the schedule is feasible, commit the changes
		 * Otherwise, overwrite the temp list
		 */
		if(list_is_feasible(best_dead, SCHED_LIST2)) {
			copy_list(best_dead, SCHED_LIST2, SCHED_LIST3);
		}
		else {
			if(best_dead_old != NULL)
				copy_list(best_dead_old, SCHED_LIST3, SCHED_LIST2);
			best_dead = best_dead_old;
		}

		/* Find the next task that's not already in the list somewhere */
		do {
			it = task_list_entry(it->task_list[SCHED_LIST1].next, SCHED_LIST1);
		} while (!list_empty(&it->task_list[SCHED_LIST2]) && it != best_ivd);

	} while(it != best_ivd);


	return best_dead == NULL ? best_ivd : best_dead;
}

struct rt_sched_local dasa = {
	.base.name = "DASA",
	.base.id = SCHED_RT_DASA,
	.flags = 0,
	.schedule = sched_dasa,
	.base.sort_key = SORT_KEY_DEADLINE,
	.base.list = LIST_HEAD_INIT(dasa.base.list)
};

static int __init dasa_init(void)
{
	return add_local_scheduler(&dasa);
}
module_init(dasa_init);

static void __exit dasa_exit(void)
{
	remove_local_scheduler(&dasa);
}
module_exit(dasa_exit);

MODULE_DESCRIPTION("DASA Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

