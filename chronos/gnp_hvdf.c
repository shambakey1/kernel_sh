/* chronos/gnp_hvdf.c
 *
 * Global Non-Preemptible HVDF Scheduler Module for ChronOS
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
#include <linux/smp.h>

struct rt_info * sched_gnp_hvdf(struct list_head *head, struct global_sched_domain *g)
{
	int cpu = raw_smp_processor_id();
	struct rt_info *it;

	list_for_each_entry(it, head, task_list[GLOBAL_LIST]) {
		if(task_pullable(it, cpu)) {
			check_task_failure(it, SCHED_FLAG_NONE);
			_remove_task_global(it, g);
			return it;
		}
	}

	return NULL;
}

struct rt_sched_global gnp_hvdf = {
	.base.name = "GNP_HVDF",
	.base.id = SCHED_RT_GNP_HVDF,
	.schedule = sched_gnp_hvdf,
	.preschedule = presched_concurrent_generic,
	.arch = &rt_sched_arch_concurrent,
	.local = SCHED_RT_FIFO,
	.base.sort_key = SORT_KEY_LVD,
	.base.list = LIST_HEAD_INIT(gnp_hvdf.base.list)
};

static int __init gnp_hvdf_init(void)
{
	return add_global_scheduler(&gnp_hvdf);
}
module_init(gnp_hvdf_init);

static void __exit gnp_hvdf_exit(void)
{
	remove_global_scheduler(&gnp_hvdf);
}
module_exit(gnp_hvdf_exit);

MODULE_DESCRIPTION("Global Non-Preemptible HVDF Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

