Final Report for Part 1: Threads
===================================
This report is in 3 parts per task:
1. task implentation summary
1. changes since initial design
1. group member reflections and recommendations for the next project


## Task 1: Effient Alarm Clock
**Summary:**
No signficant changes form design document. Current thread saves the current timer ticks then turns off interrupts. The thread then sets its wake-up time and inserts itself into a sorted sleeping queue. Finally current thread blocks and then restores inturrupts. Disabling of interrupts last through wake-up time computation, insertion into sleep queue and transition to blocking status. This ensures correct implemetation.

Sleep queue is FIFO sorted based on wake-up attribute of the threads inside.

At every tick, a check for threads that have waited their time is performed. Threads that pass this check are moved into the scheduler's ready queue. Their status changes from blocked to ready.

**Changes since Design Doc:**
1) We end up checking for threads that have completed their wake-up time in timer_tick instead of thread_tick. Checking in timer_interrupt proved better because a "fully-slept" thread would be able to be moved to the ready queue with minimal lag and have a chance to run.
2) We had considered using a priority queue (for log(n) insertion/deletion) but ended up using and already implemented linked list data structure. This linked list data structure conviniently supported sorting according to an auxillary, which in our case was the wake-up time. This saved us time implementing a priority queue data structure and allowed code re-use.

## Task 2: Priority Scheduler
**Summary:**
Our implementation of the priority scheduler chooses the next thread to run based on its highest effective priority. The thread with highest effective priority is scheduled to run. Took a lot of feedback from Josh's recommendations to avoid pitfalls.


**Changes since Design Doc:**
1. Most significant change was storing the effective priority of all threads as opposed to calculating the effective priority on the fly when the value was required.
1. Addition of a new function sema_with_greatest_waiter that only returns true if a given semaphore has a waiting thread with the highest priority. This function was essential for unblocking the highest priority thread when a resource had been released or when the current wants to signal a condition variable.


1. thread_set_effective_priority calculates the effective priority for the current thread.  It takes into account nested priority donation by checking the effective priority of the threads that are waiting for a lock that the current thread owns.

1. Priority donation was implemented in sema-down instead of lock_acquire in order to reuse the sema's waiter list and not use the lock's waiters list. This avoids redundancy because the lock's waiters list of threads would have been the same as the sema's waiters list of threads.
2. Got the max from ready list instead of maintaining a sorted ready list. 
## Task 3:
**Summary:**
Our implementation allowed the scheduler to select the next thread based on the priority scheduling algorithm defined in the spec. 

**Changes since Design Document:**
1. Use of 1 queue instead of 64 queues. Our implementation involves one single queue that gives the behavior of having multiple queues. This improves work sanity in dealing with one single queue.

1.  Use of a linked list with an auxillary function that allows popping of highest priority thread when scheduling.

1. Since recent_cpu is a moving average, it is calculated inside of thread_tick for the running thread.

1. init_MLFQS sets correct static variables for the mslfqs scheduler
2. For mlfqs, nice and recent_cpu thread attributes are set to 0 in the initialize thread function.
3. Our update_load_avg checks excludes the idle_thread from the specified formula. Instead, idle_thread formula computes the size of the ready list of threads as the updated load average variable.
4. Added functions to update the load_avg, recent_cpu, and the MLFQS priority of a thread. Also reused effective priority from task 2 to keep things simple. 

### Group Member Reflections and Recommendations:
**Emilio Aurea**

1. What did I do?
I completed the design and implementation of task 2. I also edited the design and implemented task 3. 

2. What went well?
Task 2 went well because of the solid design doc. The hardest part was figuring out how to do things in Pintos. Overall, it was interesting and fun.

3. What could be improved?
I was stuck debugging on Task 3 because I was relying on a working implementation of task 1 (which was not yet completed). This was frustating because I felt like I was doing all of the work and even went so far as to implement a non-working version of task 1. Once task 1 was implemented correctly, task 3 worked perfectly.
Overall, I wish the work was distributed more appropriately. I feel that I put in a substantial amount of work into the design doc and the code and I wish I could have discussed some of my ideas with my teammates instead of just delegating.

