/* chronos/chronos_mutex.c
 *
 * Scheduler-managed mutex to be used by real-time tasks
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 *
 * Based on normal futexes, (C) Rusty Russell.
 *
 * Quick guide:
 * We are using a data structure (struct mutex_data, in linux/chronos_types.h)
 * as a mutex. The user creates it through C/C++/Java/etc, and then passes the
 * pointer down in a system call.
 */

#include <asm/current.h>
#include <asm/atomic.h>
#include <asm/futex.h>
#include <linux/futex.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/chronos_types.h>

#ifdef CONFIG_CHRONOS

#include <linux/chronos_sched.h>
#include "chronos_mutex_stats.c"

/* The reason we need a list at all is that A) we need the address we're
 * offsetting from in the userspace to be a slab-allocated address so that it
 * isn't findable via objdumping the vmlinx, and B) we need a way to reference
 * the memory to free it if the application dies without properly informing us,
 * since we have to assume userspace is unreliable.
 */

struct process_mutex_list {
	pid_t tgid;
	struct list_head p_list;
	struct list_head m_list;
	rwlock_t lock;
};

static LIST_HEAD(chronos_mutex_list);
static DEFINE_RWLOCK(chronos_mutex_list_lock);

static struct process_mutex_list * find_by_tgid(pid_t pid)
{
	struct list_head *curr;
	struct process_mutex_list *entry, *ret = NULL;

	read_lock(&chronos_mutex_list_lock);
	list_for_each(curr, &chronos_mutex_list) {
		entry = list_entry(curr, struct process_mutex_list, p_list);
		if(entry->tgid == pid) {
			ret = entry;
			break;
		}
	}
	read_unlock(&chronos_mutex_list_lock);

	return ret;
}

static struct mutex_head * find_in_process(struct mutex_data *m, struct process_mutex_list * process)
{
	struct mutex_head *head = (struct mutex_head *)((unsigned long)process + m->id);

	if(head->id != m->id)
		head = NULL;

	return head;
}

static struct mutex_head * find_mutex(struct mutex_data *m, int tgid)
{
	struct process_mutex_list *p = NULL;

	p = find_by_tgid(tgid);
	return find_in_process(m, p);
}

static void futex_wait(u32 __user *uaddr, u32 val)
{
	do_futex(uaddr, FUTEX_WAIT, val, NULL, NULL, 0, 0);
}

/* It might seem counter-intutitive that we're only waking one task, rather than
 * all of them, since all of them should be eligeble for being run. However, if
 * we get here, the scheduler isn't managing wake-ups, and so we do this to
 * reduce unnecessary wakeups.
 */
static void futex_wake(u32 __user *uaddr)
{
	do_futex(uaddr, FUTEX_WAKE, 1, NULL, NULL, 0, 0);
}

/*********************** SH-ST ***************************/
static inline long long int t2l(struct timespec* t_param){
    //converts time paramter (t_param) to long long int
    return (t_param->tv_sec*1000000000+t_param->tv_nsec);
}

static inline int compare_ts_tmp(struct timespec *t1, struct timespec *t2)
{
	if (t1->tv_sec < t2->tv_sec || (t1->tv_sec == t2->tv_sec &&
			t1->tv_nsec < t2->tv_nsec))
		return 1;
	else
		return 0;
}

#define lower_period_tmp(t1, t2) compare_ts_tmp(t1, t2)
#define earlier_deadline_tmp(t1, t2) compare_ts_tmp(t1, t2)

