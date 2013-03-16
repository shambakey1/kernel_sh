/* chronos/nggua.c
 *
 * Non-Greedy Global Utility Accrual scheduling algorithm
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

/* Check the task list for the cpu and check for schedule feasibility */
struct rt_info* create_feasible_schedule(int cpu_id)
{
	struct rt_info *it, *best_dead, *head, *best_ivd, *last_ivd;
	struct cpu_info *cur_cpu = NULL;
	struct timespec exec_ts;
	int removed;

	cur_cpu = get_cpu_state(cpu_id);
	head = cur_cpu->head;
	best_dead = head;

	if(!head)
		goto out;

	best_ivd = head;
	it = task_list_entry(head->task_list[LIST_CPUTSK].next, LIST_CPUTSK);
	do {
		if(insert_on_list(it, best_ivd, LIST_CPUIVD, SORT_KEY_GVD, 0))
			best_ivd = it;
		it = task_list_entry(it->task_list[LIST_CPUTSK].next, LIST_CPUTSK);
	} while(it != head);

	last_ivd = task_list_entry(best_ivd->task_list[LIST_CPUIVD].prev, LIST_CPUIVD);

	do {
		removed = 0;
		it = best_dead;
		exec_ts = current_kernel_time();

		do {
			add_ts(&exec_ts, &(it->left), &exec_ts);
			if(earlier_deadline(&(it->deadline), &exec_ts)) {
				list_remove(last_ivd, LIST_CPUTSK);
				if(last_ivd == best_dead) {
				   best_dead = task_list_entry(last_ivd->task_list[LIST_CPUTSK].next, LIST_CPUTSK);
				}
				last_ivd = task_list_entry(last_ivd->task_list[LIST_CPUIVD].prev, LIST_CPUIVD);
				removed = 1;
			}
			it = task_list_entry(it->task_list[LIST_CPUTSK].next, LIST_CPUTSK);
		} while(removed == 0 && it != best_dead);
	} while(last_ivd != best_ivd && removed == 1);

out:
	cur_cpu->best_dead = best_dead;

	return best_dead;
}

struct rt_info * sched_nggua(struct list_head *head, struct global_sched_domain *g)
{
	struct rt_info *zihead, *it, *n, *best_tdead;
	struct rt_info *best_dead[NR_CPUS];
	int cpu_id, cpus = count_global_cpus(g);

	for(cpu_id = 0; cpu_id < NR_CPUS; cpu_id++)
		best_dead[cpu_id] = NULL;

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

	best_tdead = zihead;
	it = task_list_entry(zihead->task_list[LIST_ZINDEG].next, LIST_ZINDEG);
	do {
		if(insert_on_list(it, best_tdead, LIST_TDEAD, SORT_KEY_TDEADLINE, 0))
			best_tdead = it;
		it = task_list_entry(it->task_list[LIST_ZINDEG].next, LIST_ZINDEG);
	} while(it != zihead);

	initialize_cpu_state();

	/* Assign the zero in degree tasks to the processors */
	it = best_tdead;
	do {
		cpu_id = find_processor(cpus);
		insert_cpu_task(it, cpu_id);
		update_cpu_exec_times(cpu_id, it, true);
		it = task_list_entry(it->task_list[LIST_TDEAD].next, LIST_TDEAD);
	} while(it != best_tdead);

	for(cpu_id = 0;  cpu_id < cpus; cpu_id++) {
		best_dead[cpu_id] = create_feasible_schedule(cpu_id);
	}

	build_list_array(best_dead, cpus);

out:
	return best_dead[0];
}

struct rt_sched_global nggua = {
	.base.name = "NGGUA",
	.base.id = SCHED_RT_NGGUA,
	.schedule = sched_nggua,
	.preschedule = presched_abort_generic,
	.arch = &rt_sched_arch_stw,
	.local = SCHED_RT_FIFO_RA,
	.base.sort_key = SORT_KEY_NONE,
	.base.list = LIST_HEAD_INIT(nggua.base.list)
};

static int __init nggua_init(void)
{
	return add_global_scheduler(&nggua);
}
module_init(nggua_init);

static void __exit nggua_exit(void)
{
	remove_global_scheduler(&nggua);
}
module_exit(nggua_exit);

MODULE_DESCRIPTION("Non Greedy Global Utility Accrual Module for ChronOS");
MODULE_AUTHOR("Piyush Garyali <piyushg@vt.edu>");
MODULE_LICENSE("GPL");