**Samuel Mbaabu**
1. What did I do?
I worked on the design document for task 3.
I compiled the final report for the project.
I helped in debugging part 3.

2. What went well?

3. What could be improved?
Better group communication. 
More group frequent sessions.

**Juan Castillo**
1. What did I do?
a.	I designed the document for task 1 (efficient alarm clock)  along with Muluken for this part of the project. That includes designing the data structures that were used, the function the algorithms complexity and the synchronization.
b.	I collaborated with Muluken in the implementation for the solution of the efficient alarm clock for the scheduler. That included the writing of the code; the testing of the code and the commenting needed to describe the job of the functions.

2.	What went well?
a.	Task 1 went well thanks to the decision of using a sleeping queue for keeping the block threads. This gave the green light for the used, with a little of extra code , of other already implemented functions in pintos “list.h” header. Some of these functions were for example, the “list_insert_ordered” to keep our block threads sorted by the shortest time left to wake up,  “thread_block”  and  “thread_unblock” to block and unblock threads, and functions to traverse through the queue and remove or pop from it as well. The use of these functions gave us a much narrower window to commit mistakes.
b.	The initial design was also a great accomplishment since it was a great guidance to check our ideas and keep track of the steps that still needed to be implemented. 
3.	What could be improved?
a.	For task 1 the sleeping queue could be implemented a a binary heap to improved the runtime and keep it sorted at all times and spend less time sorting the n threads O(nlogn) every time a new thread gets inserted. This will improve the insertion runtime to O(logn) and the removal time will still be constant.
![Uploading file..._hg2zlyx2x]()

**Muluken Taye**
1)	What did I do?
    Collaborated with Juan to work on designing the document for task one, efficient alarm clock. Different data structures that could speed up putting running threads into sleep and waking them up in time were considered. We chose to implement the list data structure along with functions that ease ordered insertion and removal of elements. 

    Worked with Juan coding and debugging the efficient alarm clock algorithm that we crafted during the design phase. I helped in adding functions that improved operation on list and made sure the tests passed.  
2)	What went well?
    Removing the sleeping thread and putting them into a ready queue at the required time tick was easier. This is because of synchronized call by timer_interrupt() and implementation of thread_unblock() that automatically inserts them into ready queue after unblocking a thread.

    Implementation of ordered sleeping thread queue was great. It made removal of sleeping thread faster, this shortens the time elapsed during timmer_interrupt() call. This is a required feature since we don’t want interrupts disabled for a significant period of time in order to use the CPU efficiently.   
3)	What could be improved?
    Debugging required a lot of effort than implementing the actual code. It was harder to figure out what went wrong when the code failed the tests. We could have used GDB debugger.
    
    As Juan mentioned above, min-heap could be used to increase insertion of threads in an ordered queue from linear run time to logarithmic run time but this requires significant amount of work and is less reliable than the existing data structure. 
![Uploading file..._dsa360yqy]()


Final Report for Part 2: User Programs
===================================
## Index:
1. Task 1: Argument Passing
2. Task 2: Process Controll Syscalls
3. Task 3: File Operation Syscalls
4. Student Testing Report
5. Group Member Reflections and Recommendations

## Parts Described On Each Task:
1. Task implentation summary
1. Changes since initial design

## Group Members


* Emilio Aurea: <emilioaurea@berkeley.edu>
* Samuel Mbaabu: <samuel.karani@berkeley.edu>
* Juan Castillo: <jp.castillo@berkeley.edu>
* Muluken Taye: <muluken@berkeley.edu>



## Task 1: Argument Passing
**Summary:** We use an array of the max argument size (1024) to store the strings and a similar array to store the addresses of those strings copied on to the stack.

**Changes Made:** No significant changes were made since the design doc. The logic was moved into `setup_stack` because it seemed like the more appropriate location. Aside from that, we followed the design doc.
## Task 2: Process Control Syscalls
**Summary:** We used new structs to share state between parent and child for `exec` and `wait`.
```C
struct wait_status {
    int exit_code;
    struct list_elem elem;
    tid_t tid;
    struct semaphore sema;
    int ref_count;
    struct ref_count_lock;
}

struct exec_stuff {
    int load_success;
    struct semaphore sema;
}

// Additions to thread struct
struct file *file; /* Pointer to my executable file */
struct exec_stuff *child_loaded; /* Will block on this to wait for child to load. */
struct exec_stuff *parent_load; /* Will up this when loaded to signal parent. */
struct list children; /* List of execed child wait_structs */
struct wait_status *my_wait_status; /* Pointer to my wait status */
```

