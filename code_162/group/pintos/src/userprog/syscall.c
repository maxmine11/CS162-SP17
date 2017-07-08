#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* proj 2 additions */
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "lib/string.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/directory.h"
#include "filesys/inode.h"

/* Project 2 Additions */
static int get_next_fd (void);
static bool check_arg (char * arg);

static void syscall_handler (struct intr_frame *);

static bool check_esp (uint32_t * args);
static int get_next_fd (void);
static bool check_arg (char * arg);
static bool check_fd (int fd);
static bool check_fd_read_write (int fd);
void syscall_init (void);

static void k_exit (struct intr_frame *f UNUSED, uint32_t* args);
static void k_open (struct intr_frame *f UNUSED, uint32_t* args);
static void k_write (struct intr_frame *f UNUSED, uint32_t* args);
static void k_read (struct intr_frame *f UNUSED, uint32_t* args);
static void k_create (struct intr_frame *f UNUSED, uint32_t* args);
static void k_remove (struct intr_frame *f UNUSED, uint32_t* args);
static void k_filesize (struct intr_frame *f UNUSED, uint32_t* args);
static void k_seek (struct intr_frame *f UNUSED, uint32_t* args);
static void k_tell (struct intr_frame *f UNUSED, uint32_t* args);
static void k_close (struct intr_frame *f UNUSED, uint32_t* args);
static void k_exec (struct intr_frame *f UNUSED, uint32_t* args);
static void k_wait (struct intr_frame *f, uint32_t* args);
static void k_chdir (struct intr_frame *f UNUSED, uint32_t* args);
static void k_mkdir (struct intr_frame *f UNUSED, uint32_t* args);
static void k_readdir (struct intr_frame *f UNUSED, uint32_t* args);
static void k_isdir (struct intr_frame *f UNUSED, uint32_t* args);
static void k_inumber (struct intr_frame *f UNUSED, uint32_t* args);
static void error (struct intr_frame *f);

/* Check for valid user pointer ESP. */
static bool check_esp (uint32_t * args) 
{
  return args != NULL && is_user_vaddr (args) && pagedir_get_page (thread_current() ->pagedir, args) != NULL;
}

/* Return next available file descriptor for a thread, else -1. */
static int 
get_next_fd (void)
{
  unsigned int i;
  for (i = 2; i < MAX_FILE_DESCRIPTORS; i++) 
  {
    struct fd_entry *e = &(thread_current()-> fd_table[i]);
    if (!e->in_use) 
    {
      return i;
    }
  }
  return -1;
}

/* check valid pointer */
static bool 
check_arg (char * arg)
{
  return arg != NULL && check_esp ((uint32_t *) arg);
}

/* check for valid file descriptor */
static bool
check_fd (int fd) {
  return fd >= 2 && fd < MAX_FILE_DESCRIPTORS;
}

/* additional check for valid file descriptor for read/write syscalls */
static bool
check_fd_read_write (int fd) {
  return fd >= 0 && fd < MAX_FILE_DESCRIPTORS;
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

/* 
 * Kernel File and Process SYS CALL functions.
 */

/* exit process */
void k_exit (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (thread_current()->my_wait_status != NULL) {
    thread_current()->my_wait_status->exit_code = args[1];
  }
  f->eax = args[1];
  printf ("%s: exit(%d)\n", thread_current()-> name, args[1]);
  thread_exit ();
}

/* open a file */
void k_open (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_arg ((char *) args[1]))
    {
      lock_acquire (&file_lock);
      bool is_dir;
      struct dir *dir;
      char name[NAME_MAX + 1];
      bool success = path_resolution(&dir, name, (char *) args[1]);
      // printf ("inumber: %d\n", inode_get_inumber (dir_get_inode (dir)));
      if (!success)
      {
        f->eax = -1;
        return;
      }
      void *ptr = filesys_open (dir, (const char *) args[1], &is_dir);
      dir_close (dir);
      if (ptr)
      {
        int fd = get_next_fd ();
        // printf ("fd: %d\n", fd);
        struct fd_entry *entry = &(thread_current()-> fd_table[fd]);
        entry->ptr = ptr;
        entry->in_use = true;
        entry->is_dir = is_dir ? true : false;
        thread_current()-> num_fds += 1; //book-keeping
        f->eax = fd;
      }
      else
      {
        f->eax = -1;
      }
      lock_release (&file_lock);
    }
    else
    {
      error(f);
    }
}

