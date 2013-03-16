/* include/linux/chronos_types.h
 *
 * All #defines and data structures used in ChronOS
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#ifndef _CHRONOS_TYPES_H
#define _CHRONOS_TYPES_H

#include <linux/mcslock.h>
#include <linux/list.h>
#include <linux/time.h>
#include <asm/atomic.h>
/********** SH-ST **********/
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/plist.h>
/********** SH-END *********/

/* Task Flags - all others to be used in algorithm-specific ways */
#define TASK_FLAG_NONE			0x00
#define TASK_FLAG_MASK			0xFF
#define TASK_FLAG_ABORTED		0x01
#define TASK_FLAG_HUA			0x02
#define TASK_FLAG_SCHEDULED		0x04
#define TASK_FLAG_DEADLOCKED		0x08
#define TASK_FLAG_INSERT_GLOBAL		0x80

/* Task flag management */
#define task_set_flag(r, f)	((r)->flags |= TASK_FLAG_##f)
#define task_clear_flag(r, f)	((r)->flags &= ~TASK_FLAG_##f)
#define task_and_flag(r, f)	((r)->flags &= TASK_FLAG_##f)
#define task_check_flag(r, f)	((r)->flags & TASK_FLAG_##f)
#define task_init_flags(r)	((r)->flags = TASK_FLAG_NONE)

/* Masks for getting the scheduler from userspace
 * 1 bit - global
 * 23 bits - scheduler number
 * 8 bits - flags
 */
#define SCHED_GLOBAL_MASK		0x80
#define SCHED_NUM_MASK			0x7F
#define SCHED_FLAGS_MASK		0xFF

/* Scheduler - behaviors for each flag may or may not be defined */
#define SCHED_RT_FIFO 			0x00
#define SCHED_RT_RMA 			0x01
#define SCHED_RT_EDF 			0x02
#define SCHED_RT_HVDF			0x03
#define SCHED_RT_LBESA			0x04
#define SCHED_RT_DASA_ND		0x05
#define SCHED_RT_DASA			0x06
#define SCHED_RT_FIFO_RA		0x07
#define SCHED_RT_GFIFO			0x80
#define SCHED_RT_GRMA			0x81
#define SCHED_RT_GEDF			0x82
#define SCHED_RT_GNP_EDF		0x83
#define SCHED_RT_GHVDF			0x84
#define SCHED_RT_GMUA			0x85
#define SCHED_RT_GGUA			0x86
#define SCHED_RT_NGGUA			0x87
#define SCHED_RT_GNP_HVDF		0x88

/* Scheduling Flags */
/* PI == Priority Inheritance
 * HUA == HUA style abort handlers
 * ABORT_IDLE == abort the task only when the system is idle
 * NO_DEADLOCKS == with deadlock prevention
 *
 * Also, 0x*0 is left open for future flags.
 */
#define SCHED_FLAG_NONE			0x00
#define SCHED_FLAG_MASK			0xFF
#define SCHED_FLAG_HUA			0x01
#define SCHED_FLAG_PI			0x02
#define SCHED_FLAG_NO_DEADLOCKS		0x04

/* Array indices into rt_info.task_list[] */
#define LOCAL_LIST			0
#define GLOBAL_LIST			1
#define SCHED_LIST1			2
#define SCHED_LIST2			3
#define SCHED_LIST3			4
#define SCHED_LIST4			5

/* Number of lists for use by scheduling algorithms. Corresponds to the
 * number of SCHED_LISTx's there are above. */
#define SCHED_LISTS			4

/* Sorting Keys */
#define SORT_KEY_NONE			0
#define SORT_KEY_DEADLINE		1
#define SORT_KEY_PERIOD			2
#define SORT_KEY_LVD			3
#define SORT_KEY_GVD			4
/***************** SH-ST *******************/
#define SORT_KEY_TDEADLINE		5
#define SORT_KEY_TPERIOD                6
/***************** SH-END *******************/


/* Syscall multiplexing flags */
#define RT_SEG_BEGIN			0
#define RT_SEG_END			1
#define RT_SEG_ADD_ABORT		2

/* ChronOS mutex definitions */
#define CHRONOS_MUTEX_REQUEST		0
#define CHRONOS_MUTEX_RELEASE		1
#define CHRONOS_MUTEX_INIT		2
#define CHRONOS_MUTEX_DESTROY		3
/************** SH-ST **************/
/* Locking protocol definitions */
#define OMLP_PRO			0
#define RNLP_PRO			1
/************** SH-END **************/