The parent will malloc the `exec_stuff` and pass a pointer to the child in `thread_create`. Then, the parent will wait on `sema` until the child finishes loading.

The parent will also malloc the `wait_status` and pass a pointer on to the child in `thread_create`. 

When the child exits it will do the following to it's own `wait_struct`:

1. Changes the `exit_code`.
2. Ups `sema`. 
3. It also acquires the `ref_count_lock` and decrements `ref_count` and may free the struct.

And the following to it's children's `wait_struct`:

1. Acquires the `ref_count_lock` and decrements `ref_count` and may free the struct.

If the parent ever waits on the child, it will down
`sema`. Then, it will retrieve exit code. Because the child exited, it will free the struct. 

**Changes Made:** We made significant changes since the initial design doc. Basically, the initial design doc assumed we could not use `malloc`, and thus, it was completely wrong and we threw it all away. We followed Josh's recommendation and malloced new structs to keep track of shared state between parent and child. This was much simpler and cleaner than our previous approach.

## Task 3: File Operation Syscalls
**Disclaimer:** The design doc for this section was changed significantly from the time it was due until our meeting with Josh. This final report is based on the design doc that was submitted before the due date.
**Summary:** Each thread has:
- an array of file pointers where each entry in the array corresponds to a file descriptor 
- the number of file descriptors it has open

```C
// Additions to thread struct
struct file * fd_table[MAX_FILE_DESCRIPTORS];
unsigned int num_fds;
```

**Changes Made:** The main change is that the file descriptor "table" lives inside the thread struct instead of in userproj/syscall.c. The implementation was pretty much in line with the design doc.



## Student Testing Report:
### Test 1: remove-open
**Description:**
`remove-open.c` tests for the standard UNIX semantics for files in pintos. That is essentialy making sure that when a file is removed any process which has a file descriptor for that file may continue to read and write from the file. Also since the  file will no longer have a name because it was removed, this tests that no other processes will be able to open it, but it will continue to exist until all file descriptors referring to the file are closed or the machine shuts down.

**Mechanics, Qualitative Description and Expected Output:** 
1. We open a file descriptor using the sample text file `sample.txt` provided for us in tests/userprog. 
2. Remove the sample text file and check if the the text file was removed, if it was not removed we return a fail.
3. We must be able to still read from this file decriptor so after the file was removed. So we try to read a byte from the file descriptor and check that the output from read is not less than 1, if it's less than 1 this indicates remove violtes the standard UNIC semantics for files and so a fail must be returned.
4. If everything worked up until here then we close the file descriptor.
5. Finally, we try to open a new file descriptor with `sample.txt`. This open should return -1 indacating an error and we check that this is the case, if it exists we return a fail indicating this file should've been removed.

The expected output for this test should pass.

**userprog/build/tests/userprog/remove-open.output:**

`Copying tests/userprog/remove-open to scratch partition...`
`Copying ../../tests/userprog/sample.txt to scratch partition...`
`qemu -hda /tmp/UL_wCnmEKU.dsk -m 4 -net none -nographic -monitor null`
`PiLo hda1`
`Loading..........`
`Kernel command line: -q -f extract run remove-open`
`Pintos booting with 4,088 kB RAM...`
`382 pages available in kernel pool.`
`382 pages available in user pool.`
`Calibrating timer...  419,020,800 loops/s.`
`hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"`
`hda1: 166 sectors (83 kB), Pintos OS kernel (20)`
`hda2: 4,096 sectors (2 MB), Pintos file system (21)`
`hda3: 105 sectors (52 kB), Pintos scratch (22)`
`filesys: using hda2`
`scratch: using hda3`
`Formatting file system...done.`
`Boot complete.`
`Extracting ustar archive from scratch device into file system...`
`Putting 'remove-open' into the file system...`
`Putting 'sample.txt' into the file system...`
`Erasing ustar archive...`
`Executing 'remove-open':`
`(remove-open) begin`
`(remove-open) open "sample.txt"`
`(remove-open) end`
`remove-open: exit(0)`
`Execution of 'remove-open' complete.`
`Timer: 55 ticks`
`Thread: 0 idle ticks, 54 kernel ticks, 1 user ticks`
`hda2 (filesys): 114 reads, 219 writes`
`hda3 (scratch): 104 reads, 2 writes`
`Console: 977 characters output`
`Keyboard: 0 keys pressed`
`Exception: 0 page faults`
`Powering off...`