//static void check_pi_forward(struct task_mutex *tmp_tm,int sched_no){
static void check_pi_forward(struct rt_info *tmp_tm,int sched_no){
    //This function checks changes to priority inheritance triggered by rt_info in tmp_tm.
    /**** Rationale behind check_pi ****
     * First, tmp_tm is initialized to a specific task and added to a check list named 'check_list'. 
     * Temporal priority of each task preceeding current task in all conflicting mutex_head is 
     * compared to temporal priority of current task. If temporal priority of current task is bigger 
     * than the next one, then temporal priority of next task inherits that of current task, and the 
     * next task is added to the check_list. This process continues until 'check_list' contains no more
     * tasks. The name 'forward' stems from forwarding priority of current task to lower priority 
     * previous tasks in conflicting mutexes.
     */
    struct pi_test_list *it, *it1;    //Iterators to traverse check_list
    struct task_mutex *it2;     //Iterator to traverse all task_mutex of current task
    struct mutex_task *it3;     //Pointer to precedent to it2 in the specified mutex_head
    struct list_head check_list;     //used to check priority inheritance
    struct pi_test_list *tmp_pi_node;
    INIT_LIST_HEAD(&(check_list));
    tmp_pi_node=(struct pi_test_list*)(kmalloc(sizeof(struct pi_test_list), GFP_ATOMIC));
    tmp_pi_node->task_node=tmp_tm;      //Initial value
    list_add_tail(&(tmp_pi_node->pi_list),&(check_list));
    list_for_each_entry(it, &(check_list), pi_list){
        //Now check if previous task in each conflicting mutex_head should inherit priority from current task
        list_for_each_entry(it2,&(it->task_node->req_res),m_list){
            it3=NULL;
            it3=list_entry(it2->mt->fifo_list.prev,struct mutex_task,fifo_list);        //check if it3 does not point to head (head always points to NULL)
            switch(sched_no){
                case SCHED_RT_GEDF:
                    if((it3->tid) && earlier_deadline_tmp(it2->mt->tid->temp_deadline,it3->tid->temp_deadline)){
                        it3->tid->temp_deadline=it2->mt->tid->temp_deadline;
                        //Append the new task in the check_list
                        tmp_pi_node=(struct pi_test_list*)(kmalloc(sizeof(struct pi_test_list), GFP_ATOMIC));
                        tmp_pi_node->task_node=it3->tid;
                        list_add_tail(&(tmp_pi_node->pi_list),&(check_list));
                    }
                    break;
                case SCHED_RT_GRMA:
                    if((it3->tid) && lower_period_tmp(it2->mt->tid->temp_period,it3->tid->temp_period)){
                        it3->tid->temp_period=it2->mt->tid->temp_period;
                        //Append the new task in the check_list
                        tmp_pi_node=(struct pi_test_list*)(kmalloc(sizeof(struct pi_test_list), GFP_ATOMIC));
                        tmp_pi_node->task_node=it3->tid;
                        list_add_tail(&(tmp_pi_node->pi_list),&(check_list));
                    }
                    break;
                default:
                    break;
            }
        }
    }
    //Now, free memory from 'check_list'
    list_for_each_entry_safe(it,it1,&(check_list),pi_list){
        kfree(it);
    }
}

//static void check_pi_backward(struct task_mutex *tmp_tm,int sched_no){
static void check_pi_backward(struct rt_info *tmp_tm,int sched_no){
    //This function checks changes to priority inheritance triggered by rt_info in tmp_tm.
    /**** Rationale behind check_pi ****
     * First, we start with each mutex_task for current task in each required mutex. For each task, 
     * there are 3 sources of priority: 1) base priority for current task. 2) donor priority for 
     * current task if any. 3) Inherited priority of previous task in each required mutex if exists. 
     * Temporal priority of current task is the largest between the 3. All previous tasks to current 
     * task in all conflicting mutexes are added to 'check_list' if they do not already exist in 
     * 'check_list'. The name 'backward' stems from checking priority of current task with successors 
     * in all conflicting mutexes.
     */
    struct list_head check_list;     //used to check priority inheritance
    struct pi_test_list *tmp_pi_node;
    struct pi_test_list *it, *it1;    //Iterators to traverse check_list
    struct task_mutex *it2;     //Iterator to traverse all task_mutex of current task
    struct mutex_task *it3;     //Pointer to successor to it2 in the specified mutex_head
    struct mutex_task *it4;     //Pointer to precedent to it2 in the specified mutex_head
    int task_exist;   //1 if task already exists in 'check_list' starting from current check_list pointer. 0 otherwise.
    task_exist=0;
    INIT_LIST_HEAD(&(check_list));
    tmp_pi_node=(struct pi_test_list*)(kmalloc(sizeof(struct pi_test_list), GFP_ATOMIC));
    tmp_pi_node->task_node=tmp_tm;      //Initial value
    list_add_tail(&(tmp_pi_node->pi_list),&(check_list));
    list_for_each_entry(it, &(check_list), pi_list){
        //Initially, set temporaral priority of current task to max(base priority, donor priority if exist)
        switch(sched_no){
            case SCHED_RT_GEDF:
                if((it->task_node->donor) && earlier_deadline_tmp(it->task_node->donor->temp_deadline,&it->task_node->deadline)){
                    it->task_node->temp_deadline=it->task_node->donor->temp_deadline;
                }
                else{
                    it->task_node->temp_deadline=&it->task_node->deadline;
                }
                break;
            case SCHED_RT_GRMA:
                if((it->task_node->donor) && lower_period_tmp(it->task_node->donor->temp_period,&it->task_node->period)){
                    it->task_node->temp_period=it->task_node->donor->temp_period;
                }
                else{
                    it->task_node->temp_period=&it->task_node->period;
                }
                break;
            default:
                break;
        }
        //Now check if current task should inherit priority from previous task in each required mutex_head
        list_for_each_entry(it2,&(it->task_node->req_res),m_list){
            it3=list_is_last(&it2->mt->fifo_list,&it2->mt->mh->fifo_list)?NULL:list_entry(it2->mt->fifo_list.next,struct mutex_task,fifo_list);        //check if it3 does not point to head before inhereting priority
            it4=list_is_first(&it2->mt->fifo_list,&it2->mt->mh->fifo_list)?NULL:list_entry(it2->mt->fifo_list.prev,struct mutex_task,fifo_list);     //check if it4 does not point to head before adding to check_list. Also it4 should not already exist in 'check_list'
            task_exist=0;
            switch(sched_no){
                case SCHED_RT_GEDF:
                    if((it3) && earlier_deadline_tmp(it3->tid->temp_deadline,it2->mt->tid->temp_deadline)){
                        it2->mt->tid->temp_deadline=it3->tid->temp_deadline;
                    }
                    break;
                case SCHED_RT_GRMA:
                    if((it3) && lower_period_tmp(it3->tid->temp_period,it2->mt->tid->temp_period)){
                        it2->mt->tid->temp_period=it3->tid->temp_period;
                    }
                    break;
                default:
                    break;
            }
            //Check if it4 does not already exist in 'check_list' starting from current pointer in 'check_list'
            it1=it;
            list_for_each_entry_from(it1, &(check_list), pi_list){
                if(it1->task_node==it4->tid){
                    task_exist=1;
                    break;
                }
            }
            if(!task_exist){
                //Add previous task to the current mutex_task to 'check_list'
                tmp_pi_node=(struct pi_test_list*)(kmalloc(sizeof(struct pi_test_list), GFP_ATOMIC));
                tmp_pi_node->task_node=it4->tid;
                list_add_tail(&(tmp_pi_node->pi_list),&(check_list));
            }
        }
    }
    //Now, free memory from 'check_list'
    list_for_each_entry_safe(it,it1,&(check_list),pi_list){
        kfree(it);
    }
}
/************************** SH-END ******************************/

