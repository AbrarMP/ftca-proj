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

#define MAX_ERROR 300

#define MAX_RETRY 2
#define WAIT_TIME 4
// typedef int bool;

typedef short int pixel_t;

int *done;
int* done_seg;
int* done_canny;
int* done_writing;
int *pids;
image<rgb> *input_child, *seg_child;
pixel_t *canny_child, *ellipse_child;




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

void setup_shm(int width, int height){

//variables to set up shared memory pointer
  int shmid;
  int *shm_ptr;
  key_t key=12345;

//setting up the shared memory pointer to 'done' variable
  shmid=shmget(key,8*sizeof(int),0666|IPC_CREAT);
   
  if (shmid < 0){
      perror("shmget");
      exit(1);
  }
  shm_ptr=(int *)shmat(shmid,(void *)0,0);
   
  if (shm_ptr == (int *)(-1)){
      perror("shmat:shm_ptr");
      exit(1);
  }

  done = shm_ptr;
  done_seg = shm_ptr+2;
  done_canny = shm_ptr+4;
  done_writing = shm_ptr+6;


//also need to get shared memory pointer for child data structures:
//namely image<rgb> input, image<rbg> segment, pixel_t canny_in, pixel_t ellipse_in
  int shmid_input, shmid_seg, shmid_canny, shmid_ellipse;
  image<rgb> *shm_ptr_seg, *shm_ptr_input;
  pixel_t *shm_ptr_canny, *shm_ptr_ellipse;
  key_t key_seg = 12346;
  key_t key_canny = 12347;
  key_t key_ellipse = 12348;
  key_t key_input = 12349;

  shmid_input = shmget(key_input, width*height*sizeof(image<rgb>),0666|IPC_CREAT);
  if(shmid_input<0){
    perror("shmget input");
    exit(1);
  }
  shm_ptr_input=(image<rgb> *)shmat(shmid_input,(void *)0,0);

  if(shm_ptr_input == (image<rgb> *)(-1)){
    perror("shmat:shm_ptr_input");
    exit(1);
  }
  input_child = shm_ptr_input;


  shmid_seg = shmget(key_seg, width*height*sizeof(image<rgb>),0666|IPC_CREAT);
  if(shmid_seg<0){
    perror("shmget seg");
    exit(1);
  }
  shm_ptr_seg=(image<rgb> *)shmat(shmid_seg,(void *)0,0);

  if(shm_ptr_seg == (image<rgb> *)(-1)){
    perror("shmat:shm_ptr_seg");
    exit(1);
  }
  seg_child = shm_ptr_seg;
  

  shmid_canny = shmget(key_canny, width*height*sizeof(pixel_t),0666|IPC_CREAT);
  if(shmid_canny<0){
    perror("shmget canny");
    exit(1);
  }
  shm_ptr_canny=(pixel_t *)shmat(shmid_canny,(void *)0,0);

  if(shm_ptr_canny == (pixel_t *)(-1)){
    perror("shmat:shm_ptr_canny");
    exit(1);
  }
  canny_child = shm_ptr_canny;


  shmid_ellipse = shmget(key_ellipse, width*height*sizeof(pixel_t),0666|IPC_CREAT);
  if(shmid_ellipse<0){
    perror("shmget ellipse");
    exit(1);
  }
  shm_ptr_ellipse=(pixel_t *)shmat(shmid_ellipse,(void *)0,0);

  if(shm_ptr_ellipse == (pixel_t *)(-1)){
    perror("shmat:shm_ptr_ellipse");
    exit(1);
  }
  ellipse_child = shm_ptr_ellipse;
}

void set_cpu_affinity(cpu_set_t &mask, pid_t pid, int &mycpu, int &othercpu){
  /* CPU_ZERO initializes all the bits in the mask to zero. */ 
  CPU_ZERO( &mask );
  if(pid==0){
    //set mask such that this process can only be scheduled on CPU0
    mycpu = 0;
    othercpu = 1;
    CPU_SET(mycpu, &mask);
    if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 )
        printf("WARNING: Could not set CPU Affinity, continuing...\n");
    mycpu = 0;
    othercpu = 1;  
  } 
    else{
    mycpu=1;
    othercpu = 0;
   //set mask such that this process can only be scheduled on CPU1
    CPU_SET(mycpu, &mask); 
    if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 )
        printf("WARNING: Could not set CPU Affinity, continuing...\n");

  } 
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

  image<rgb> *input, *seg;
  pixel_t * canny_in, *ellipse_in;
  int num_ccs; 
  int width, height;
  input = loadPPM(argv[1]); 
  width = input->width();
  height = input->height(); 

  setup_shm(width, height);
  for(int i=0;i<8;i++)
    done[i]=0;

  int mycpu = 0;
  int othercpu = 0;
  int wait_count = 0;
  int retries = 0;
  bool killed = 0;
  bool to_retry = 0;
  int errors = 0;

  cpu_set_t mask;
//processes fork here
  pid_t pid;

  retries=0;


//both threads run this code
segment:
  pid = fork();
/* CPU_ZERO initializes all the bits in the mask to zero. */ 
  set_cpu_affinity(mask, pid, mycpu, othercpu);
  printf("---PID %d with MYCPU %d is running on CPU %d---\n", pid, mycpu, (CPU_ISSET(mycpu, &mask)==1)?mycpu:200);

 
  done_seg[mycpu] = 0;
  done[mycpu]=0;
  wait_count = 0;
  to_retry = 0;
  seg = segment_image(input, sigma, k, min_size, &num_ccs); 
  canny_in = image_to_pixel_t(seg, width, height);