**userprog/build/tests/userprog/remove-open.result:**

`PASS`

**Potential Kernel Bugs:**
- In pintos we are allowed to create a simulated disk named filesys.dsk that contains a 2 MB Pintos file system partition if specified as such or with a certain fixed memory. If we were to have a file in the file system that needs almost all the memory assigned to the file system partition and then we were to remove that file then the file can't be accessed anymore from other calls but it's still allocated in the file system. On top of that there's also no way of checking this is the actual reason for not accesing the file because we can't add anything to the file system anymore due to the memory being full.
- There's also no way of cheking for memory leaks this can potentially affect the memory of pintos and thus that of the file system.
        
### Test 2: safe-boundary
**Description:**

The test checks the safety for memory region that fall within a page boundary i.e. containing part valid and part invalid memory regions. It tests if Pintos prevents users from writing and reading data that can't fit within the user memory. A data type or buffer that goes beyond user virtual memory limit (PHYS_BASE) should cause error.  

**Mechanics, Qualitative Description and Expected Output:**

In the test, the last two bytes of virtual user memory has been used to store four bytes of data and we attempted to access the same data. The address can be computed as 0xc0000000 - 0x2 = 0xbffffffd, where 0xc0000000 is the valid virtual memory boundary. We dereferenced this address to store integer value at this location and access values stored in it.  

An integer data type takes four bytes of memory. If we try to write an integer at a memory address that is two bytes less than the boundary, 0xbffffffd, the value won't fit inside valid user memory region since we only have two bytes of user memory, 0xbffffffd and 0xbffffffe. The other two bytes will span into the kernel memory region. If we succeed writing and reading at this address, the test will output a text message that confirms successful write and read. Otherwise, it returns -1. 

This is illegal operation and pintots shouldn't this. So, we expect the test to cause error and -1 to be returned. 

Output:

`Copying tests/userprog/safe-boundary to scratch partition...`
`qemu -hda /tmp/1nfRxg4p6S.dsk -m 4 -net none -nographic -monitor null`
`PiLo hda1`
`Loading..........`
`Kernel command line: -q -f extract run safe-boundary`
`Pintos booting with 4,088 kB RAM...`
`382 pages available in kernel pool.`
`382 pages available in user pool.`
`Calibrating timer...  532,480,000 loops/s.`
`hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"`
`hda1: 166 sectors (83 kB), Pintos OS kernel (20)`
`hda2: 4,096 sectors (2 MB), Pintos file system (21)`
`hda3: 101 sectors (50 kB), Pintos scratch (22)`
`filesys: using hda2`
`scratch: using hda3`
`Formatting file system...done.`
`Boot complete.`
`Extracting ustar archive from scratch device into file system...`
`Putting 'safe-boundary' into the file system...`
`Erasing ustar archive...`
`Executing 'safe-boundary':`
`(safe-boundary) begin`
`Page fault at 0xc0000000: rights violation error writing page in user context.`
`safe-boundary: dying due to interrupt 0x0e (#PF Page-Fault Exception).`
`Interrupt 0x0e (#PF Page-Fault Exception) at eip=0x80480a3`
`cr2=c0000000 error=00000007`
` eax=00000100 ebx=00000000 ecx=0000000e edx=00000027`
`esi=00000000 edi=00000000 esp=bfffff80 ebp=bfffffb8`
`cs=001b ds=0023 es=0023 ss=0023`
`safe-boundary: exit(-1)`
`Execution of 'safe-boundary' complete.`
`Timer: 64 ticks`
`Thread: 0 idle ticks, 63 kernel ticks, 1 user ticks`
`hda2 (filesys): 61 reads, 206 writes`
`hda3 (scratch): 100 reads, 2 writes`
`Console: 1271 characters output`
`Keyboard: 0 keys pressed`
`Exception: 1 page faults`
`Powering off...`

