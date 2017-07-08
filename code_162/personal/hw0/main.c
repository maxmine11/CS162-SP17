#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    // First get the limit on memory 
    getrlimit (RLIMIT_STACK, &lim); 
    printf("stack size: %ld\n", lim.rlim_max);
    printf("process limit: %ld\n", 0L);
    printf("max file descriptors: %ld\n", 0L);
    return 0;
}