/* write a file */
void k_write (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_fd_read_write (args[1]))
  {
    if (args[3] == 0)
    {
      f->eax = 0;
    }
    else if (args[1] > 0 && check_arg ((char *) args[2]))
    {
      lock_acquire (&file_lock);
      if (args[1] == 1)
      {
        putbuf ((const char * ) (args[2]), args[3]); // write to console
      }
      else
      {
        struct fd_entry *e = &(thread_current()-> fd_table[(unsigned) args[1]]);
        // Check if the fd is allocated and not a directory
        if (!e->in_use || e->is_dir) {
          error(f);
        }
        // Get the file struct pointer
        struct file *fl = (struct file *) e->ptr;
        if (fl)
        {
          f->eax = file_write(fl, (const void *) args[2], args[3]);
          // printf ("bytes written: %d\n", f->eax);
        }
        else
        {
          error(f);
        }
      }
      lock_release (&file_lock);
    }
    else
    {
      error(f);
    }
  }
  else
  {
    error(f);
  }
}

/* read a file */
void k_read (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_fd_read_write (args[1]))
  {
    if (args[3] == 0)
    {
      f->eax = 0;
    }
    else if (check_arg((char *) args[2]))
    {
      lock_acquire (&file_lock);
      if (args[1] == 0)
      {
        uint8_t * new_buffer = (uint8_t *) args[2];
        int size = args[3];
        int i;
        for (i = 0; i < size; i++)
        {
          new_buffer[i] = input_getc ();
        }
        f->eax = i;
      }
      else 
      {
        struct fd_entry *e = &(thread_current()-> fd_table[(unsigned) args[1]]);
        // Check if the fd is allocated and not a directory
        if (!e->in_use || e->is_dir) {
          error(f);
        }
        // Get the file struct pointer
        struct file *fl = (struct file *) e->ptr;
        if (fl)
        {
          f->eax = file_read (fl, (void *) args[2], args[3]);
        }
        else
        {
          error(f);
        }  
      }
    lock_release ( &file_lock);  
    }
    else 
    {
      error(f);
    }
  }
  else
  {
    error(f);
  }
}

/* create new file */
void k_create (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_arg ((char *) args[1]))
  {
    lock_acquire (&file_lock);
    struct dir *dir;
    char name[NAME_MAX + 1];
    bool success = path_resolution(&dir, name, (char *) args[1]);
    f->eax = success ? filesys_create (name, args[2], dir, false) : success;
    dir_close(dir);
    lock_release (&file_lock); 
  }
  else
  {
    error(f);
  }
}

/* remove a file */
void k_remove (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_arg ((char *) args[1]))
  {
    lock_acquire (&file_lock);
    struct dir *dir = dir_reopen(thread_current()->cwd);
    f->eax = filesys_remove ((const char *) args[1], dir);
    dir_close(dir);
    lock_release (&file_lock);
  }
  else
  {
    error(f);
  }
}

/* return file size */
void k_filesize (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_fd (args[1])) 
  {
    struct fd_entry *e = &(thread_current()-> fd_table[(unsigned) args[1]]);
    // Check if the fd is allocated and not a directory
    if (!e->in_use || e->is_dir) {
      error(f);
    }
    // Get the file struct pointer
    struct file *fl = (struct file *) e->ptr;
    if (fl)
    {
      lock_acquire (&file_lock);
      f->eax = file_length (fl);
      lock_release ( &file_lock); 
    }
    else
    {
      error(f);
    }
  }
  else
  {
    error(f);
  }
}

