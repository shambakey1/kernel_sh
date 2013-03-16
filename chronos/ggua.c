/* chronos/ggua.c
 *
 * Greedy Global Utility Accrual scheduling algorithm
 *
 * Author(s)
 *	- Piyush Garyali, piyushg@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/module.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>
#include <linux/chronos_global.h>
#include <linux/list.h>
#include <linux/string.h>

struct rt_info * sched_ggua(struct list_head *head, struct global_sched_domain *g)
{
	struct rt_info *givdhead, *zihead, *it, *n;
	struct rt_info *best_dead[NR_CPUS];
	struct cpu_info *cur_cpu = NULL;
	struct rt_info *best_dead_old = NULL;
	int failed, cpu_id, cpus = count_global_cpus(g);
	int _mask[NR_CPUS];

	for(cpu_id = 0; cpu_id < NR_CPUS; cpu_id++) {
		best_dead[cpu_id] = NULL;
	}

	list_for_each_entry_safe(it, n, head, task_list[GLOBAL_LIST])  {
		if(check_task_failure(it, SCHED_FLAG_NONE)) {
			_remove_task_global(it, g);
			continue;
		}

		livd(it, false, SCHED_FLAG_NONE);
		initialize_lists(it);
		initialize_graph(it);
	}

	zihead = find_zero_indegree_tasks(head, SCHED_FLAG_NONE);
	if(!zihead)
		goto out;

	givdhead = zihead;
	it = task_list_entry(zihead->task_list[LIST_ZINDEG].next, LIST_ZINDEG);
	do {
		if(insert_on_list(it, givdhead, LIST_CPUIVD, SORT_KEY_GVD, 0))
			givdhead = it;
		it = task_list_entry(it->task_list[LIST_ZINDEG].next, LIST_ZINDEG);
	} while(it != zihead);

	initialize_cpu_state();
	it = givdhead;

	do {
		memset(_mask, 0, NR_CPUS);

		do {
			failed = 0;
			cpu_id = find_processor_ex (_mask, cpus);

			/* "it" is not feasible on any cpu, move on to next */
			if(cpu_id == -1) {
				break;
			}

			insert_cpu_task(it, cpu_id);
			update_cpu_exec_times(cpu_id, it, true);
			cur_cpu = get_cpu_state(cpu_id);
			best_dead_old = cur_cpu->best_dead;

			if(cur_cpu->best_dead == NULL) {
				cur_cpu->best_dead = it;
			} else {
				if(insert_on_list(it, cur_cpu->best_dead,
						LIST_CPUDEAD, SORT_KEY_DEADLINE, 0))
					cur_cpu->best_dead = it;
			}

			if(!list_is_feasible(cur_cpu->best_dead, LIST_CPUDEAD)) {
				list_remove_init(it, LIST_CPUTSK);
				list_remove_init(it, LIST_CPUDEAD);
				cur_cpu->best_dead = best_dead_old;
				update_cpu_exec_times(cpu_id, it, false);
				SETBIT(_mask, cpu_id);
				failed = 1;
			}
		} while(failed);

		it = task_list_entry(it->task_list[LIST_CPUIVD].next, LIST_CPUIVD);
	} while(it != givdhead);

	for(cpu_id = 0;  cpu_id < cpus; cpu_id++) {
		cur_cpu = get_cpu_state(cpu_id);
		best_dead[cpu_id] = cur_cpu->best_dead;
	}

	build_list_array(best_dead, cpus);

out:
	return best_dead[0];
}

struct rt_sched_global ggua = {
	.base.name = "GGUA",
	.base.id = SCHED_RT_GGUA,
	.schedule = sched_ggua,
	.preschedule = presched_abort_generic,
	.arch = &rt_sched_arch_stw,
	.local = SCHED_RT_FIFO_RA,
	.base.sort_key = SORT_KEY_NONE,
	.base.list = LIST_HEAD_INIT(ggua.base.list)
};

static int __init ggua_init(void)
{
	return add_global_scheduler(&ggua);
}
module_init(ggua_init);

static void __exit ggua_exit(void)
{
	remove_global_scheduler(&ggua);
}
module_exit(ggua_exit);

MODULE_DESCRIPTION("Greedy Global Utility Accrual Module for ChronOS");
MODULE_AUTHOR("Piyush Garyali <piyushg@vt.edu>");
MODULE_LICENSE("GPL");

