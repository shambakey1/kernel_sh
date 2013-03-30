/* chronos/gedf.c
 *
 * Global EDF Scheduler Module for ChronOS
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

struct rt_info * sched_gedf(struct list_head *head, struct global_sched_domain *g)
{
	int count = 1, cpus = count_global_cpus(g);
	//struct rt_info *it, *earliest = get_global_task(head->next);
	struct rt_info *it, *tmp_it, *spin_tmp_it, *earliest;
	struct list_head tmp_head;
	*spin_tmp_it=NULL;

	//it = earliest;
	//INIT_LIST_HEAD(&earliest->task_list[SCHED_LIST1]);
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
        earliest=list_first_entry(&(tmp_head), struct rt_info, task_list[SCHED_LIST2]);
        it=earliest;
        INIT_LIST_HEAD(&earliest->task_list[SCHED_LIST1]);
        

	//list_for_each_entry_continue(it, head, task_list[GLOBAL_LIST]) {
        list_for_each_entry_continue(it, &tmp_head, task_list[SCHED_LIST2]) {
        /********************* SH-END *********************/
		count++;
		list_add_before(earliest, it, SCHED_LIST1);

		if(count == cpus)
			break;
	}

	return earliest;
}

struct rt_sched_global gedf = {
	.base.name = "GEDF",
	.base.id = SCHED_RT_GEDF,
	.schedule = sched_gedf,
	.preschedule = presched_stw_generic,
	.arch = &rt_sched_arch_stw,
	.local = SCHED_RT_FIFO,
        /******************* SH-ST ********************/
	//.base.sort_key = SORT_KEY_DEADLINE,
        .base.sort_key = SORT_KEY_TDEADLINE,
        /******************* SH-END ********************/
	.base.list = LIST_HEAD_INIT(gedf.base.list)
};

static int __init gedf_init(void)
{
	return add_global_scheduler(&gedf);
}
module_init(gedf_init);

static void __exit gedf_exit(void)
{
	remove_global_scheduler(&gedf);
}
module_exit(gedf_exit);

MODULE_DESCRIPTION("Global EDF Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

