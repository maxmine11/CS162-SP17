#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"


void 
test_main(void)
{
	char buf;
	int fd, byte_cnt;
	CHECK ((fd = open ("sample.txt")) > 1, "open \"sample.txt\"");

	if (!remove ("sample.txt")){
		fail( "Couldn't remove file.");
	}
	byte_cnt = read(fd, &buf, 1);

	if (byte_cnt < 1){
		fail ("The process should've continued to use the descriptor after remove.");
	}
	close(fd);
	fd = open ("sample.txt");
	if (fd != -1){
		fail("File shouldn't exist after the last file descriptor was closed.");
	}


}