static int init_rt_resource(struct mutex_data __user *mutexreq/** SH-ST **/, int __user *lock_pro/** SH-END **/)
{
    //lock_pro is an interger that specifies which locking protocol is used
    struct process_mutex_list *process = find_by_tgid(current->tgid);
    struct mutex_head *m = kmalloc(sizeof(struct mutex_head), GFP_KERNEL);

    if(!m)
            return -ENOMEM;

    m->mutex = mutexreq;
    m->owner_t = NULL;

    if(!process) {
            process = kmalloc(sizeof(struct process_mutex_list), GFP_KERNEL);

            if(!process) {
                    kfree(m);
                    return -ENOMEM;
            }

            write_lock(&chronos_mutex_list_lock);
            list_add(&process->p_list, &chronos_mutex_list);
            write_unlock(&chronos_mutex_list_lock);

            process->tgid = current->tgid;
            INIT_LIST_HEAD(&process->m_list);
            rwlock_init(&process->lock);
            cmutexstat_inc(processes);
    }

    write_lock(&process->lock);
    list_add(&m->list, &process->m_list);
    write_unlock(&process->lock);

    mutexreq->id = (unsigned long)m - (unsigned long)process;
    m->id = mutexreq->id;
    cmutexstat_inc(locks);
    /********** SH-ST ************/
    switch (*lock_pro){
        case OMLP_PRO:      //OMLP and RNLP take the same initialization
        case RNLP_PRO:
            spin_lock_init(&(m->mutex_lock));
            m->no_tasks=0;
            m->max_no_tasks=1;  //only 1 task can hold mutex at any time except in RNLP token
            INIT_LIST_HEAD(&(m->fifo_list));
            plist_head_init(&(m->pri_list), NULL);
            break;
        default:
            break;
    }	
    /********** SH-END ************/

    return 0;
}

static int destroy_rt_resource(struct mutex_data __user *mutexreq)
{
	int empty;
	struct process_mutex_list *process = find_by_tgid(current->tgid);
	struct mutex_head *m = find_in_process(mutexreq, process);

	if(!m || !process)
		return 1;

	// Remove the mutex_head
	write_lock(&process->lock);
	list_del(&m->list);
	empty = list_empty(&process->m_list);
	write_unlock(&process->lock);
	kfree(m);

	if(empty) {
		write_lock(&chronos_mutex_list_lock);
		list_del(&process->p_list);
		write_unlock(&chronos_mutex_list_lock);
		kfree(process);
		cmutexstat_dec(processes);
	}

	cmutexstat_dec(locks);

	return 0;
}

