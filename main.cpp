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


  image<rgb> *input = loadPPM(argv[1]); 
  const int width = input->width();
  const int height = input->height(); 
  image<rgb> *seg;
  pixel_t * canny_in, *ellipse_in;

//variables to set up shared memory pointer
  int shmid;
  int *shm_ptr;
  key_t key=12345;

//setting up the shared memory pointer to 'done' variable
  shmid=shmget(key,2*sizeof(int),0666|IPC_CREAT);
   
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


//also need to get shared memory pointer for child data structures:
//namely image<rgb> input, image<rbg> segment, pixel_t canny_in, pixel_t ellipse_in
  int shmid_seg, shmid_canny, shmid_ellipse;
  image<rgb> *shm_ptr_seg;
  pixel_t *shm_ptr_canny, *shm_ptr_ellipse;
  key_t key_seg = 12346;
  key_t key_canny = 12347;
  key_t key_ellipse = 12348;

  shmid_seg = shmget(key_seg, width*height*sizeof(image<rgb>),0666|IPC_CREAT);
  if(shmid<0){
    perror("shmget seg");
    exit(1);
  }
  shm_ptr_seg=(image<rgb> *)shmat(shmid,(void *)0,0);

  if(shm_ptr_seg == (image<rgb> *)(-1)){
    perror("shmat:shm_ptr_seg");
    exit(1);
  }
  image<rgb>* seg_child = shm_ptr_seg;
  

  shmid_canny = shmget(key_canny, width*height*sizeof(pixel_t),0666|IPC_CREAT);
  if(shmid<0){
    perror("shmget canny");
    exit(1);
  }
  shm_ptr_canny=(pixel_t *)shmat(shmid,(void *)0,0);

  if(shm_ptr_canny == (pixel_t *)(-1)){
    perror("shmat:shm_ptr_canny");
    exit(1);
  }
  pixel_t *canny_child = shm_ptr_canny;


  
  shmid_ellipse = shmget(key_ellipse, width*height*sizeof(pixel_t),0666|IPC_CREAT);
  if(shmid<0){
    perror("shmget ellipse");
    exit(1);
  }
  shm_ptr_ellipse=(pixel_t *)shmat(shmid,(void *)0,0);

  if(shm_ptr_ellipse == (pixel_t *)(-1)){
    perror("shmat:shm_ptr_ellipse");
    exit(1);
  }
  pixel_t *ellipse_child = shm_ptr_ellipse;
  

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
    // while(pids[othercpu] == 0){
    //   ;
    // }
    // pids[mycpu] = pids[othercpu]-1;
    // printf("child pid is %d\n", pids[mycpu]);
    
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
    // pids[mycpu] = pid;
    // printf("parent pid is %d\n", pids[mycpu]);
  }  


//both threads run this code
  printf("CPU %d is processing image.\n", mycpu);
  int num_ccs; 

segment_image:
//checkpoint here
  if(mycpu==0){
    seg_child = segment_image(input, sigma, k, min_size, &num_ccs); 
    canny_child = image_to_pixel_t(seg_child, width, height);
    ellipse_child = canny_edge_detection(canny_child, width, height, 45, 50, 1.5f);
    done[mycpu] = 1;
    printf("CPU %d is done processing image\n", mycpu);

    printf("Done array in CPU 0: %d %d\n", done[0], done[1]);
    while(done[othercpu] != 1){
      if(done[othercpu] < 0)
        goto segment_image;
    }
  } 
    else{

    seg = segment_image(input, sigma, k, min_size, &num_ccs); 
    canny_in = image_to_pixel_t(seg, width, height);
    ellipse_in = canny_edge_detection(canny_in, width, height, 45, 50, 1.5f);
    done[mycpu] = 1;
    printf("CPU %d is done processing image\n", mycpu);
    printf("Waiting for CPU %d\n", othercpu);

    printf("Done array in CPU 1: %d %d\n", done[0], done[1]);
    // while(done[othercpu] != 1){
    //   ;
    // }
    printf("CPU %d done\n", othercpu);
    int errors = 0;
    // for(int x=0; x<width;x++)
    //   for(int y=0; y<height; y++){
    //     if(imRef(input,x,y).r != imRef(input_child,x,y).r)
    //       errors++;
    //     if(imRef(input,x,y).g != imRef(input_child,x,y).g)
    //       errors++;
    //     if(imRef(input,x,y).b != imRef(input_child,x,y).b)
    //       errors++;
    // }
    printf("%d errors detected\n", errors);

  }
  // image<rgb> *input = loadPPM(argv[1]); 
  // const int width = input->width();
  // const int height = input->height();	


  // printf("Process: %d is processing\n", pid);

  // image<rgb> *seg = segment_image(input, sigma, k, min_size, &num_ccs); 
  // pixel_t* canny_in = image_to_pixel_t(seg, width, height);
  // pixel_t *ellipse_in = canny_edge_detection(canny_in, width, height, 45, 50, 1.5f);

  done[mycpu]=0;
  if(mycpu==0){
    //child 
    // hang(1);
    savePPM3(ellipse_child, width, height, "ellipse_in_child.ppm");
    savePPM(seg_child, "canny_in_child.ppm");
    printf("CPU %d created files canny_in_child.ppm and ellipse_in_child.ppm\n", mycpu);
 

  } else{
    //parent will hang
    // hang(1);
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

  if(mycpu==1)
  kill(pid, SIGKILL);
else
  sleep(10);
  //checking to make sure that if parent/child is killed due to hanging, other can still fork and continue with computation 
  pid = fork();
  printf("respawn\n");
  return 0;
}

