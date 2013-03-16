/* chronos/hvdf.c
 *
 * HVDF Single-Core Scheduler Module for ChronOS
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

struct rt_info* sched_hvdf(struct list_head *head, int flags)
{
	struct rt_info *it, *best = local_task(head->next);

	list_for_each_entry(it, head, task_list[LOCAL_LIST]) {
		if(check_task_failure(it, flags))
			return it;

		livd(it, false, flags);
		if(it->local_ivd < best->local_ivd)
			best = it;
	}

	if(flags & SCHED_FLAG_PI)
		best = get_pi_task(best, head, flags);

	return best;
}

struct rt_sched_local hvdf = {
	.base.name = "HVDF",
	.base.id = SCHED_RT_HVDF,
	.flags = 0,
	.schedule = sched_hvdf,
	.base.sort_key = SORT_KEY_DEADLINE,
	.base.list = LIST_HEAD_INIT(hvdf.base.list)
};

static int __init hvdf_init(void)
{
	return add_local_scheduler(&hvdf);
}
module_init(hvdf_init);

static void __exit hvdf_exit(void)
{
	remove_local_scheduler(&hvdf);
}
module_exit(hvdf_exit);

MODULE_DESCRIPTION("hvdf Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