/* Returning 0 means everything was fine, returning > -1 means we got the lock */
static int request_rt_resource(/** SH-ST **/struct mutex_data __user **mutexreq, int __user *num_mutex, int __user *lock_pro, int __user *num_proc, int __user *sched_no/** SH-END **/)
{
    //lock_pro identifies locking protocol. num_mutex is number of required mutexes. num_mutex is 
    //important to determine end of mutexreq array. num_proc is 
    //number of processors. mutexreq can be a pointer to a an array of requested mutexes. 
    //Even a single mutex should be represented as an array of one pointer
    int c, ret = 0;
    struct rt_info *r = &current->rtinfo;
    /****************** SH-ST ******************/
    int mutex_indx=0;
    int mutex_cnt=(*num_mutex);
    int num_proc_in=*num_proc;
    struct mutex_task* new_task;
    struct mutex_task *mh_first;
    /* Some of the following code is modified from the original chronos_mutex.c to work on array of mutexes per request */

    /* FIXME: This is for reentrant locking. It needs modification because in new locking 
     * protocols, a task may be in a queue of mutex_head but is not its owner. So, to check 
     * reentrance, we should check if task already exists in queues of mutex_head. But we can 
     * neglect this check for the moment as the inputs mutexes are not repeated
     */
    /*
     * //Un-comment this block and fix it if you need to check for re-entrance locking
    int ret_val=0;          //temp variable to check in reentrant locking for array of requested mutexes
    int mutex_cnt=0;        //mutex counter if requesting an array of mutexes
    while(mutex_cnt<(*num_mutex)){
        if(mutexreq[mutex_cnt]->owner != current->pid){
            ret_val++;
        }
        else if(check_task_abort_nohua(r))
            return -EOWNERDEAD;
        mutex_cnt++;
    }
    if(!ret_val){
        return ret_val;   //if 0, then current process is owner of each requested mutex
    }
     */
    
    /* m is modified to array of pointers because multiple mutexes can be requested */
    /* at the same time per request if they are known a priori */
    struct mutex_head* m[mutex_cnt];    //because mutex_cnt-ret_val mutexes are already acquired by current task
    unsigned long flags;

    while(mutex_indx<mutex_cnt){
	struct req_mutex *rm;
        if(!(m[mutex_indx]= find_mutex(mutexreq[mutex_indx], current->tgid))){
            /* If corresponding mutex_head cannot be retreived, then we should erase added
             * information into req_mutex_list.
             */
            struct req_mutex *it1, *it2;
            list_for_each_entry_safe(it1,it2,&(r->req_mutex_list),req_mutex_node){
                list_del(&(it1->req_mutex_node));
                kfree(it1);
            }
            ret -EINVAL;
        }
        (m[mutex_indx]->no_tasks)++;
        rm=(struct req_mutex*)(kmalloc(sizeof(struct req_mutex),GFP_KERNEL));
        rm->mh=m[mutex_indx];
        list_add_tail(&(rm->req_mutex_node),&(r->req_mutex_list));
        mutex_indx++;
    }

    switch (*lock_pro){
            case OMLP_PRO:
                /* OMLP uses group locking. So, there will be only one mutex for each set of 
                 * resources, and there will be only one mutex per request. Lock FIFO and 
                 * Priority queues in omlp, insert task in proper location. Number of requesting  
                 * tasks has already been incremented. In OMLP, we don't need to use req_res neither 
                 * req_mutex_list because each task will be put directly in the corresponding FIFO 
                 * or Priority queues.
                 */
                spin_lock_irqsave(&(m[0]->mutex_lock),flags);
                new_task=(struct mutex_task*)(kmalloc(sizeof(struct mutex_task), GFP_ATOMIC));
                new_task->tid=r;
                new_task->mh=m[0];
                r->rt_status=SUSPENDED;         //default
                if(m[0]->no_tasks<=num_proc_in){
                    //Add the new task to the corresponding FIFO queue
                    list_add_tail(&(new_task->fifo_list),&(m[0]->fifo_list));
                    //If the new task is in head of mutex FIFO, then change its status to NON_PREEMPTIVE
                    if(list_is_first(&(new_task->fifo_list),&(m[0]->fifo_list))){
                        r->rt_status=NON_PREEMPTIVE;
                    }
                }
                else{
                    //Add the new task in the corresponding Priority queue
                    if(*sched_no==SCHED_RT_GEDF){
                        plist_node_init(&(new_task->pr_list), t2l(r->temp_deadline));
                    }
                    else if(*sched_no==SCHED_RT_GRMA){
                        plist_node_init(&(new_task->pr_list), t2l(r->temp_period));
                    }
                    plist_add(&(new_task->pr_list),&(m[0]->pri_list));
                }
                //Inherit priority of new_task if it has higher priority
                mh_first=list_first_entry(&(m[0]->fifo_list),struct mutex_task,fifo_list);
                if(*sched_no==SCHED_RT_GEDF){
                    if(earlier_deadline_tmp(r->temp_deadline,mh_first->tid->temp_deadline)){
                        mh_first->tid->temp_deadline=r->temp_deadline;
                    }
                }
                else if(*sched_no==SCHED_RT_GRMA){
                    if(lower_period_tmp(r->temp_period,mh_first->tid->temp_period)){
                        mh_first->tid->temp_period=r->temp_period;
                    }
                }
                spin_unlock_irqrestore(&(m[0]->mutex_lock),flags);
                force_sched_event(current);
                schedule();
                return 0;
            case RNLP_PRO:
                //In case of R^2DGLP, the only lock to be obtained is the token lock because
                //requesting token lock and registering in each mutex queue is atomic operation
                //under R^2DGLP when all mutexes per request are known a priori
                spin_lock_irqsave(&(rnlp_acc_token_list.mutex_lock),flags);
                if(r->token==1){
                    //current task already has token
                    /* Nothing to be implemented here now because all resources per request are known a priori */
                }
                else{
                    struct mutex_task* new_task=(struct mutex_task*)(kmalloc(sizeof(struct mutex_task), GFP_ATOMIC)); //new_task is used inside token list only
		    int tmp_cnt;
                    new_task->tid=r;
                    new_task->mh=&(rnlp_acc_token_list);
                    r->rt_status=SUSPENDED; //Default
                    //must obtain token first
                    (rnlp_acc_token_list.no_tasks)++;       //Increment number of current tasks accessing a token
                    if(rnlp_acc_token_list.no_tasks<=rnlp_acc_token_list.max_no_tasks){
                        //Tokens are still available
                        r->token=1; //current task gets a token
                        list_add_tail(&(new_task->fifo_list), &(rnlp_acc_token_list.fifo_list));
                        /* Append a new mutex_task to FIFO queue of each required mutex_head */
                        tmp_cnt=0;    //mutex counter
                        while(tmp_cnt<mutex_cnt){
                            //make a new mutex_task for each required mutex_head
                            struct mutex_task* tmp_new_task=(struct mutex_task*)(kmalloc(sizeof(struct mutex_task), GFP_ATOMIC));
			    //make a new task_mutex for current task in current mutex_head
			    struct task_mutex *tmp_task_mutex=(struct task_mutex*)(kmalloc(sizeof(struct task_mutex), GFP_ATOMIC));
                            tmp_new_task->tid=r;
                            tmp_new_task->mh=m[tmp_cnt];
                            //tmp_new_task->pid=new_task->pid;
                            //Append task to each requested resource FIFO queue.
                            list_add_tail(&(tmp_new_task->fifo_list), &(m[tmp_cnt]->fifo_list));
                            //Increment res_cnt if current task is not head of current mutex_head
                            if(!list_is_first(&(tmp_new_task->fifo_list),&(m[tmp_cnt]->fifo_list))){
                                (r->res_cnt)++;
                            }
                            tmp_task_mutex->mt=tmp_new_task;
                            //Append this task_mutex to list of accessed objects by current job
                            list_add_tail(&(tmp_task_mutex->m_list), &(r->req_res));
                            tmp_cnt++;
                        }
                        //If job is in head of each required mutex, then change its state to NON_PREEMPTIVE
                        if(r->res_cnt==0){
                            r->rt_status=NON_PREEMPTIVE;
                        }
                        else{
                            //Check for priority inheritance by new task to previous tasks in required mutexes
                            check_pi_forward(r,*sched_no);
                        }
                    }
                    else{
                        //Append new task to donor list according to its priority
                        //Create a pnode for current task to be added to donors' queue
			struct mutex_task *tmp_min;
			struct mutex_task *tmp_indx;
                        if(*sched_no==SCHED_RT_GEDF){
                            plist_node_init(&(new_task->pr_list), t2l(r->temp_deadline));
                        }
                        else if(*sched_no==SCHED_RT_GRMA){
                            plist_node_init(&(new_task->pr_list), t2l(r->temp_period));
                        }
                        plist_add(&(new_task->pr_list),&(rnlp_acc_token_list.pri_list));
                        //Donate priority of new task to lowest priority task with token
                        tmp_min=list_first_entry(&(rnlp_acc_token_list.fifo_list),struct mutex_task,fifo_list);
                        //Find least priority task holding token
                        list_for_each_entry(tmp_indx,&(rnlp_acc_token_list.fifo_list),fifo_list){
                            if(*sched_no==SCHED_RT_GEDF){
                                if(earlier_deadline_tmp(tmp_indx->tid->temp_deadline,tmp_min->tid->temp_deadline)){
                                    tmp_min=tmp_indx;
                                }
                            }
                            else if(*sched_no==SCHED_RT_GRMA){
                                if(lower_period_tmp(tmp_indx->tid->temp_period,tmp_min->tid->temp_period)){
                                    tmp_min=tmp_indx;
                                }
                            }
                        }
                        //Now, donate priority to the tmp_min task
                        if(*sched_no==SCHED_RT_GEDF){
                            if(earlier_deadline_tmp(r->temp_deadline,tmp_min->tid->temp_deadline)){
                                tmp_min->tid->temp_deadline=r->temp_deadline;
                                tmp_min->tid->donor=r;
                                r->donee=tmp_min->tid;
                                //check if priority should be propagated
                                check_pi_forward((tmp_min->tid),*sched_no);
                            }
                        }
                        else if(*sched_no==SCHED_RT_GRMA){
                            if(lower_period_tmp(r->temp_period,tmp_min->tid->temp_period)){
                                tmp_min->tid->temp_period=r->temp_period;
                                tmp_min->tid->donor=r;
                                r->donee=tmp_min->tid;
                                //check if priority should be propagated
                                check_pi_forward((tmp_min->tid),*sched_no);
                            }
                        }
                    }
                }
                r->requested_resource = m;
                spin_unlock_irqrestore(&(rnlp_acc_token_list.mutex_lock),flags);
                force_sched_event(current);
                schedule();
                return 0;
    }
    /************* SH-END *************/

    /* Notify that we are requesting the resource and call the scheduler */
    r->requested_resource = m;
    force_sched_event(current);
    schedule();

    /* Our request may have been cancelled for some reason */
    /*
    if(r->requested_resource != m)
            return -EOWNERDEAD;
    */

    /* Try to take the resource */
    /*
    if((c = cmpxchg(&(mutexreq->value), 0, 1)) != 0) {
            ret = 1;
            do {
                    if(c == 2 || cmpxchg(&(mutexreq->value), 1, 2) != 1) {
                            futex_wait(&(mutexreq->value), 2); }
            } while((c = cmpxchg(&(mutexreq->value), 0, 2)) != 0);
            cmutexstat_inc(locking_failure);
    } else
            cmutexstat_inc(locking_success);

    mutexreq->owner = current->pid;
    m->owner_t = r;
    r->requested_resource = NULL;
     */

    return ret;
}

