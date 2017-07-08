Design Document for Project 2: User Programs
============================================

## Group Members


* Emilio Aurea <emilioaurea@berkeley.edu>
* Samuel Mbaabu <samuel.karani@berkeley.edu>
* Juan Castillo <jp.castillo@berkeley.edu>
* Muluken Taye <muluken@berkeley.edu>

## Task 1: Argument Passing
### Data Structures and Functions
New function `tokenize` will parse and tokenize `file_name` and set static variables `argv` and `argc`.

```C
#define ARGS_LIMIT 1024;
char * argv[ARGS_LIMIT];
unsigned int argc;
bool tokenize(char * file_name);
```
New function checks if `esp` is not word-aligned and then rounds the stack pointer down to a multiple of 4 before the first argument address push. Otherwise does not change `esp`.
```C
void word_align(void **esp)
```

### Algorithms
#### Tokenizing 
 `tokenize` will use `strtok_r` in its implementation to parse the `file_name` with _space_ as the delimiter.
 
  `tokenize` will be called inside `start_process` just before the call to `load`. If succesful, `load` may now correctly be called with `argv[0]` that was set by `tokenize`. A successful call to `filesys_open` inside `load` can now happen.
 
 #### Stack setup
 `load` will be modified to push `argv` _values_ and _addresses_ as well as `argc` and a _return address_ after the call to `setup_stack` has successfully finished.
 
#### Order of Pushing
1. Each argument in `argv` will be pushed onto the stack from right to left starting at `argc-1`. 
2. Call `word_align`.
3. Push `0` as the _null terminating sentinel_.
4. Each argument's address will be fetched (by calling `&argv[i]`) and pushed onto the stack. 
4. Push `argv`'s address onto the stack
6. Push `argc` onto the stack
7. Push a fake return address ` void (*) ()` that is equal to `0`. 

### Synchronization
Using `strtok_r` will maintain thread safety. Additionally, the function introduces a `char ** saveptr` that is used internally to maintain context between successive calls parsing the same string.

### Edge Cases
**Is using a static variables `argv` and `argc` thread safe?**
Static variables are stored in the data segment of memory but we are not sure how this will affect thread safety. A case to clarify over design review.

**How many arguments are allowed?**
In our impelmentation `tokenizer` will return `false` when the argument count exceeds `1024`

### Rationale
According to`process.c`,  `file_name` has to be tokenized before calling `load` and after calling `process_execute`. This is because both fuctions have `const char *file_name` in their function paramaters - meaning they don't expect `file_name` to be modified within them.

## Task 2: Process Control Syscalls
### Data Structures and Functions
Function to check syscall arguments:
```C
/* Checks if s is a valid string in a user page */
check_valid_string(char *s)
```
Function to modify that calls correct syscall:
```C
/* Will call correct syscall using switch statement*/
static void syscall_handler (struct intr_frame *f UNUSED)
```
Additions to thread struct to implement `wait`:
```C
/* Will hold the list of threads created from calling exec */
struct list child_proccesses
/* Each child has exactly one parent.
This list elem will be put in the parent's list of children */
struct list_elem child_elem
/* Child the parent is waiting for */
pid_t wait_for_child
/* Will hold the exit status and is initalized to NULL */
int exit_status
/* This semaphore will be used to signal the parent for wait
It is initialized to 0*/
struct semaphore sema_wait
```
Additionally, we will create the bottom half of all the syscalls inside syscall.c with prefix k. For example: `k_exec`
Moreover, we will be modifying `thread_exit` first set the exit status and then to block the thread if it wants to exit, but it has a parent. This is for `wait` so we can retrieve the status code.
Lastly, we will modify `process_execute` to set the current thread as the parent of the thread that was created after thread create returns. We will do this by disabling interrupts.
### Algorithms
#### - Argument Checking
This will be handled in `syscall_handler`. If the first argument is `SYS_EXEC`, then we will call `check_valid_string` with the second argument. `check_valid_string` will check 2 things:
> 1. The pointer not NULL.
> 2. All chars including the null terminator lie in user pages.
#### - Wait and Exit
These two go hand in hand. `wait` will work in 6 steps:
> 0. If child is not child return -1
> 1. Setting `wait_for_child`
> 2. Checking `exit_status` of child 
> 3. Try downing the `sema_wait` or actual down if `exit_status` was NULL.
> 4. Get the `exit_status` of child if NULL before
> 5. Remove child from children list and Unblock child 