Result:

`PASS`
**Potential kernel bugs:**
- The Kernal boundary (PHYS_BASE) has a default value of 0xc0000000. In the test for memory safety, memory address at the boundary is hardcoded. If the default value of PHYS_BASE increases by at least two, our test will fail.  
- Had Pintos checked only if the pointer to a given data is inside the user's virtual memory boundary without checking the length of the data, the test would have failed. 

## Group Member Reflections and Recommendations:

**Emilio Aurea**
1. What did I do?
I implemented task 2 system calls and wrote the final report for tasks 1, 2, 3.
2. What went well?
I learned a lot of the inner workings of pintos from having to write `wait` and `exec`. It was challenging, but a great learning experience.
3. What could be improved?
I think pair programming would have made my life much easier. I had a small bug (forgot to add a break in a switch statement) that took me a day to figure out. Also, I wish we had discussed who was going to do what beforehand because one team member took it upon himself to do most of the work (which I am eternally grateful for), but it left parts left to work on pretty isolated so I didn't get to work with anyone. 

**Samuel Mbaabu**
1. What did I do?
2. What went well?
3. What could be improved?


**Juan Castillo**
1. What did I do?
Planned and worked in the implementation and design for the extra tests cases requiered for this project. Excercised and investigatd the GDB questions required for this project. Collaborated and wrote part of the Student Testing Report with Muluken which included exploring and coming up with non-trivial kernel bugs that may've had a significant impact to our written tests.
2. What went well?
The tests we wrote went well, we ran into a couple of misunderstanding while creating the tests, in terms of perl formatting and how to access the tests with makeTest but overall it was a success.
3. What could be improved?
There's always big gap when it comes to testing for errors and one should always be attentive when testing by making sure tests are robust enough. We could improve testing by adding more testing and make our tests even more detailed.

**Muluken Taye**
1. What did I do?
Worked on the student testing part. Wrote code that tests the saftey of memory boundary and completed the testing report. I also helped on the file system testing.  
2. What went well?
We were able to succesfuly test that we can continue making different file system calls on a removed file as long as it isn't closed. In addition, memory safety checking was easily implemented by hardcoding the address and assigning value at that location. 
3. What could be improved?
Adding different cases to the memory safety test could have improved the coverage well. Such as using buffered data and other data types. Including more file system calls e.g. calling remove twice could also strengthen the test. 


Part 3: File Systems
===========================================

## Group Members

* Emilio Aurea <emilioaurea@berkeley.edu>
* Samuel Mbaabu <samuel.karani@berkeley.edu>
* Juan Castillo <jp.castillo@berkeley.edu>
* Muluken Taye <muluken@berkeley.edu>

## Task 1: Buffer Cache
### Data Structures and Functions
We will create a new file, Cache.c, with the following:
```C
struct cache_block {
    block_sector_t block_tag; // Which sector is currently in the cache
    char sector[512]; // Basically 512 contiguous bytes.
    struct lock block_lock; // One Lock for each block.
    bool dirty_bit; // Does it need to be written to disk on eviction?
    struct list_elem elem; // Will go into LRU list.
}
struct cache_block blocks[64]; // Our cache blocks.
static struct list LRU_list; // Doubly Linked List to implement LRU.
struct lock LRU_lock; // Only one thread can modify list at a time.
```
### Algorithms
#### Overview
The cache will be Fully Associative. No `elem` will be put in `LRU_list` at the beginning.
#### Access
When a thread wants to search for a block, the first thing they must do is acquire the `LRU_lock`. The reason for this is that only a thread with the `LRU_lock` can evict or add a block. Once the thread has this lock, it will iterate `while (i <= last_block)` initializing `i = 0` and checking the `block_tag` to see if it is the one it is looking for. If it finds this block, then it will put it at the back of the `LRU_list`. After this, it will release the `LRU_lock` and put itself in line to acquire the `block_lock`.
#### Addition/Eviction
This is the case when a thread searched through all the blocks, but could not find the block it wanted. Thus, it still has the `LRU_lock`. Now, it will get the block at the front of the `LRU_list`. If `last_block != 63`, then we just got an unitialized block, so we increment `last_block += 1`. Then, we can simply acquire the `block_lock` for this block because it was uninitialized. We can update the `block_tag`

