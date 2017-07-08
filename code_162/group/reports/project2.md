Final Report for Project 2: User Programs
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
Implemented task 1, parts of task 2 and task 3. prepared design document for tasks 1 and 3.
2. What went well?
The project was a great learning experience for program loading and the process of syscalls execution.
3. What could be improved?
Everything went well.


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
