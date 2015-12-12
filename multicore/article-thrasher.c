/*******************************************************************
 * This program has the sole purpose of showing some kernel API 
 * for CPU affinity. Consider this merely a demo...
 * 
 * Written by Eli Michael Dow <emdow@us.ibm.com>
 * 
 * Last Modified: May 31 2005. 
 *
 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <unistd.h>
#define  __USE_GNU
#include <sched.h>
#include <ctype.h>
#include <string.h>

/* Create us some pretty boolean types and definitions */
typedef int bool; 	
#define TRUE  1
#define FALSE 0 

/* Method Declarations */
void usage();                           	 /* Simple generic usage function */
bool do_cpu_stress(int numthreads);		 /* Entry point into CPU thrash   */
int  do_cpu_expensive_op(int myitem);            /* Single thread cpu intensive   */
bool check_cpu_expensive_op(int possible_result);/* Compare value to precomputed  */


int main( int argc, char **argv )
{
	int return_code = FALSE;

	/* Determine the actual number of processors */
	int NUM_PROCS = sysconf(_SC_NPROCESSORS_CONF);
  	printf("System has %i processor(s).\n", NUM_PROCS);

	/* These need sane defaults, because the values will be used unless overriden */
	int num_cpus_to_spin = NUM_PROCS; 

        /* Check for user specified parameters */
	int option = 0; 
   	while ((option = getopt(argc, argv, "m:c:l?ahd")) != -1)
   	{
      		switch (option)
      		{
	       		case 'c': /* SPECIFY NUM CPUS TO MAKE BUSY */
		          num_cpus_to_spin = atoi(optarg);
			  if (num_cpus_to_spin < 1)
			  {
				printf("WARNING: Must utilize at least 1 cpu. Spinning "
					" all %i cpu(s) instead...\n", NUM_PROCS);
				num_cpus_to_spin = 1;
			  }
			  else if (num_cpus_to_spin > NUM_PROCS)
			  {
				printf("WARNING: %i cpu(s), are not "
				       "available on this system, spinning all %i cpu(s) "
                                       "instead...\n", num_cpus_to_spin, NUM_PROCS);
				num_cpus_to_spin = NUM_PROCS;
			  }
			  else
			  {	
			  	printf("Maxing computation on %i cpu(s)...\n",
			   	       num_cpus_to_spin);
			  }
			break;


			case '?':
	        		if (isprint (optopt))
	          		{
					fprintf (stderr, 
					"Unknown option `-%c'.\n", optopt);
				}
	        		else
				{
	          			fprintf (stderr,
	                   		"Unknown option character `\\x%x'.\n",
	                   		optopt);
				}
			break;

		        default:
		          usage(argv[0]);
		          exit(0);
		}
	}

	/* Kick off the actual work of spawning threads and computing */
	do_cpu_stress(num_cpus_to_spin); 
	return return_code;
}



/* This method simply prints the usage information for this program */
void usage()
{
	printf("[-c  NUM_CPUS_TO_STRESS]\n");
	printf("If no parameters are specified all cpu's will be made busy.\n");
	return;
}



/* This method creates the threads and sets the affinity. */
bool do_cpu_stress(int numthreads)
{
	int ret = TRUE;
	int created_thread = 0;

	/* We need a thread for each cpu we have... */
	while ( created_thread < numthreads - 1 )
	{
		int mypid = fork();
		
		if (mypid == 0) /* Child process */
 		{
		    printf("\tCreating Child Thread: #%i\n", created_thread);
		    break;
		}

		else /* Only parent executes this */
		{ 
		    /* Continue looping until we spawned enough threads! */ ;
		    created_thread++;
		} 
	}



	/* NOTE: All threads execute code from here down! */



	cpu_set_t mask;

	/* CPU_ZERO initializes all the bits in the mask to zero. */ 
        CPU_ZERO( &mask ); 	

	/* CPU_SET sets only the bit corresponding to cpu. */
        CPU_SET( created_thread, &mask );  
        
	/* sched_setaffinity returns 0 in success */
        if( sched_setaffinity( 0, sizeof(mask), &mask ) == -1 )
	{
		printf("WARNING: Could not set CPU Affinity, continuing...\n");
	}
	/* sched_setaffinity sets the CPU affinity mask of the process denoted by pid. 
	   If pid is zero, then the current process is used.

	   The affinity mask is represented by the bitmask stored in mask. The least
	   significant bit corresponds to the first logical processor number on the
	   system, while the most significant bit corresponds to the last logical 
	   processor number on the system. A set bit corresponds to a legally schedulable
	   CPU while an unset bit corresponds to an illegally schedulable CPU. In other 
	   words, a process is bound to and will only run on processors whose 
           corresponding bit is set. Usually, all bits in the mask are set.

	   Also the affinity is passed on to any children!
	*/

	 /* Now we have a single thread bound to each cpu on the system */
	 int computation_res = do_cpu_expensive_op(41);
	 cpu_set_t mycpuid;	 
	 sched_getaffinity(0, sizeof(mycpuid), &mycpuid);

	 if ( check_cpu_expensive_op(computation_res) )
	 {
		printf("SUCCESS: Thread completed computational task, and PASSED integrity check!\n",
			mycpuid);
		ret = TRUE;	
	 }
	 else 
	 {
		printf("FAILURE: Thread failed integrity check!\n",
			mycpuid);
		ret = FALSE;
	 }	


	return ret;
} 


/* Lame (computationally wasteful) recursive fibonaci sequence finder 
   Intentionally does not store known computed values. 
*/
int do_cpu_expensive_op(int myitem)
{
	/* FIXME: Should check myitem size because this could overflow quick */
	if (myitem == 0 || myitem == 1) 
	{ 
		return myitem; 
	}

	return ( do_cpu_expensive_op( myitem - 1 ) + do_cpu_expensive_op( myitem - 2 ) ); 
}



/* This method simply takes an integer argument
   and compares it to a precomputed correct value.
*/
bool check_cpu_expensive_op(int possible_result)
{
	/* 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987 ... fib(41) = 165580141 */ 
	int actual_result = 165580141;
	return ( actual_result == possible_result );
}