static int release_rt_resource(/** SH-ST **/struct mutex_data __user **mutexreq, int __user *num_mutex, int __user *lock_pro, int __user *sched_no/** SH-END **/)
{
    /********** SH-ST ************/
    int mutex_cnt=(*num_mutex);    //Holds number of mutexes to be released
    int mutex_indx=0;   //Mutex index inside mutexreq
    unsigned long flags;        //to store interrupts while holding lock
    struct mutex_task *m_head;  //Pointer to the new head of mutex_head
    struct rt_info *r = &current->rtinfo;
    struct mutex_head *m[mutex_cnt];    //Array of pointers to all requested mutex_heads to be released
    struct mutex_task *mt[mutex_cnt];   //Array of pointer to mutex_task of r in each requested mutex_head to be released
    struct task_mutex *tm_it,*tm_it1;      //Iterator pointers over task_mutexes held by r
    struct req_mutex *rm_release_it1, *rm_release_it2;
    
    /*
    if(!m)
            return -EINVAL;

    if(mutexreq->owner != current->pid)
            return -EACCES;

    mutexreq->owner = 0;
    m->owner_t = NULL;
     */
    
    switch(*lock_pro){
        case OMLP_PRO:
            if(!(m[0]= find_mutex(mutexreq[0], current->tgid))){
                //requested mutex_head does not exist
                return -EINVAL;
            }
            spin_lock_irqsave(&(m[0]->mutex_lock),flags);
            if(list_empty(&(m[0]->fifo_list))){
                return -EACCES;
            }
            else{
                mt[0]=list_first_entry(&(m[0]->fifo_list),struct mutex_task,fifo_list);
            }
            if((mt[0]->tid)!=r){
                //r has not requested mutex_head or not in the head of mutex_head
                return -EACCES;
            }
            //Decrement number of tasks requiring this mutex_head
            (m[0]->no_tasks)--;
            //Remove the old task in mutex_head
            list_del(&mt[0]->fifo_list);
            /* Move the highest priority task in Priority list to tail of mutex FIFO list */
            if(!plist_head_empty(&(m[0]->pri_list))){
                struct mutex_task *tmp_new_mt=plist_first_entry(&(m[0]->pri_list),struct mutex_task,pr_list);
                plist_del(&(tmp_new_mt->pr_list),&(m[0]->pri_list));
                list_add_tail(&(tmp_new_mt->fifo_list), &(m[0]->fifo_list));
            }
            //*)Change the rt_status of the new head of the mutex to NON_PREEMPTIVE
            if(!list_empty(&(m[0]->fifo_list))){
                m_head=list_first_entry(&(m[0]->fifo_list),struct mutex_task,fifo_list);
                m_head->tid->rt_status=NON_PREEMPTIVE;
                /* The new mutex head should inherit priority of the removed task if higher */
                if(*sched_no==SCHED_RT_GEDF){
                    if(earlier_deadline_tmp(r->temp_deadline,m_head->tid->temp_deadline)){
                        m_head->tid->temp_deadline=r->temp_deadline;
                    }
                }
                else if(*sched_no==SCHED_RT_GRMA){
                    if(lower_period_tmp(r->temp_period,m_head->tid->temp_period)){
                        m_head->tid->temp_period=r->temp_period;
                    }
                }
            }
            //*)Free memory space for required mutex_task from each released mutex_head.
            kfree(mt[0]);
            //*)Set temporal priority of r to base priority.
            if(*sched_no==SCHED_RT_GEDF){
                r->temp_deadline=&r->deadline;
            }
            else if(*sched_no==SCHED_RT_GRMA){
                r->temp_period=&r->period;
            }
            //*)Restore real time status of r to NORMAL
            r->rt_status=T_NORMAL;
            spin_unlock_irqrestore(&(m[0]->mutex_lock),flags);
            force_sched_event(current);
            schedule();
            return 0;
        case RNLP_PRO:
            //In case of R^2DGLP, the only lock to be obtained is the token lock
	    spin_lock_irqsave(&(rnlp_acc_token_list.mutex_lock),flags);
            while(mutex_indx<mutex_cnt){
                if(!(m[mutex_indx] = find_mutex(mutexreq[mutex_indx], current->tgid))){
                    //requested mutex_head does not exist
                    return -EINVAL;
                }
                if(!list_empty(&(m[mutex_indx]->fifo_list))){
                    mt[mutex_indx]=list_first_entry(&(m[mutex_indx]->fifo_list),struct mutex_task,fifo_list);
                }
                else{
                    return -EACCES;
                }
                if((mt[mutex_cnt]->tid)!=r){
                    //r has not requested mutex_head or not in the head of mutex_head
                    return -EACCES;
                }
                //Decrement number of tasks requiring this mutex_head
                (m[mutex_indx]->no_tasks)--;
                mutex_indx++;
            }
            //To release specified mutexes, follow the following starred steps:
            mutex_indx=0;
            while(mutex_indx<mutex_cnt){
                //Release mutex_head from list of required mutexes by current task
                list_for_each_entry_safe(rm_release_it1,rm_release_it2,&(r->req_mutex_list),req_mutex_node){
                    if((rm_release_it1->mh)==m[mutex_indx]){
                        list_del(&(rm_release_it1->req_mutex_node));
                        kfree(rm_release_it1);
                        break;
                    }
                }
                //*)Delete r from head of each mutex_head and free memory from that mutex_task.
                list_del(&(mt[mutex_indx]->fifo_list));
                //*)For the next task (if exists) in the mutex_head, decrement number of holding mutexes and change 
                //its real-time status to NON_PREEMPTIVE if it is the head of all its required mutexes
                if(!list_empty(&(m[mutex_indx]->fifo_list))){
                    m_head=list_first_entry(&(m[mutex_indx]->fifo_list),struct mutex_task,fifo_list);
                    (m_head->tid->res_cnt)--;
                    if((m_head->tid->res_cnt)==0){
                        m_head->tid->rt_status=NON_PREEMPTIVE;
                    }
                }
                //*)Delete the corresponding task_mutex from r->req_res and free its memory.
                list_for_each_entry_safe(tm_it,tm_it1,&(r->req_res),m_list){
                    if(tm_it->mt==mt[mutex_indx]){
                        list_del(&tm_it->m_list);
                        kfree(tm_it);
                        break;
                    }
                }
                //*)Free memory space for required mutex_task from each released mutex_head.
                kfree(mt[mutex_indx]);
                mutex_indx++;
            }
            //*)Temporarily set temporal priority of r to base priority.
            if(*sched_no==SCHED_RT_GEDF){
                r->temp_deadline=&r->deadline;
            }
            else if(*sched_no==SCHED_RT_GRMA){
                r->temp_period=&r->period;
            }
            //*)*** IF r RELEASED ALL ITS MUTEXES ***/
            if(mutex_cnt==r->res_cnt){
	    	struct mutex_task *rnlp_fifo_it, *rnlp_fifo_it1;
                //*)Reset 'token' variable in r.
                r->token=0;
                //*)Remove task from fifo_list in rnlp_acc_token_list.
                list_for_each_entry_safe(rnlp_fifo_it,rnlp_fifo_it1,&(rnlp_acc_token_list.fifo_list),fifo_list){
                    if(rnlp_fifo_it->tid==r){
                        list_del(&rnlp_fifo_it->fifo_list);
                        kfree(rnlp_fifo_it);
                        break;
                    }
                }
                //*)Reduce number of tasks in rnlp_acc_token_list.
                (rnlp_acc_token_list.no_tasks)--;
                //*)Restore real time status of r to NORMAL
                r->rt_status=T_NORMAL;
                //*)Reset donor in r, and reset donee in donor.
                r->donor=NULL;
                r->donor->donee=NULL;
                //*)Move the highest priority task in priority list of rnlp_acc_token_list to the tail of
                //fifo list in rnlp_acc_token_list. As the donor for r has priority lower or equal to the
                //moved task from prioriy list in rnlp_acc_token_list to the tail of FIFO of rnlp_acc_token_list, 
                //then donor will have no new donee. Deal with the new task in tail of fifo_list as a new task added to fifo of rnlp_acc_token_list 
                //and required mutexes.
                if(!plist_head_empty(&(rnlp_acc_token_list.pri_list))){
                    struct mutex_task *tmp_new_mt=plist_first_entry(&(rnlp_acc_token_list.pri_list),struct mutex_task,pr_list);
		    struct req_mutex *rm_it1;
                    plist_del(plist_first(&(rnlp_acc_token_list.pri_list)),&(rnlp_acc_token_list.pri_list));
                    tmp_new_mt->tid->token=1;
                    list_add_tail(&(tmp_new_mt->fifo_list), &(rnlp_acc_token_list.fifo_list));
                    list_for_each_entry(rm_it1,&(tmp_new_mt->tid->req_mutex_list),req_mutex_node){
                        struct mutex_task* tmp_new_task=(struct mutex_task*)(kmalloc(sizeof(struct mutex_task), GFP_ATOMIC));
			struct task_mutex *tmp_task_mutex=(struct task_mutex*)(kmalloc(sizeof(struct task_mutex), GFP_ATOMIC));
                        tmp_new_task->tid=tmp_new_mt->tid;
                        tmp_new_task->mh=rm_it1->mh;
                        list_add_tail(&(tmp_new_task->fifo_list), &(rm_it1->mh->fifo_list));
                        if(!list_is_first(&(tmp_new_task->fifo_list),&(rm_it1->mh->fifo_list))){
                            (tmp_new_task->tid->res_cnt)++;
                        }
                        tmp_task_mutex->mt=tmp_new_task;
                        list_add_tail(&tmp_task_mutex->m_list,&(tmp_new_mt->tid->req_res));
                    }
                    if((tmp_new_mt->tid->res_cnt)==0){
                        (tmp_new_mt->tid->rt_status)=NON_PREEMPTIVE;
                    }
                    check_pi_forward((tmp_new_mt->tid),*sched_no);
                }
                //*)(This point is included only as a demonstration, you don't need to implement it) We 
                //don't need to insert a new task to the donors if number of tasks in priority list of 
                //rnlp_acc_token_list is greater than number of tokens because donors are already arranged 
                //according to priority in priority list in rnlp_acc_token_list. As before, this new donor 
                //have the least priority among donors. Since all tasks in fifo_list in rnlp_acc_token_list 
                //have higher or equal priority to the new donor, the new donor will have no donee
            }
            //*)*** IF r STILL HAVE SOME MUTEXES ***/
            else{
                //*)r should check its priority according to remaining mutexes. Apply check_pi_backward to 
                //remaining task_mutexes. The new priority will be maximum between base, donor and remaining mutexes.
		struct mutex_task *tmp_new_mt=plist_first_entry(&(rnlp_acc_token_list.pri_list),struct mutex_task,pr_list);
                check_pi_backward((tmp_new_mt->tid),*sched_no);
            }
                
            spin_unlock_irqrestore(&(rnlp_acc_token_list.mutex_lock),flags);
            force_sched_event(current);
            schedule();
            return 0;
        default:
            break;
                    //Following code is the default one
    }
    /*
    if(cmpxchg(&(mutexreq->value), 1, 0) == 2) {
            mutexreq->value = 0;
            futex_wake(&(mutexreq->value));
    }

    force_sched_event(current);
    schedule();
     */
/******** SH-END *********/

    return 0;
    
}
#endif
/*
SYSCALL_DEFINE2(do_chronos_mutex, struct mutex_data __user, *mutexreq, int, op)
{
*/
	/* We have to check this every time, so just do it here */
