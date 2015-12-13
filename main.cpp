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

#define MAX_ERROR 3500

#define MAX_RETRY 3
#define WAIT_TIME 5
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

void set_cpu_affinity(cpu_set_t mask, pid_t pid, int &mycpu, int &othercpu){
  /* CPU_ZERO initializes all the bits in the mask to zero. */ 
  CPU_ZERO( &mask );
  if(pid==0){
    //set mask such that this process can only be scheduled on CPU0
    CPU_SET(0, &mask);
    if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 )
        printf("WARNING: Could not set CPU Affinity, continuing...\n");
    mycpu = 0;
    othercpu = 1;  
  } 
    else{

   //set mask such that this process can only be scheduled on CPU1
    CPU_SET(1, &mask); 
    if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 )
        printf("WARNING: Could not set CPU Affinity, continuing...\n");
    mycpu=1;
    othercpu = 0;
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
  pid = fork();


/* CPU_ZERO initializes all the bits in the mask to zero. */ 
  set_cpu_affinity(mask, pid, mycpu, othercpu);

  // CPU_ZERO( &mask );
  // if(pid==0){
  //   //set mask such that this process can only be scheduled on CPU0
  //   CPU_SET(0, &mask);
  //   if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 )
  //       printf("WARNING: Could not set CPU Affinity, continuing...\n");
  //   mycpu = 0;
  //   othercpu = 1;  
  // } 
  //   else{

  //  //set mask such that this process can only be scheduled on CPU1
  //   CPU_SET(1, &mask); 
  //   if( sched_setaffinity( pid, sizeof(mask), &mask ) == -1 )
  //       printf("WARNING: Could not set CPU Affinity, continuing...\n");
  //   mycpu=1;
  //   othercpu = 0;
  // }  

//both threads run this code
segment:
  done_seg[mycpu] = 0;
  done[mycpu]=0;
  wait_count = 0;
  to_retry = 0;
  seg = segment_image(input, sigma, k, min_size, &num_ccs); 
  canny_in = image_to_pixel_t(seg, width, height);
//chile copies results to shared memory for comparison later
  if(mycpu==0){
    for(int i=0;i<width*height-5;i++)
      canny_child[i] = canny_in[i];
    for(int i=width*height-5;i<width*height;i++)
      canny_child[i] = 7;
  }
  // else
      // sleep(6);
  done_seg[mycpu]=1;
  printf("CPU %d done with seg\n", mycpu);

