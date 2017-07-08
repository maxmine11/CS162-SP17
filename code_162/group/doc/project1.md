Design Document for Project 1: Threads
======================================

## Group Members

* Emilio Aurea <emilioaurea@berkeley.edu>
* Samuel Mbaabu <samuel.karani@berkeley.edu>
* Juan Castillo <jp.castillo@berkeley.edu>
* Muluken Taye <muluken@berkeley.edu>

## Task 1: Efficient Alarm Clock

### Data Structures and Functions
```C 
struct list SLEEP_QUEUE;

struct slept_thread {
    struct *thread;
    int64_t wake_up_time;
};

void timer_sleep (int64_t ticks) {

    ASSERT (intr_get_level () == INTR_ON);
    disable_interrupt(thread);
    compute wake_up time;
    block_thread;
    insert into sorted SLEEP_QUEUE;
}
```
SLEEP_QUEUE will be a global variable inside thread.c source code. It is going to be a sorted queue that stores threads that have been set to sleep by timer_sleep(). The data stored will be the struct above, which is formed from thread and wake_up_time.
```C
int64_t wake_up_time() {
    ret = current_tick + ticks
    return ret
}
```
wake_up_time() goes inside timer_sleep() and computes wake_up_time before adding the calling thread to SLEEPQUEUE. We disable interrupts and block the thread right before calling this function and placing the calling thread into the queue.

```C
void check_queue(struct list *list) {
    while list_begin(list).wake_up_time < current_time {
        pop min_slept_thread
        unblock min_slept_thread//unblcks & puts it in ready queue
    }
}
```
The purpose of this function is to wake up the slept threads that have been inside SLEEP_QUEUE long enough. It iterates through the queue until if finds a thread that requires more time to get unblocked. 

This function will be placed in thread_tick() because we want it to be triggered by timer_interrupt().

### Algorithms

A thread calls timer_sleep() with the number of ticks it needs to stay asleep as an argument. It disables interrupts and blocks current thread by calling thread_block(). Then the function computes a wake up time by calling wake_up_time() puts the thread with its wake-up time inside the sorted queue.

The system calls timer_interrupt() at every tick, which in turn calls thread_tick(). This is when we need to check if there are threads in the SLEEP_QUEUE that needs to wake up. In order to perform this task, we add check_queue() inside thread_tick() method that iterates through the queue and checks the wake-up time.

### Synchronization
#### - Disabling Interrupts:
  
Once timer_sleep() is called by a thread, it's necessary to disable the interupt from the calling thread by using the function intr_disable(). If interrupts are on, as they normally are, then the running thread may be preempted by another at any time, whether between two C statements or even within the execution of one. We also try to disable the thread interruption in the least amount of code possible to avoid losing important things such as timer ticks or input events.

#### - Timer Interrupt:
We use timer_interrupt() to check for when to wake up a thread put in the sleeping queque. The timer_interrupt() to relate to the thread_tick that will determine the time at which the threads in the SLEEP_QUEUE will be woken up.
### Rationale

We have considered using minimum priority heap data structure to keep track of sleeping threads. However, using the data structure we could built would be prone to errors that would probably compromise the system. So we decided to stick to the provided linked list data structure.

Both priority queue and sorted linked list have a constant run time of O(1) for removing min. But priority queue would be faster for insertion operation, log(n), as opposed to insertion into a sorted linked list that takes a run time of O(n). The space complexity for either of the data structures is more or less the same.

Adding additonal features for the priority queue will require more code than the linked list since we already have many features that are already implemented for the later.

## Task 2: Priority Scheduler

### Data Structures and Functions
Modifications to Thread struct:
```C
Lock *wait_for_lock; /* Keep track of which lock thread is waiting for */
List locks_owned; /* Which locks we hold */
struct list_elem wait_elem /* To put into waiting_threads list */
int effective_priority; /* Store actual priority */
```
Modifications to Lock struct:
```C
struct list_elem elem; /* To put in locks_owned list */
struct list waiting_threads; /* Which threads waiting on this lock */
```
Other functions that will be modified:
```C
static struct thread *next_thread_to_run (void);
int thread_get_priority (void);
void thread_set_priority (int);
void sema_up (struct semaphore *);
void recursive_donation (struct thread *owner, struct thread *donor);
bool greater_priority(const struct list_elem *a,
                      const struct list_elem *b,
                      void *aux);
```
### Algorithms
#### - Choosing the next thread to run
Get the thread with the highest effective priority off of the ready queue. Use max to find it in O(n) time. Use new function: greater_priority to calculate which thread has greater effective priority.
#### - Acquiring a Lock
If a lock has no owner, then the thread trying to aquire  it should be the highest priority thread according to how it was popped off the queue. Thus, we will grant it the lock and update the lock's owner and the list of locks this thread owns.