`exit` - will set the exit status, print it exited and call `thread_exit`
`thread_exit` - after disabling interrupts, it will set the exit status if it's NULL and it will up the waiter's sema and it will block itself 

Only when the child thread **that the parent is waiting for** reaches thread_exit will it up the `sema_wait` of it's parent, thereby allowing it to wake up. Let's consider the 3 different cases to ensure correctness:
 1. Child reaches `thread_exit` before step 1 of `wait` - All of child will execute. So it will simply block itself. Then, parent will check the `exit_status` and retrieve the value. It will try down the `sema` just in case it was up. Lastly, step 5.
 2. Child reaches `thread_exit` between step 1 and 2 - Child will see that it's parent is waiting for it. So, it will up the sema and block itself. Then, parent will see the child's `exit_status` and retieve the value. Then, it will try down the `sema_wait` (which shouldn't be up but is) and do nothing with value. Lastly, step 5.
 3. Child reaches `thread_exit` between after step 2 -  Child will up the parent's `sema_wait` and block itself. Parent will then actually down the sema b/c the child's `exit_status` was NULL. This won't be a problem b/c the the sema is up. So it will do steps 4 and 5.
#### - Exec

Main idea is to call `process_execute` and use the tid as the pid. 
### Synchronization
#### - Argument Checking
There shouldn't be a problem because the user thread owns the pages and it won't continue executing until the syscall_handler completes.
#### - Wait and Exit
The algorithm section also covers the synchronization issues.
#### - Exec
Still trying to figure out the synchronization issue other than load.
### Rationale
#### - Argument Checking
We decided to use the first approach recommended in the design doc because it was coneptually easier to understand and implement.
#### - Wait and Exit
Needed a way for parent to be able to access a thread's exit status. If a thread exit, all the data in its thread_struct is garbage. Thus, if each thread has it's exit value, then it's thread struct can't be released (it can't die) until it's parent asks for it. One problem is that if it's parent asks for it, then it will never die. This is an easy fix in `thread_exit` if necessary. 

One idea was to create a new struct with (tid, exit_status) and have it not be in a thread's struct but somewhere else. In this way, the child can put this in a list the parent has access to. Thus, we would need malloc and we weren't sure if malloc could be used for this project.

We also considered using conditional variables. In fact, this gives cleaner and more concise code. The only issue is if the child tries to exit before the parent is waiting for it then all the threads waiting on a thread to exit will busy wait because they will keep signaling the next thread waiting for the condition.
#### - Exec
Insert completed rationale here.
## Task 3: File Operation Syscalls
### Data Structures and Functions

Initialize and maintain a table of file descriptor numbers and corresponding `struct file* files` (**process file descriptor table**) for each process after a successful call to `open` system call. File descriptor numbers will be uniquely assigned incrementally from `2`-`128` for each process. A **file descriptor entry** is removed on a call to `close` system call in that process's file descriptor table.

When the `128` file descriptors per process limit is hit, all file desciptor numbers will shifted down accordingly if any file descriptors had been closed.

A suitable data structure would be a _hash map_. However to promote code reuse and to avoid dynamic memory allocation, we intend on using a Pintos **linked list** holding `int fd's` and the associated `struct file* files` for our _process file descriptor table_.

The kernel will have an **open file table** that will map `struct file *files` to their corresponding `struct * inodes`. When a _process's file descriptor table_ becomes empty after _closing_ all its files or after exiting, it will prompt the kernal to delete its's **open table entry** if no other processes are using `file`.

```
/* process file table linked list */
struct int_list_elem
  {
    int fd;
    struct file * file;
    struct list_elem elem;
};
```

Then we setup the list:
```
/* Declare and initialize a list */
struct list fd_table;// inside thread.c
list_init (&fd_table); // inside thread_init(void)

/* Declare a list element. */
struct int_list_elem two = {2, some_file,  {NULL, NULL}};
/* Add it to the list */
list_push_back (&fd_table, &two.elem);
```

The **kernel open table** linked list and linked list elems will be similar but mapping `struct file* file` to `inode *` inside `userprog/syscall.c`.
### Algorithms
**`bool create (const char *file, unsigned initial_size)`**
This function will call `filesys_create (const char *name, off_t initial_size)` with `file` and `initial_size`. The success boolean from `filesys_create` will be returned.

**`bool remove (const char *file)`**
This function will call `filesys_remove (const char *name)` with `file`. The success boolean from `filesys_create` will be returned.

**`int open (const char *file)`**
This function will call `filesys_open (const char *name, off_t initial_size)` with `file` and `initial_size`. The `struct file * file` returned will be assigned a unique _file descriptor number_ and both will be added into the **file descriptor table** for that process.

**`int filesize (int fd)`**
This function will call `off_t file_length (struct file *file)` after looking up the associated `struct file *file` from the file descriptor table using `fd`. 

**`int read (int fd, void *buffer, unsigned size)`**
This function will call `off_t file_read (struct file *file, void *buffer, off_t size)` after looking up the associated `struct file *file` from the file descriptor table using `fd`. `buffer` and `size` will be passed along correspondingly.

**`int write (int fd, const void *buffer, unsigned size)`**
This function will call `off_t file_write (struct file *file, void *buffer, off_t size)` after looking up the associated `struct file *file` from the file descriptor table using `fd`. `buffer` and `size` will be passed along correspondingly.


**`void seek (int fd, unsigned position)`**
This function will call `void file_seek (struct file *file, off_t new_pos)` after looking up the associated `struct file *file` from the file descriptor table using `fd`. `unsigned_position` will be passed along correspondingly.

**`unsigned tell (int fd)`**
This function will call `off_t file_tell (struct file *file)` after looking up the associated `struct file *file` from the file descriptor table using `fd`. 

**`void close (int fd)`**
This function will call `void file_close (struct file *file)` after looking up the associated `struct file *file` from the file descriptor table using `fd`.

### Edge Cases
**When a process attempts to open more than 128 (opened) files:**
Should return a -1 code.

**When an opened file is removed:**
File will still be accessible by processes with that file descriptor but it will not have a name and all no process will be able to open it.


### Synchronization
Creating a global `struct lock file_lock` in `userprog/syscall.c`. This lock is acquired before calling any of the functions in `file.c` or `filesys.c` and is released after each call. 

This will ensure thread saftey for the file system calls.

### Rationale
Functions `create`, `remove` and `open` will call their respectively-named functions in `filesys.c` and simply return that result from those calls. 

Functions `file_size`, `read`, `write`, `seek`, `tell` and `close` will lookup `fd` in their _process file descriptor tables_ and get `struct file* file`. With`file`, the functions will now call respectively-named functions in `file.c` and simply return their results. 

Each process will have its own **process file descriptor table** attribute inside `struct thread`. This attribute is updated when a process _opens_ and _closes_ a _file_. The **kernel open table** will live inside `userprog/syscall.c`.


## 2.1.2 Design Document Additional Questions:
1. _test file_: `sp-ba-sp.c`
The stack pointer `%%esp` is set to a negative address `-(64*1024*1024)`  in _line 18_ which is an invalid address.  
The test checks that the initiated system call exits with a `-1` code. 

1. _test file_:  `sc-bad-arg.c`. 
The system call number is pushed to the very top of the stack at address `0xbffffffc` in _line 14_.
When it is popped, other system call arguments that may follow would be further above the page boundary (i.e. `PHYS_BASE = 0xc0000000`) which is outside the user address space.
The test checks that the initiated system call exits with a `-1` code.

1. * Thread safety for the filesystem. Tests that check that file operation system calls are not called concurrently. 
    * Safety for memory regions that fall within a page boundary i.e. containing part valid and part invalid memory regions.

## 2.1.3 GDB Questions
1. thread address: `0xc000e008` thread name: `"main"`
other existing threads: `pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main",
 '\000' <repeats 11 times>, stack = 0xc000ee0c "\210", <incomplete sequence \357>, prio
rity = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev =
0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446
325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle",
 '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc0
00e020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next =
 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}`

1. backtrace: `#0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userpro
g/process.c:32
#1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
#2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
#3  main () at ../../threads/init.c:133`

1. thread address: `0xc010a008` thread name: `"args-none"`
other existing threads: `pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main",
 '\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev =
 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, ne
xt = 0xc003655c <temporary+12>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle",
 '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc0
00e020, next = 0xc010a020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <
ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #2: 0xc010a000 {tid = 3, status = THREAD_RUNNING, name = "args-n
one\000\000\000\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0
xc0104020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, nex
t = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}`

1. creation point of start_process: `#1  0xc002128f in kernel_thread (function=0xc002a125 <start_process>, aux=0xc0109000) a
t ../../threads/thread.c:424`

1. address of page fault: `0x0804870c`

1. btpagefault: `#0  _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<erro
r reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9`

1. Error reading `argv` in **line 9 of entry.c**. This is because argument passing has not yet been implemented and a page fault occurs when trying to access `argv[0]` to print it.