/* change next bytes to be read/written */
void k_seek (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_fd (args[1]))
  {
    struct fd_entry *e = &(thread_current()-> fd_table[(unsigned) args[1]]);
    // Check if the fd is allocated and not a directory
    if (!e->in_use || e->is_dir) {
      error(f);
    }
    // Get the file struct pointer
    struct file *fl = (struct file *) e->ptr;
    if (fl)
    {
      lock_acquire (&file_lock);
      file_seek (fl, args[2]);
      lock_release (&file_lock);
    }
    else
    {
      error(f);
    }
  }
  else
  {
    error(f);
  }
}

/* return position of next bytes to be read/written */
void k_tell (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_fd (args[1])) {
    struct fd_entry *e = &(thread_current()-> fd_table[(unsigned) args[1]]);
    // Check if the fd is allocated and not a directory
    if (!e->in_use || e->is_dir) {
      error(f);
    }
    // Get the file struct pointer
    struct file *fl = (struct file *) e->ptr;
    if (fl)
    {
      lock_acquire (&file_lock);
      f->eax = file_tell (fl);
      lock_release (&file_lock);
    }
    else
    {
      error(f);
    }
  }
  else
  {
    error(f);
  }
}

/* close a file */
void k_close (struct intr_frame *f UNUSED, uint32_t* args) 
{
  if (check_fd (args[1])) {
    struct fd_entry *e = &(thread_current()-> fd_table[(unsigned) args[1]]);
    // Check if the fd is allocated and not a directory
    if (!e->in_use || e->is_dir) {
      error(f);
    }
    // Get the file struct pointer
    struct file *fl = (struct file *) e->ptr;
    if (fl)
    {
      lock_acquire (&file_lock);
      file_close (fl);
      e->in_use = false;
      thread_current()-> num_fds -= 1; //book-keeping
      lock_release (&file_lock);
    }
    else
    {
      error(f);
    }
  }
  else
  {
    error(f);
  }
}
 
/* Exec a new process */
void k_exec (struct intr_frame *f UNUSED, uint32_t* args) 
{
  
  if (!check_arg ((char *) args[1])) {
    error(f);
  }

  struct exec_stuff *loaded_metadata; 
  loaded_metadata = (struct exec_stuff *) malloc (sizeof (struct exec_stuff));
  
  if ((void *) loaded_metadata == NULL) {
    f->eax = -1;
    return;
  }
  
  sema_init (&loaded_metadata->sema, 0);
  thread_current ()->child_loaded = loaded_metadata;
  
  tid_t pid = process_execute ((char *) args[1]);
  if (pid == TID_ERROR) {
    f->eax = -1;
    free ((void *) loaded_metadata);
    return;
  }
  // Block until we know child has tried to load.
  sema_down (&loaded_metadata->sema);

  if (!loaded_metadata->load_success) 
  {
    f->eax = -1;
    free ((void *) loaded_metadata);
    return;
  }

  free ((void *) loaded_metadata);
  f->eax = pid;
}

/* Sets return value to -1 then exits the thread. */
void k_wait (struct intr_frame *f, uint32_t* args) 
{
  tid_t pid = args[1];
  f->eax = process_wait (pid);
}

/* Change the process's current working directory */
void k_chdir (struct intr_frame *f UNUSED, uint32_t* args UNUSED) {
  // Implement me!!!
  if (!check_arg ((char *) args[1])) {
    // Invalid pointer
    error(f);
  }
  struct dir *dir;
  char name[NAME_MAX + 1];
  bool success = path_resolution(&dir, name, (char *) args[1]);
  if (success) {
    dir_close (thread_current()->cwd);
    thread_current()->cwd = dir_reopen(dir);
    f->eax = true;
  } else {
    f->eax = false;
  }
  dir_close (dir);
}

