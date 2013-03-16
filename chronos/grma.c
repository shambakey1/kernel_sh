/* chronos/grma.c
 *
 * Global RMA Scheduler Module for ChronOS
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

struct rt_info * sched_grma(struct list_head *head, struct global_sched_domain *g)
{
	int count = 1, cpus = count_global_cpus(g);
	struct rt_info *it, *lowest = get_global_task(head->next);
	struct list_head tmp_head;
	struct rt_info *tmp_it, *spin_tmp_it=NULL;

	it = lowest;
	INIT_LIST_HEAD(&lowest->task_list[SCHED_LIST1]);
        /********************* SH-ST *********************/
        INIT_LIST_HEAD(&tmp_head);      /*This list contains tasks of GLOBAL_LIST in the same 
                                         * order except SPINNING and SUSPENDED tasks */
        list_for_each_entry(tmp_it,head,task_list[GLOBAL_LIST]){
            /* SCHED_LIST2 will contain tasks of GLOBAL_LIST except that all SPINNING tasks will 
             * be at the head, SUSPENDED tasks will not be included, and remaining tasks will be 
             * included according to their order in GLOBAL_LIST*/
            if(tmp_it->rt_status==NON_PREEMPTIVE ){
                /*In order to keep SPINNING tasks in their priority order, spin_tmp_it points 
                 * to the last added SPINNING task in SCHED_LIST2 */
                if(!spin_tmp_it){
                    list_add(&(tmp_it->task_list[SCHED_LIST2]),&tmp_head);
                }
                else{
                    list_add(&(tmp_it->task_list[SCHED_LIST2]),&(spin_tmp_it->task_list[SCHED_LIST2]));
                }
                spin_tmp_it=tmp_it;
            }
            else if(tmp_it->rt_status==SUSPENDED){
                continue;
            }
            else{
                list_add_tail(&(tmp_it->task_list[SCHED_LIST2]),&tmp_head);
            }
        }
        lowest=list_first_entry(&(tmp_head), struct rt_info, task_list[SCHED_LIST2]);
        it=lowest;
        

	//list_for_each_entry_continue(it, head, task_list[GLOBAL_LIST]) {
        list_for_each_entry_continue(it, head, task_list[SCHED_LIST2]) {
        /********************* SH-END *********************/
            count++;
		list_add_before(lowest, it, SCHED_LIST1);

		if(count == cpus)
			break;
	}

	return lowest;
}

struct rt_sched_global grma = {
	.base.name = "GRMA",
	.base.id = SCHED_RT_GRMA,
	.schedule = sched_grma,
	.preschedule = presched_stw_generic,
	.arch = &rt_sched_arch_stw,
	.local = SCHED_RT_FIFO,
        /******************** SH-ST *********************/
	//.base.sort_key = SORT_KEY_PERIOD,
        .base.sort_key = SORT_KEY_TPERIOD,
        /******************** SH-END *********************/
	.base.list = LIST_HEAD_INIT(grma.base.list)
};

static int __init grma_init(void)
{
	return add_global_scheduler(&grma);
}
module_init(grma_init);

static void __exit grma_exit(void)
{
	remove_global_scheduler(&grma);
}
module_exit(grma_exit);

MODULE_DESCRIPTION("Global RMA Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