Else if a lock has an owner, then the thread trying to aquire the lock will turn off interrupts and we will add thread to the lock's waiting_threads. We will also recursively update the effective priority held by the upstream of threads we are waiting on (recursive donation.) Then we call sema down. After we return, we re-enable interrupts and acquire the lock.
#### - Releasing a Lock
Disable interrupts. Remove the lock from thread's owned locks. Remove thread from lock's owner. Recompute effective priority. Continue with sema-up (this will unblock the thread).Re-enable interrupts. Then yield. If thread has highest priority, it will be scheduled again.
#### - Computing the effective priority
The effective priority will be computing the following formula:
```
max (priority, max (t.effective_priority for t in
                    lock.waiting_threads for lock in
                    locks_owned)); 
```
#### - Priority scheduling for locks
Handled in sema up.
#### - Priority scheduling for semaphores
Handled in sema up.
#### - Priority scheduling for condition variables
Handled in sema up.
#### - Changing thread’s priority
A thread will keep track of its base priority and effective priority. Changing its priority will override the base prioority and update effective priority by choosing the max of the two. Because a thread can only update its priority when it's running, then there is no need to recursively update other priorities.
### Synchronization
Because we are disabling interrupts for critical sections, then we will not have race conditions. However, it is possible to miss interrupts which could cause some issues. 
### Rationale
Because we have a handful of threads, it is not as bad to disable interrupts. However, if we were to have hundreds or thousands of threads, then disabling interrupts would not be the solution.

We also considered implementing critical sections with locks instead of disabling interrupts. This option is still being considered depending on the trade-off between the time spent with interrupts disabled O(n).


## Task 3:  Multi-level Feedback Queue Scheduler (MLFQS)

### Functions

#### - GLOBAL VARIABLES:

```
NICE_MAX = 20
NICE_MIN = -20
PRI_MAX = 63
PRI_MIN = 64
RECENT_CPU_TIME
LOAD_AVG
```

This function will update global variables RECENT_CPU_TIME and LOAD_AVG at each tick cycle. Calculates thread priority to determine which queue a thread will be put on.

Returns queue_num that thread *t should be placed on


``` 
int mlfqs(struct thread *t, int priority):

```
#### - Calculating thread priority:

```
priority = PRI_MAX − (recent_cpu/4) − (nice × 2)

queue_num = priority % 4

This will determine what queue the thread will be placed on
```

#### - Calculating recent CPU time:
```
recent_cpu = (2 × load_avg)/(2 × load_avg + 1) × recent_cpu + nice
```
#### - Calculating load avg:
```
load_avg = (59/60) × load_avg + (1/60) × ready_threads
```
### Data Structures
### - 64 (FIFO) Queues implemented using linked lists.
API:
```
insert
dequeue
peek
```

### Algorithm
```loop
    check for new thread in top-level queue
    add new process to PRI_MAX FIFO queue
    dequeue thread and allocate stack frame
    wait for time quantum to elapse
    check if thread uses up all quantum time:
        if time quantum used up, add thread to end of next lower level queue
        else 
            check if thread relinquished process voluntarily
                if relinquished voluntary, PRI_MAX remains the same
                else PRI_MAX -= 1 and add to next lower level queue
    until proces completes and reach base level queue
        if base level, loop in round robin till no threads left in lowest level queue
        if no ready thread, run kernel idle thread(s).
```

### Synchronization
### Rationale

1. 64 FIFO queues.
1. MLFQS algorithm determines which queue a thread will be placed on (out of the 64 queues) based on the threads priority.
2. Threads with I/0 maintain high priorities
3. Algorithm assumes that new processes will be  short
4. 
## Design Document Additional Questions

#### 1. 
This test will have 3 threads: A, B, C with values 5, 10, 15 respectively. Thread A will acquire a semaphore with value 1. Then, thread B will attempt to acquire the semaphore, but it will fail. 
#### 2.   
The table below showing the scheduling decision   and the recent_cpu and priority values for each     thread after each given number of timer ticks. R(A) and P(A) denote the recent_cpu and priority values of thread A, for brevity.

timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          | 0     |   0   |  0    |  0    |   1   |   2   |P(c)
 4          |  4    |   4   |   4   |      |      |      |
 8          |   8   |   8   |   8   |      |      |      |
12          |   12   |  12    |  12    |      |      |      |
16          |  16    |  16     |  16    |      |      |      |
20          | 20     |   20  |   20   |      |      |      |
24          |  24    |   24   |  24    |      |      |      |
28          |  28    |   28   |  28    |      |      |      |
32          |  32    |   32   |   32   |      |      |      |
36          |  36    |   36   |  36    |      |      |      |