/* Make a new directory. */
void k_mkdir (struct intr_frame *f UNUSED, uint32_t* args UNUSED) {
  // Implement me!!!
  lock_acquire (&file_lock);
  struct dir *dir;
  char name[NAME_MAX + 1];
  bool is_dir;
  if (check_arg ((char *) args[1]))
  {
    bool success = path_resolution(&dir, name, (char *) args[1]);
    if (success) {
      f->eax = success ? filesys_create (name, 16, dir, true) : success;
      if (f->eax) {
        struct dir *new_dir = filesys_open(dir, name, &is_dir);
        init_dir(dir, new_dir);
        dir_close (new_dir);
      }
    }
    dir_close(dir);
    lock_release (&file_lock);
  }
  else
  {
    lock_release(&file_lock);
    error (f);
  }
  
  
}

/* Reads the next directory entry in a directory. */
void k_readdir (struct intr_frame *f UNUSED, uint32_t* args UNUSED) {
  // Implement me!!!
  if (!check_fd (args[1]))
    // Invalid fd
    error(f);
  struct fd_entry *e = &(thread_current ()->fd_table[args[1]]);
   if (e->is_dir && e->in_use)
   {
     struct dir *dir = (struct dir *) e->ptr;
     f->eax = dir_readdir (dir, (char *) args[2]);
   }
   else 
   {
     f->eax = false; 
   }
}

/* Checks if a fd corresponds to a directory or not. */
void k_isdir (struct intr_frame *f UNUSED, uint32_t* args UNUSED) {
  // Implement me!!!
  if (!check_fd (args[1]))
    // Invalid fd
    error(f);
  f->eax = (thread_current ()-> fd_table[args[1]]).is_dir;
}

/* Returns the inode number corresponding to a fd. */
void k_inumber (struct intr_frame *f UNUSED, uint32_t* args UNUSED) {
  // Implement me!!!
  if (!check_fd (args[1]))
    // Invalid fd
    error(f);
  struct fd_entry *e = &(thread_current ()-> fd_table[args[1]]);
  struct inode *i;
  if (e->is_dir)
    i = dir_get_inode(e->ptr);
  else
    i = file_get_inode(e->ptr);
  f->eax = inode_get_inumber(i);
}

void error (struct intr_frame *f)
{
  f->eax = -1;
  printf ("%s: exit(%d)\n", thread_current()-> name, -1);
  thread_exit ();
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* args = ((uint32_t*) f->esp);
  bool success = check_esp (args) && !is_kernel_vaddr (args + 4);
  if (!success)
  {
    error(f);
  }
  switch(args[0]) {
    case SYS_EXIT:
      k_exit(f, args);
      break;
    case SYS_OPEN:
      k_open(f, args);
      break;
    case SYS_WRITE:
      // printf ("write fd: %d\n", (int) args[1]);
      k_write(f, args);
      break;
    case SYS_READ:
      k_read(f, args);
      break;
    case SYS_CREATE:
      k_create(f, args);
      break;
    case SYS_REMOVE:
      k_remove(f, args);
      break;
    case SYS_FILESIZE:
      k_filesize(f, args);
      break;
    case SYS_SEEK:
      k_seek(f, args);
      break;
    case SYS_TELL:
      k_tell(f, args);
      break;
    case SYS_CLOSE:
      k_close(f, args);
      break;
    case SYS_PRACTICE:
      f->eax  = args[1] + 1;
      break;
    case SYS_HALT:
      shutdown_power_off();
      break;
    case SYS_EXEC:
      k_exec(f, args);
      break;
    case SYS_WAIT:
      k_wait(f, args);
      break;
    case SYS_CHDIR:
      k_chdir(f, args);
      break;
    case SYS_MKDIR:
      k_mkdir(f, args);
      break;
    case SYS_READDIR:
      k_readdir(f, args);
      break;
    case SYS_ISDIR:
      k_isdir(f, args);
      break;
    case SYS_INUMBER:
      k_inumber(f, args);
      break;
    default:
      // Syscall Not Implemented
      error(f);
  }
}