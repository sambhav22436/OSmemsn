#include"../../mems.h"
#include<stdio.h>
#include<stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>

void setmemlimit(){
        struct rlimit memlimit;
        long bytes = 5*(1024*1024);
        memlimit.rlim_cur = bytes;
        memlimit.rlim_max = bytes;
        setrlimit(RLIMIT_AS, &memlimit);
}

int main(int argc, char const *argv[])
{
    printf("-------- STARTING TEST ---------\n");
    setmemlimit();
    printf("Limiting the total memory used by the program\n");
    mems_init();
    int* ptr[10000];
    int* phy_ptr;
    for(int i=0;i<10000;i++){
        ptr[i] = (int*)mems_malloc(10*sizeof(int));
        phy_ptr = (int*)mems_get(ptr[i]);
        phy_ptr[0] = i;
    }
    printf("\nTEST: SUCCESS\n");
    printf("--------- ENDING TEST ----------\n");
    return 0;
}