//check to make sure other is not hanging
  while(done_seg[othercpu]==0){
    sleep(1);
    wait_count++;
    if(wait_count == WAIT_TIME){
      // if(retries <= MAX_RETRY)
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
      pid = fork();
      set_cpu_affinity(mask, pid, mycpu, othercpu);
      goto segment_done;
    }else{

    pid = fork();
    set_cpu_affinity(mask, pid, mycpu, othercpu);
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
    
    printf("%d errors detected in canny\n", errors);
    if(errors >= MAX_ERROR){
      retries++;
      if(retries <= MAX_RETRY){
        done[mycpu] = -1;
        goto segment;
      }
      done[mycpu] = 1;
    }
  } else{
    while(done[othercpu] == 0)
      ;
    if(done[othercpu] == -1)
      goto segment;
  }

segment_done:
  printf("%d retries for segment\n", retries);

canny:
  done_canny[mycpu] = 0;
  done[mycpu]=0;
  wait_count = 0;
  to_retry = 0;

//checkpoint here
  if(mycpu==0){

    seg = segment_image(input, sigma, k, min_size, &num_ccs); 
    canny_in = image_to_pixel_t(seg, width, height);
    for(int i=0;i<width*height;i++)
      canny_child[i] = canny_in[i];
    done_seg[mycpu] = 1;
    printf("Done with seg\n");
    

    ellipse_in = canny_edge_detection(canny_in, width, height, 45, 50, 1.5f);
    for(int i=0;i<width*height;i++)
      ellipse_child[i] = ellipse_in[i];
    done_canny[mycpu] = 1;
    printf("Done with canny\n");

    printf("b. CPU %d is done processing image\n", mycpu);

    // for(int i=0;i<width*height;i++)
    //   seg_child[i] = seg[i];




    // printf("b. Done array in CPU %d: %d %d\n", mycpu, done[0], done[1]);
  //   while(done[othercpu] != 1){
  //     if(done[othercpu] < 0)
  //       goto segment_image;
  //   }
  }else{

    seg = segment_image(input, sigma, k, min_size, &num_ccs); 
    canny_in = image_to_pixel_t(seg, width, height);
    ellipse_in = canny_edge_detection(canny_in, width, height, 45, 50, 1.5f);
    done[mycpu] = 1;
    printf("b. CPU %d is done processing image\n", mycpu);
    printf("b. Waiting for CPU %d\n", othercpu);

    printf("b. Done array in CPU %d: %d %d\n", mycpu, done[0], done[1]);
    // while(done[othercpu] != 1){
    //   ;
    // }
    // printf("b. CPU %d done\n", othercpu);
    // int errors = 0;
    // for(int x=0; x<width;x++)
    //   for(int y=0; y<height; y++){
    //     if(imRef(input,x,y).r != imRef(input_child,x,y).r)
    //       errors++;
    //     if(imRef(input,x,y).g != imRef(input_child,x,y).g)
    //       errors++;
    //     if(imRef(input,x,y).b != imRef(input_child,x,y).b)
    //       errors++;
    // }
    // printf("%d errors detected\n", errors);

  }
  // image<rgb> *input = loadPPM(argv[1]); 
  // const int width = input->width();
  // const int height = input->height();  


  // printf("Process: %d is processing\n", pid);

  // image<rgb> *seg = segment_image(input, sigma, k, min_size, &num_ccs); 
  // pixel_t* canny_in = image_to_pixel_t(seg, width, height);
  // pixel_t *ellipse_in = canny_edge_detection(canny_in, width, height, 45, 50, 1.5f);

  if(mycpu==0){
    //child 
    printf("blooh blah\n");
    // savePPM(seg, "canny_in_child.ppm");
    savePPM3(canny_child, width, height, "canny2_in_child.ppm");

    savePPM3(ellipse_child, width, height, "ellipse_in_child.ppm");
    done_writing[mycpu]=1;
    printf("CPU %d created files canny_in_child.ppm and ellipse_in_child.ppm\n", mycpu);
 

  } else{
    //parent 
    savePPM3(ellipse_in, width, height, "ellipse_in.ppm");
    savePPM3(canny_in, width, height, "canny2_in.ppm");

    // savePPM(seg, "canny_in.ppm");
    printf("CPU %d created files canny_in.ppm and ellipse_in.ppm\n", mycpu);

  }  

//first process to finish will check if the other is done
  sleep(5);
  done[mycpu] = 1;
  // if(done[othercpu]==0){
  //   printf("CPU %d not done yet, lets wait\n", othercpu);
  //   sleep(WAIT_TIME);
  //   if(done[othercpu]==0){ //if still not done, we kill it
  //     printf("CPU %d hanging\n", othercpu);
  //     printf("Attempting to kill PID: %d\n", pids[othercpu]);
      
  //     if(pid==0){ //if child, kill parent
  //       kill(getppid(), SIGTERM);
  //       printf("Parent killed\n");
  //     }
  //     else{ //if parent, kill child
  //       kill(pid, SIGTERM);
  //       printf("Child killed\n");
  //     }

  //   }
  // }


//synchronize here (both should be done)
  if(mycpu==1)
  {
    printf("Waiting for CPU %d to finish\n", othercpu);
    
    while(done[othercpu]!=1){
      sleep(1);
      printf("Waiting for CPU %d to finish\n", othercpu);
    }
    int errors = 0;
    for(int i=0; i<width*height;i++)
      if(canny_in[i]!=canny_child[i])
        errors++;
    
    printf("%d errors detected in canny\n", errors);

    errors = 0; 
    for(int i=0; i<width*height;i++)
      if(ellipse_in[i]!=ellipse_child[i])
        errors++;
    
    printf("%d errors detected in ellipse\n", errors);
  }

  if(mycpu==1){
    free (input);
  }

  free (canny_in);
  free (ellipse_in);
  free (seg);
  
  printf("--Done!--\n");

  if(mycpu==1){
    printf("Has CPU %d finished writing? Done: %d\n", othercpu, done[othercpu]);
    while(done[othercpu]!=1){
      sleep(1);
      printf("Waiting for CPU %d to finish writing\n", othercpu);
    }
    kill(pid, SIGKILL);
  }
    else
  sleep(10);
  //checking to make sure that if parent/child is killed due to hanging, other can still fork and continue with computation 
  pid = fork();
  printf("respawn\n");
  return 0;
}

