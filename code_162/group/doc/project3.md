Design Document for Project 3: File Systems
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
    block_sector_t curr_block; // Which sector is currently in the cache
    char sector[512]; // Basically 512 contiguous bytes.
    struct lock block_lock; // One Lock for each block.
    bool dirty_bit; // Does it need to be written to disk on eviction?
    struct list_elem elem; // Will go into LRU list.
}
struct cache_block *blocks[64]; // Our cache blocks.
static struct list LRU_list; // Doubly Linked List to implement LRU.
struct lock LRU_lock; // Only one thread can modify list at a time.
```
### Algorithms
#### Overview
The cache will be Direct Mapped. We will determine where a sector will go by computing it's `block_sector_t` mod 64. The eviction algorithim is LRU. The front of `LRU_list` is the least recently used.
#### First Access
If that corresponding entry is `NULL`, then this is the first time and we can `malloc` a new `cache_block` and read or write to the cache entry instead after fetching the block from disk. We will also acquire `LRU_lock` and add the `elem` to back of `LRU_list`.
#### Subsequent Access
Otherwise, we shall acquire the `block_lock` and read or write as necessary to `sector`, setting `dirty_bit` if necessary. We will also acquire `LRU_lock` and add the `elem` to **back** of `LRU_list`.
#### Eviction
Peak in `LRU_list` to find the **first** `elem` and acquire the `block_lock` for the corresponding `cache_block`. Also, acquire the `LRU_lock` and move the `elem` to the back. Then, fetch the new sector from disk and update `curr_block` and `dirty_bit`. Release `LRU_lock`, and proceed as with **Subsequent Access**.

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

1. One way of implementing `write-behind` would be to create a new kernel thread `dirty_thread` that will have a single task to flush the dirty bits after certain `wait_time` has passed.
2. `dirty_thread` will be put to sleep using non-busy waiting `timer_sleep()` after it has flushed all the dirty cache blocks to disk and thus we will achieve a periodical flushing.


**Read-ahead:**
1. One way of implementing `read_ahead` will be to run an inner thread `thread_ahead`, while we are reading in the first block from disk to cache,  whose single task will be to read in the second block to cache (that is the following block from the file if there are any).
