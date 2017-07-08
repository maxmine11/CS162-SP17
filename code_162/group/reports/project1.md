Final Report for Project 1: Threads
===================================
This report is in 3 parts per task:<br/>
1. task implemenattion summary:<br/>
2. changes since initial design:<br/>
3. group member reflections and recommendations for the next project:<br/>

## Task 1: Effient Alarm Clock
**Summary:**<br />
No signficant changes form design document. Current thread saves the current timer ticks then turns off interrupts. The thread then sets its wake-up time and inserts itself into a sorted sleeping queue. Finally current thread blocks and then restores inturrupts. Disabling of interrupts last through wake-up time computation, insertion into sleep queue and transition to blocking status. This ensures correct implemetation.

Sleep queue is FIFO sorted based on `wake_up` attribute of the threads inside.

At every tick, a check for threads that have waited their time is performed. Threads that pass this check are moved into the scheduler's ready queue. Their status changes from blocked to ready.

**Changes since Design Doc:**<br />
1) We end up checking for threads that have completed their wake-up time in `timer_tick` instead of `thread_tick`. Checking in timer_interrupt proved better because a "fully-slept" thread would be able to be moved to the ready queue with minimal lag and have a chance to run.<br />
2) We had considered using a priority queue (for log(n) insertion/deletion) but ended up using and already implemented linked list data structure. This linked list data structure conviniently supported sorting according to an auxillary, which in our case was the wake-up time. This saved us time implementing a priority queue data structure and allowed code re-use.<br />

## Task 2: Priority Scheduler
**Summary:**<br />
Our implementation of the priority scheduler chooses the next thread to run based on its highest effective priority. The thread with highest effective priority is scheduled to run. Took a lot of feedback from Josh's recommendations to avoid pitfalls.


**Changes since Design Doc:**<br />

1. Most significant change was storing the effective priority of all threads as opposed to calculating the effective priority on the fly when the value was required. <br />
2. Addition of a new function `sema_with_greatest_waiter` that only returns true if a given semaphore has a waiting thread with the highest priority. This function was essential for unblocking the highest priority thread when a resource had been released or when the current wants to signal a condition variable. <br />

3. `thread_set_effective_priority` calculates the effective priority for the current thread.  It takes into account nested priority donation by checking the effective priority of the threads that are waiting for a lock that the current thread owns.<br />

4. Priority donation was implemented in `sema_down` instead of `lock_acquire` in order to reuse the sema's waiter list and not use the lock's waiters list. This avoids redundancy because the lock's waiters list of threads would have been the same as the sema's waiters list of threads.<br />
5. Got the max from ready list instead of maintaining a sorted ready list.<br />

## Task 3:
**Summary:**<br />
Our implementation allowed the scheduler to select the next thread based on the priority scheduling algorithm defined in the spec. 

**Changes since Design Document:**<br />

1. Use of 1 queue instead of 64 queues. Our implementation involves one single queue that gives the behavior of having multiple queues. This improves work sanity in dealing with one single queue.<br />

2.  Use of a linked list with an auxillary function that allows popping of highest priority thread when scheduling.<br />

3. Since `recent_cpu` is a moving average, it is calculated inside of `thread_tick` for the running thread.<br />

4. `init_MLFQS` sets correct static variables for the mslfqs scheduler<br />
5. For mlfqs, `nice` and `recent_cpu` thread attributes are set to 0 in the initialize thread function.<br />
6. Our `update_load_avg` checks excludes the `idle_thread` from the specified formula. Instead, `idle_thread` formula computes the size of the ready list of threads as the updated load average variable.<br />
7. Added functions to update the `load_avg`, `recent_cpu`, and the MLFQS priority of a thread. Also reused effective priority from task 2 to keep things simple. <br />

### Group Member Reflections and Recommendations:
**Emilio Aurea**<br />

1. What did I do?<br />
I completed the design and implementation of task 2. I also edited the design and implemented task 3. 

2. What went well?<br />
Task 2 went well because of the solid design doc. The hardest part was figuring out how to do things in Pintos. Overall, it was interesting and fun.

3. What could be improved?<br />
I was stuck debugging on Task 3 because I was relying on a working implementation of task 1 (which was not yet completed). This was frustating because I felt like I was doing all of the work and even went so far as to implement a non-working version of task 1. Once task 1 was implemented correctly, task 3 worked perfectly.
Overall, I wish the work was distributed more appropriately. I feel that I put in a substantial amount of work into the design doc and the code and I wish I could have discussed some of my ideas with my teammates instead of just delegating.

**Samuel Mbaabu**<br />

1. What did I do?<br />
I worked on the design document for task 3.
I compiled the final report for the project.
I helped in debugging part 3.

2. What went well?<br />

3. What could be improved?<br />
Better group communication. 
More group frequent sessions.

**Juan Castillo**<br />

1. What did I do?<br />
a.	I designed the document for task 1 (efficient alarm clock)  along with Muluken for this part of the project. That includes designing the data structures that were used, the function the algorithms complexity and the synchronization.<br />
b.	I collaborated with Muluken in the implementation for the solution of the efficient alarm clock for the scheduler. That included the writing of the code; the testing of the code and the commenting needed to describe the job of the functions.<br />

2.	What went well?<br />
a.	Task 1 went well thanks to the decision of using a sleeping queue for keeping the block threads. This gave the green light for the used, with a little of extra code , of other already implemented functions in pintos “list.h” header. Some of these functions were for example, the “list_insert_ordered” to keep our block threads sorted by the shortest time left to wake up,  “thread_block”  and  “thread_unblock” to block and unblock threads, and functions to traverse through the queue and remove or pop from it as well. The use of these functions gave us a much narrower window to commit mistakes.<br />
b.	The initial design was also a great accomplishment since it was a great guidance to check our ideas and keep track of the steps that still needed to be implemented. <br />

3.	What could be improved?<br />
a.	For task 1 the sleeping queue could be implemented a a binary heap to improved the runtime and keep it sorted at all times and spend less time sorting the n threads O(nlogn) every time a new thread gets inserted. This will improve the insertion runtime to O(logn) and the removal time will still be constant.<br />

**Muluken Taye**<br />

1. What did I do?<br />
Collaborated with Juan to work on designing the document for task one, efficient alarm clock. Different data structures that could speed up putting running threads into sleep and waking them up in time were considered. We chose to implement the list data structure along with functions that ease ordered insertion and removal of elements.

  Worked with Juan coding and debugging the efficient alarm clock algorithm that we crafted during the design phase. I helped in adding functions that improved operation on list and made sure the tests passed.

2.	What went well?<br />
Removing the sleeping thread and putting them into a ready queue at the required time tick was easier. This is because of synchronized call by timer_interrupt() and implementation of thread_unblock() that automatically inserts them into ready queue after unblocking a thread.

  Implementation of ordered sleeping thread queue was great. It made removal of sleeping thread faster, this shortens the time elapsed during timmer_interrupt() call. This is a required feature since we don’t want interrupts disabled for a significant period of time in order to use the CPU efficiently.

3.	What could be improved?<br />
Debugging required a lot of effort than implementing the actual code. It was harder to figure out what went wrong when the code failed the tests. We could have used GDB debugger.

  As Juan mentioned above, min-heap could be used to increase insertion of threads in an ordered queue from linear run time to logarithmic run time but this requires significant amount of work and is less reliable than the existing data structure.
