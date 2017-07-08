/* This program attempts to write and read data that spans from user to kernel
   This should terminate the process with a -1 exit code. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
 	*(int *)0xbffffffd = 25;
	fail ("should have exited with -1, bad write");
 	msg ("Congratulations - you have successfully read user that spans into the kernel memory: %d",
        *(int *)0xbffffffd);
  	fail ("should have exited with -1, bad read");
}