/* States for must_block (used for STW scheduling) */

/* This CPU has inserted or removed a task, so
 * it must schedule and it cannot be forced to
 * block. */
#define BLOCK_FLAG_CANNOT_FORCE_BLOCK   0

/* This CPU has finished with pick_next_task(), so its
 * must_block flag has been cleared. */
#define BLOCK_FLAG_UNSET                1

/* Another CPU has performed a global reschedule, so
 * this CPU will wait until that CPU has released 
 * the global scheduling lock and then continue execution. */
#define BLOCK_FLAG_MUST_BLOCK           2

/* Struct for a owner-tracking futex
 * USERSPACE SHARED
 */
struct mutex_data {
	u32 value;
	int owner;
	unsigned long id;
        /************** SH-ST *****************/
        void* res;      //pointer to array of resources protected by current mutex. It's included
                        //just as a precaution. It may not be needed if resources are assigned to 
                        //mutexes in application-level, not kernel level
        /************** SH-END *****************/
};

/*********** SH-ST ****************/
struct mutex_task{
    //It points to a node in a list of tasks waiting for a specific mutex. A task can be in either 
    //FIFO or PRIORITY lists of current mutex but not both
    struct rt_info *tid;	//current job
    struct mutex_head *mh;      //pointer to mutex_head
    struct list_head fifo_list;     //FIFO list containing current task
    struct plist_node pr_list;       //Priority list containing current task
};

struct task_mutex{
    //It points to a node of a list of mutexes requested by a specific task
    struct mutex_task *mt;       //pointer to one of required mutexe_task
    struct list_head m_list;    //neighbours to other required mutex_task
};

struct req_mutex{
    //Points to required mutexes by current task. It's similar to task_mutex except that that task has
    //not made any mutex_task yet for the required mutex_head
    struct mutex_head *mh;
    struct list_head req_mutex_node;
};

struct pi_test_list{
    //Used to check for priority inheritance
    struct rt_info *task_node;       //pointer to req_res in a specific rt_info
    struct list_head pi_list;
};
/*********** SH-END ****************/

struct mutex_head {
	struct list_head list;
	struct rt_info *owner_t;
	struct mutex_data *mutex;
	unsigned long id;
	/************ SH-ST *************/
        struct list_head fifo_list;	//List of tasks for current mutex organized in FIFO
	struct plist_head pri_list;	//List of tasks for current mutex organized according to priority
	int no_tasks;			//Current number of tasks accessing or trying to access 
                                        //current mutex.
        int max_no_tasks;               //Maximum number of tasks that can concurrently access this 
                                        //mutex (mutex will be a semaphore)
	spinlock_t mutex_lock;	//Lock to protect any operation done on this mutex or semaphore
	/************ SH-END *************/
};

/******************** SH-ST ***********************/
static struct mutex_head rnlp_acc_token_list;  //Token in RNLP as defined by I-KGLP (latter known as R^2DGLP)
                                        //in rnlp_acc_token_list, fifo_list contains tasks that can concurrently hold the token
                                        //pri_list contains donors arranged by priority

static inline void rnlp_token_init(int num_tokens){
    //Initilizes arrays of concurrent access to tokens
    rnlp_acc_token_list.max_no_tasks=num_tokens;
    rnlp_acc_token_list.no_tasks=0;
    INIT_LIST_HEAD(&(rnlp_acc_token_list.fifo_list));
    plist_head_init(&(rnlp_acc_token_list.pri_list), NULL);
    spin_lock_init(&(rnlp_acc_token_list.mutex_lock));
}
/******************** SH-END ***********************/

struct abort_info {
	struct timespec deadline;
	unsigned long exec_time;
	int max_util;
};

/* Struct for passing parameters down to kernel
 * USERSPACE SHARED
 */
struct rt_data {
	int tid;
	int prio;
	unsigned long exec_time;
	unsigned int max_util;
	struct timespec *deadline;
	struct timespec *period;
};

/* Structure used by x-GUA class of algos for the DAG */
struct rt_graph {
	struct timespec agg_left;
	unsigned long 	agg_util;
	long in_degree;
	long out_degree;
	struct rt_info *neighbor_list;
	struct rt_info *next_neighbor;
	struct rt_info *parent;
	struct rt_info *depchain;
};

/* Structure attached to struct task_struct
 * Order everything by how often it is used, that way the most common parts
 * reside in the same cacheline */
