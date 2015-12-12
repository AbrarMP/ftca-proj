#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
 

#include <sys/mman.h>
#include <sched.h>
#include <ctype.h>
#include <string.h>

// typedef int bool;   
#define TRUE  1
#define FALSE 0 
 
bool *a;

 
int main()
{


    printf("--beginning of program\n");

    int NUM_PROCS = sysconf(_SC_NPROCESSORS_CONF);
    printf("System has %i processor(s).\n", NUM_PROCS);
     
    pid_t pid;
    pid_t cpid;
    int status;
    int shmid;
    bool *shm_ptr;
    key_t key=1234;
     
    //setting up the shared memory pointer to a
    shmid=shmget(key,1*sizeof(bool),0666|IPC_CREAT);
     
    if (shmid < 0)
    {
        perror("shmget");
        exit(1);
    }
     
    shm_ptr=(bool *)shmat(shmid,(void *)0,0);
     
    if (shm_ptr == (bool *)(-1))
    {
        perror("shmat:shm_ptr");
        exit(1);
    }
     
    a=shm_ptr; //a now points to a shared location in memory accessible by all processes
    *a = 0;
    pid = fork();

    cpu_set_t mask;

    /* CPU_ZERO initializes all the bits in the mask to zero. */ 
    CPU_ZERO( &mask );
    
    if (pid == 0) 
    {   
        printf("child pid is %d\n", pid);
        CPU_SET(3, &mask);
        if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 ){
            printf("WARNING: Could not set CPU Affinity, continuing...\n");
        }
        *a = 0;
        printf("a: (in child Proc)= %d\n",*a);
        double array[2048];
        for(int i = 0; i < 2048;i++)
        {
            array[i] = (double)i;
        }
        for(int k=0;k<2048;k++)
            for(int l=0;l<2048;l++)
        for(int j=0; j<2048;j++)
        {
            for(int i =0;i<2048;i++)
            {
                array[i]+=j;
            }
        }
        // sleep(10);
        *a = 1;
        printf("a: (in child Proc)= %d\n",*a);
        // exit(0);
    } else {

        CPU_SET(1, &mask);
        if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 ){
            printf("WARNING: Could not set CPU Affinity, continuing...\n");
        }
        printf("parent pid is %d\n", pid);
        while(*a==0)
        {
            printf("waiting for child Proc to return..\n");
            sleep(1);
        }
        printf("Child process has returned: a: %d\n", *a);
        // exit(0);
    }
     
     
    return 0;
 
}