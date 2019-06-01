Design Document for Project 1: Threads
======================================

## Group Members

* George Ong <gkong88@gmail.com>


1) Data structures and functions

Initialization

Data Structures:
struct sleeping_thread
{
    int64_t wake_up_time; // tick time that thread wants to get woken up
    tid_t tid; // thread id
    struct list_elem elem; // list_elem so it can be stored in linked list data structure
};

Functions:

MODIFY: DONE
timer.c "header"
struct list sleeping_list;

MODIFY: DONE
timer_init (void) {
    /*
    add initialization code for global variable sleeping_list
    */
    list_init (&sleeping_list);
}

MODIFY: DONE
timer_sleep (int64_t ticks) {
    /*
    - remove busy wait while loop
    + add thread_block()
    +...
    struct sleeping_thread thread;
    sleeping_thread_init(&thread);
    thread.wake_up_time = //CURRENT TIME + WAIT TIME
    thread.tid = //GET CURRENT THREAD
    list_push_back (&sleeping_list, &thread->elem);
    */
}

MODIFY: DONE
timer_interrupt() {
    + wake_sleeping_threads();
}


T2)

Scheduler: respect priorities

// consider your implementation of MLFQ. if it's not a list, you'll need a different suite of functions
// for your POP FRONT

Modify Function:
"next_thread_to_run(void)"
- change list_pop_front to arg_max_priority(&ready_list)

New Function:
static struct thread *arg_max_priority(struct list *ready_list) {
}
int 


Primitives: Locks, CVs, Semaphores: Respect priorities AND implement priority donation (do some checking on waiting)
Locks / Semaphores / CVs:
MODIFY sema_up:
replace list_pop_front w/ arg_max_priority

MODIFY lock_try_acquire:
if (success == false) {
    if current_priority > holder_priority:
        old_priority = holder_priority
        holder_priority = current_priorty
}
// somewhere later, TODO:
holder_priority = old_priority // restore its default priority

T3)

2) Algorithms

3) Synchronization

4) Rationale