struct rt_info {
	/* Task state information */
	unsigned char flags;
	int cpu;

	/* LOCAL, GLOBAL lists (used by ChronOS internally), and
	 * SCHED_LISTS additional scheduler-managed lists */
	struct list_head task_list[SCHED_LISTS + 2];

	/* Real-Time information */
	struct timespec deadline;		/* monotonic time */
	struct timespec *temp_deadline;		/* monotonic time */
	struct timespec period;			/* relative time */
	struct timespec left;			/* relative time */
	unsigned long exec_time;		/* WCET, us */
	unsigned int max_util;
	long local_ivd;
	long global_ivd;
	unsigned int seg_start_us;
	/******** SH-ST **********/
	struct timespec *temp_period;		/* relative time */
	/******** SH-END *********/

	/* Lock information */
	struct rt_info *dep;
	/****** SH-ST *********/
        struct mutex_head **requested_resource;  //Recently defined to array of mutex_heads
        enum rt_stat_op {
            B_NORMAL,T_NORMAL,SUSPENDED,SPINNING,NON_PREEMPTIVE    //usually spinning is the same as non_preemptive, but it is included for generality
                                                                /* B_NORMAL is BELOW NORMAL. It is of lower priority than any other NORMAL real time task 
                                                                 * B_NORMAL is of theoritical use (nothing uses it till now) */
        }rt_status;
        int token;      //0 (default) if current task holds NO token. 1 otherwise. Usefull in RNLP
        int res_cnt;    //0 (default) if current task is in head of each resource queue it needs
                        //incremented for each resource with preceding tasks in resource queue
        //struct task_mutex req_res;       //list of task_mutex of current task in each current required mutex.
        struct list_head req_res;       //list of task_mutex of current task in each current required mutex.
        struct rt_info *donor;  //pointer to rt_task donating priority to current task. Default is NULL. Used in RNLP
        struct rt_info *donee;  //pointer to rt_task donated priority by current task. Default is NULL. Used in RNLP
	struct list_head req_mutex_list;        //list of required mutexes. It differs from req_res in
                                        //that req_res points to already allocated task_mutex of current
                                        //task in specified mutex_heads, but this may not be the case
                                        //with req_mutex_list. Besides, req_mutex_list points to mutex_head 
                                        //not task_mutex
        /****** SH-END ********/

	/* DAG used by x-GUA class of algorithms */
	struct rt_graph graph;

	/* Abort information */
	struct abort_info abortinfo;
};

struct global_sched_domain {
	/* The global scheduler */
	struct rt_sched_global *scheduler;
	/* The global task list */
	struct list_head global_task_list;
	/* The CPUs in this domain */
	cpumask_t global_sched_mask;
	/* Global scheduling priority in this domain */
	int prio;
	/* Task list lock */
	raw_spinlock_t global_task_list_lock;
	/* Scheduling lock */
	mcs_lock_t global_sched_lock;
	/* Timestamp of the global queue */
	unsigned int queue_stamp;
	/* Current task count */
	atomic_t tasks;
	/* Global domain list - This is the least used item, so put it at the
	 * end so that it will be the thing sticking over the end of the 
	 * cacheline on x86_64 platforms - possibly not an issue
	 * because of prefetching */
	struct list_head list;
} __attribute__ ((__aligned__(SMP_CACHE_BYTES)));

struct rt_sched_arch {
	int (*arch_init) (struct global_sched_domain *g, int block);
	void (*arch_release) (struct global_sched_domain *g);
	void (*map_tasks) (struct rt_info *head, struct global_sched_domain *g);
};

struct sched_base {
	struct list_head list;
	/* Scheduler Name */
	char *name;
	/* Scheduling number and flags */
	unsigned int id;
	/* Sort key for the global list */
	unsigned int sort_key;
	/* The mask of CPUs this scheduler is active on */
	cpumask_t active_mask;
};

struct rt_sched_local {
	/* Base information */
	struct sched_base base;
	/* Flags - currently not needed for any globals */
	unsigned int flags; // should maybe be per-processor?
	/* Scheduling function */
	struct rt_info* (*schedule) (struct list_head *head, int flags);
};

struct rt_sched_global {
	/* Base information */
	struct sched_base base;
	/* Scheduling functions */
	struct rt_info* (*schedule) (struct list_head *head, struct global_sched_domain *g);
	struct rt_info* (*preschedule) (struct list_head *head);
	struct rt_sched_arch *arch;
	/* The local scheduler to be used with this global */
	int local;
};
#endif

