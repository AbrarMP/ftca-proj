/*
Copyright (C) 2006 Pedro Felzenszwalb

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

// #include <cstdio>
// #include <cstdlib>

#include <stdio.h>
#include <stdlib.h>
#include <image.h>
#include <misc.h>
#include <pnmfile.h>
#include "segment-image.h"
#include <iostream>
#include <fstream>
#include "canny.h"
#include <unistd.h>


#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <errno.h>
 

#include <sys/mman.h>
#include <sched.h>
#include <ctype.h>
#include <string.h>

#define TRUE  1
#define FALSE 0 

#define WAIT_TIME 6
// typedef int bool;
int *done;
int *pids;

typedef short int pixel_t;
static pixel_t* image_to_pixel_t(image<rgb>* im, int width, int height)
{
  pixel_t *out = (pixel_t*)malloc(width*height*sizeof(pixel_t));
  for (int y=0;y<height;y++)
  {
    for(int x=0;x<width;x++)
    {
        out[x+width*y] = imRef(im,x,y).r;
    }
  }
  return out;
}

void savePPM3(const pixel_t* data, int width, int height, const char *name) {

    std::ofstream out;
    out.open(name);
  
    out << "P3\n" << width << " " << height << "\n" << UCHAR_MAX << "\n";
  // fprintf(file, "P3\n%d %d\n%d\n", width, height, UCHAR_MAX);// file << "P3\n" << width << " " << height << "\n" << UCHAR_MAX << "\n";
  for (int i = 0; i < width*height; i++) {
      out << data[i] << "\n";
      out << data[i] << "\n";
      out << data[i] << "\n";

    }
  }

void hang(int a){
  double array[2048];
  for(int i = 0; i < 2048;i++)
    array[i] = (double)i;
  for(int z=0;z<a;z++)
    for(int k=0;k<2048;k++)
      for(int l=0;l<2048;l++)
        for(int j=0; j<2048;j++)
          for(int i =0;i<2048;i++)
            array[i]+=j;
}


int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s input(ppm) \n", argv[0]);
    return 1;
  }
  printf("--beginning of program\n");
  int NUM_PROCS = sysconf(_SC_NPROCESSORS_CONF);
  printf("System has %i processor(s).\n", NUM_PROCS);

  //parameters for image segmentation
  float sigma = 0.5;
  float k = 500.0;
  int min_size = 35000;

//variables to set up shared memory pointer
  int shmid;
  int *shm_ptr;
  key_t key=12345;

//setting up the shared memory pointer to 'done' variable
  shmid=shmget(key,4*sizeof(int),0666|IPC_CREAT);
   
  if (shmid < 0)
  {
      perror("shmget");
      exit(1);
  }
   
  shm_ptr=(int *)shmat(shmid,(void *)0,0);
   
  if (shm_ptr == (int *)(-1))
  {
      perror("shmat:shm_ptr");
      exit(1);
  }

  done = shm_ptr;
  pids = shm_ptr+2;
  pids[0] = pids[1] = 1;

	int mycpu = 0;
  int othercpu = 0;
//processes fork here
  pid_t pid = fork();

  cpu_set_t mask;

/* CPU_ZERO initializes all the bits in the mask to zero. */ 
  CPU_ZERO( &mask );


  if(pid==0){
    //set mask such that this process can only be scheduled on CPU0
    CPU_SET(0, &mask);
    if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 ){
        printf("WARNING: Could not set CPU Affinity, continuing...\n");
    }
    mycpu = 0;
    othercpu = 1;
    done[mycpu] = 0;
    while(pids[othercpu] == 0){
      ;
    }
    pids[mycpu] = pids[othercpu]-1;
    printf("child pid is %d\n", pids[mycpu]);
    
  } 
    else{

   //set mask such that this process can only be scheduled on CPU1
    CPU_SET(1, &mask); 
    if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 ){
        printf("WARNING: Could not set CPU Affinity, continuing...\n");
    }
    mycpu=1;
    othercpu = 0;
    done[mycpu] = 0;
    pids[mycpu] = pid;
    printf("parent pid is %d\n", pids[mycpu]);
  }  


//both threads run this code
  printf("Process: %d is loading input image.\n", pid);
  image<rgb> *input = loadPPM(argv[1]);

  const int width = input->width();
  const int height = input->height();	
  printf("Process: %d is processing\n", pid);
  int num_ccs; 
  image<rgb> *seg = segment_image(input, sigma, k, min_size, &num_ccs); 
  pixel_t* canny_in = image_to_pixel_t(seg, width, height);
  pixel_t *ellipse_in = canny_edge_detection(canny_in, width, height, 45, 50, 1.5f);


  if(pid==0){
    //child will hang
    // hang(1);
    savePPM3(ellipse_in, width, height, "ellipse_in_child.ppm");
    savePPM(seg, "canny_in_child.ppm");
    printf("CPU %d created files canny_in_child.ppm and ellipse_in_child.ppm\n", mycpu);
 

  } else{
    //parent will hang
    hang(1);
    savePPM3(ellipse_in, width, height, "ellipse_in.ppm");
    savePPM(seg, "canny_in.ppm");
    printf("CPU %d created files canny_in.ppm and ellipse_in.ppm\n", mycpu);

  }  

//first process to finish will check if the other is done
  done[mycpu] = 1;
  printf("CPU %d done\n", mycpu);

  if(done[othercpu]==0){
    // printf("CPU %d not done yet, lets wait\n", othercpu);
    sleep(WAIT_TIME);
    if(done[othercpu]==0){ //if still not done, we kill it
      printf("CPU %d hanging\n", othercpu);
      printf("Attempting to kill PID: %d\n", pids[othercpu]);
      
      if(pid==0){ //if child, kill parent
        kill(getppid(), SIGTERM);
      }
      else //if parent, kill child
        kill(pid, SIGTERM);
    }
  }

  free (seg);
  free (input);
  free (canny_in);
  free (ellipse_in);
  printf("--Done!--\n");

  //checking to make sure that if parent/child is killed due to hanging, other can still fork and continue with computation 
  pid = fork();
  printf("respawn\n");
  return 0;
}