### Synchronization

Only one thread will be able to access a block in the cache whether its a read or a write. Moreover, because the cache is direct-mapped, there is only one place to look at for the sector. Because of this, any read or write to the same sector will be sequencial. Thus, there are no race conditions.
### Rationale
To make synchronization easier for different sectors, we decided to use a direct mapped cache. The tradeoff is that with lower associativity, we will have more conflict misses.

Other designs we considered included a Fully Associative cache, but we could not figure out how to elegantly deal with synchronization. 
## Task 2: Extensible Files
### Data Structures and Functions
`struct inode` will be modified to have `inode_disk *` as an attribute inside the struct instead of `inode_disk`. Inode_disk will have  direct, indirect, and doubly-indirect pointers that point to the next available `inode_disk` struct that  will allow file extension.
### Algorithms
When `inode_write_at` attempts to write on inode_disk struct that is full, the write operation will create a  new pointer locating a new inode_disk that can be written to. Before allocation a new `inode_disk`, a check for enough space to accomodate a new `inode_disk` struct will be performed. If true, a successful allocation shall occur.

`inumber(int fd)` will read the current thread's file descriptor table and find the corresponding struct file *file. This `struct file *file` will be used to acquire the inode number encapsulated in `inode struct` under the `sector` attribute.
### Synchronization
An inode lock in `inode.c` will be acquired when creating new inodes and `inode_disk` pointers. This will account for correct behavior when calling inode_write and a new `inode_disk` block needs to be assigned to the current block.
### Rationale
Our implementation will facilitate direct, indirect, doubly indirect, and triple indirect access to blocks. We decided to have 3 layers because it would allow us to access all 2^14 number of blocks a file can own.
## Task 3: Subdirectories
### Data Structures and Functions
Additions to thread struct 
```C
struct dir *cwd;
```
Additions to `dir_entry` struct
```C
bool is_dir; // Is entry a directory or a file?
```
Additional struct to put in the fd_table
```C
struct fd_entry {
    bool is_dir; /* Tells us if this file descriptor is a directory or a file. */
    void *ptr; /* Points to a directory or a file. */
}
```
Modify `dir_add` to take in the boolean and initialize the `dir_entry` appropriately and add it to the target directory. Modify `filesys_open` to call either `file_open` or `dir_open`, and `dir_lookup` to return not only bool of success or fail but also give us access to the corresponding `dir_entry` for the `inode`. We will also refractor the syscalls to work with `fd_entry` and get the file or directory -- thus, we'll have to malloc the struct.
### Algorithms
#### Overview
We will store a pointer to the cwd of a process in the thread struct.
#### Computing a Relative Path
Beginning at our `cwd`, we will append the relative path and use `dir_lookup`.
#### Computing an Absolute Path
Beginning at the root directory, we'll use the for loop in `dir_lookup` and the code provided in the spec to resolve the file or directory and get its `inode`.
#### Deleting Current Working Directory
It is easier to implement when you **CANNOT** delete the current working directory.
### Synchronization
Will need to handle this. For now, global lock. 
### Rationale
Feedback from Josh. 
## Design Document Additional Questions
### Possible Implementation Strategies for Cache:
**Write-behind:**

 One way of implementing `write-behind` would be to create a new kernel thread `dirty_thread` that will have a single task to flush the dirty bits after certain `wait_time` has passed.
 
`dirty_thread` will be put to sleep using non-busy waiting `timer_sleep()` after it has flushed all the dirty cache blocks to disk and thus we will achieve a periodical flushing.

We will experiment with different values of `n` in `timer_sleep(n)` to find a sweet spot bound for n `x<n<y` that gives best perfomance.


**Read-ahead:**
One way of implementing `read_ahead` will be to run an inner thread `thread_ahead`, while we are reading in the first block from disk to cache,  whose single task will be to read in the **second sequential block** to cache as well as the first. Thus for each block fetched, we always fetch the next sequential block (unless we fetch the last block on disk).