/*
	if(!mutexreq || !access_ok(VERIFY_WRITE, mutexreq, sizeof(*mutexreq)))
		return -EFAULT;

	switch(op) {
#ifdef CONFIG_CHRONOS
		case CHRONOS_MUTEX_REQUEST:
			return request_rt_resource(mutexreq);
		case CHRONOS_MUTEX_RELEASE:
			return release_rt_resource(mutexreq);
		case CHRONOS_MUTEX_INIT:
			return init_rt_resource(mutexreq);
		case CHRONOS_MUTEX_DESTROY:
			return destroy_rt_resource(mutexreq);
#endif
		default:
			return -EINVAL;
	}
}
*/

//The previous SYSCALL_DEFINE2 is modified to the following SYSCALL_DEFINE6 to accommodate for the additional parameters with the specific locking protocol
/************ SH-ST **************/
SYSCALL_DEFINE6(do_chronos_mutex, struct mutex_data __user, **mutexreq, int, op, int __user, *num_mutex, int __user, *lock_pro, int __user, *num_proc, int __user, *sched_no)
{
	/* We have to check this every time, so just do it here */
    int mutex_indx=0;
    int mutex_cnt=(*num_mutex);
    while(mutex_indx<mutex_cnt){
	if(!(mutexreq[mutex_indx]) || !access_ok(VERIFY_WRITE, mutexreq[mutex_indx], sizeof(*(mutexreq[mutex_indx]))))
		return -EFAULT;
        mutex_indx++;
    }

	switch(op) {
#ifdef CONFIG_CHRONOS
		case CHRONOS_MUTEX_REQUEST:
			return request_rt_resource(mutexreq,num_mutex,lock_pro,num_proc,sched_no);
		case CHRONOS_MUTEX_RELEASE:
			return release_rt_resource(mutexreq,num_mutex,lock_pro,sched_no);
		case CHRONOS_MUTEX_INIT:
			return init_rt_resource(mutexreq[0],lock_pro);
		case CHRONOS_MUTEX_DESTROY:
			return destroy_rt_resource(mutexreq[0]);
#endif
		default:
			return -EINVAL;
	}
}
/************ SH-END *************/