//chile copies results to shared memory for comparison later
  if(mycpu==0){
    for(int i=0;i<width*height;i++)
      canny_child[i] = canny_in[i];
    // hang(1);
  }
  else;// hang(1);
  done_seg[mycpu]=1;
  printf("CPU %d done with seg\n", mycpu);

//check to make sure other is not hanging
  while(done_seg[othercpu]==0){
    sleep(1);
    wait_count++;
    if(wait_count == WAIT_TIME){
      to_retry = 1;
      retries++;
      printf("CPU %d is hanging\n", othercpu);
      if(mycpu==0) { //if child, kill parent
        kill(getppid(), SIGTERM);
        printf("CPU %d:parent, killed\n", othercpu);
        break;
      }else{ //if parent, kill child
        kill(pid, SIGTERM);
        printf("CPU %d:child, killed\n", othercpu);
        break;
      }
    }
  }

//after a process has hanged and been killed
  if(to_retry == 1){

    if(retries > MAX_RETRY){ //no more retrying, just skip
      goto segment_done;

    }else{
      goto segment;
    }
  }

//data mismatch condition/both CPU's still up
  if(mycpu==1){
    to_retry = 0;
    errors = 0;
    for(int i=0; i<width*height;i++)
      if(canny_in[i]!=canny_child[i])
        errors++;
    
    printf("%d errors detected in segment\n", errors);
    kill(pid,SIGTERM);
    
    if(errors >= MAX_ERROR){
      retries++;
      if(retries < MAX_RETRY){
        done[mycpu] = -1;
        goto segment;
      }
      done[mycpu] = 1;
    }

  } 
    else
      while(1);
  

segment_done:
  printf("%d retries for segment\n", retries);
  retries=0;
  // set_cpu_affinity(mask, pid, mycpu, othercpu);


canny:
  pid = fork();
  set_cpu_affinity(mask, pid, mycpu, othercpu);
  printf("---PID %d with MYCPU %d is running on CPU %d---\n", pid, mycpu, (CPU_ISSET(mycpu, &mask)==1)?mycpu:200);

  done_canny[mycpu] = 0;
  done[mycpu]=0;
  wait_count = 0;
  to_retry = 0;
  // printf("MYPID is %d and MYCPU is %d and OTHERCPU is %d\n", pid, mycpu, othercpu);
  ellipse_in = canny_edge_detection(canny_in, width, height, 45, 50, 1.5f);
//child copies results to shared memory for comparison later
  if(mycpu==0){
    for(int i=0;i<width*height;i++)
      ellipse_child[i] = ellipse_in[i];
  } else;// hang(1);

  done_canny[mycpu]=1;
  printf("CPU %d done with canny\n", mycpu);

//check to make sure other is not hanging
  while(done_canny[othercpu]==0){
    sleep(1);
    wait_count++;
    if(wait_count == WAIT_TIME){
      to_retry = 1;
      retries++;
      printf("CPU %d is hanging\n", othercpu);
      if(mycpu==0) { //if child, kill parent
        kill(getppid(), SIGTERM);
        printf("CPU %d:parent, killed\n", othercpu);
        break;
      }else{ //if parent, kill child
        kill(pid, SIGTERM);
        printf("CPU %d:child, killed\n", othercpu);
        break;
      }
    }
  }

//after a process has hanged and been killed
  if(to_retry == 1){

    if(retries > MAX_RETRY){ //no more retrying, just skip
      goto canny_done;
    }else{
      goto canny;
    }
  }

//data mismatch condition/both CPU's still up
  if(mycpu==1){
    to_retry = 0;
    errors = 0;
    for(int i=0; i<width*height;i++)
      if(ellipse_in[i]!=ellipse_child[i])
        errors++;
    
    printf("%d errors detected in canny\n", errors);
    kill(pid,SIGTERM);

    if(errors >= MAX_ERROR){
      retries++;
      if(retries < MAX_RETRY){
        done[mycpu] = -1;
        goto canny;
      }
      done[mycpu] = 1;
    }
  } else
      while(1);
      
    

canny_done:
  printf("%d retries for canny\n", retries);



write_output:
  pid = fork();
  set_cpu_affinity(mask, pid, mycpu, othercpu);

  if(mycpu==0){
    //child 
    savePPM3(canny_child, width, height, "segment0.ppm");

    savePPM3(ellipse_child, width, height, "canny0.ppm");
    done_writing[mycpu]=1;
    printf("CPU %d created files segment0.ppm and canny0.ppm\n", mycpu);
 

  } else{
    //parent 
    savePPM3(canny_in, width, height, "segment1.ppm");
    savePPM3(ellipse_in, width, height, "canny1.ppm");

    printf("CPU %d created files segment1.ppm and canny1.ppm\n", mycpu);
    done_writing[mycpu] = 1;

  }  


  if(mycpu==1){
    free (input);
    // free(canny_child);
    // free(seg_child);
    // free(ellipse_child);
  }
  // else

  free (canny_in);
  free (ellipse_in);
  free (seg);
  


  if(mycpu==1){
    printf("Has CPU %d finished writing? Done: %d\n", othercpu, done_writing[othercpu]);
    while(done_writing[othercpu]!=1){
      sleep(1);
      printf("Waiting for CPU %d to finish writing\n", othercpu);
    }
    kill(pid, SIGKILL);
  }
  else
    while(1);
    // else
  // sleep(10);
  //checking to make sure that if parent/child is killed due to hanging, other can still fork and continue with computation 
  
  printf("--Done!--\n");
  // pid = fork();
  // printf("respawn\n");
  // set_cpu_affinity(mask, pid, mycpu, othercpu);
  // printf("---PID %d with MYCPU %d is running on CPU %d---\n", pid, mycpu, (CPU_ISSET(mycpu, &mask)==0)?mycpu:200);
  return 0;
}

