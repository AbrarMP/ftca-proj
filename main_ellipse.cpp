#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <unistd.h>




#include <errno.h>
 

#include <sched.h>
#include <ctype.h>
#include <string.h>

typedef short int pixel_t;


pixel_t* loadPPM3(int &width, int &height, const char *name) {

    std::ifstream in;
    in.open(name);
    char buff[10];
  
    in >>  buff;
    in >>  width;
    in >>  height;
    in >>  buff;
    pixel_t* data = new pixel_t[width*height];
  	for (int i = 0; i < width*height; i++) {
      in >> data[i];
      in >> data[i];
      in >> data[i];
    }
    return data;
  }

void savePPM3(const pixel_t* data, int width, int height, const char *name) {

    std::ofstream out;
    out.open(name);
  
    out << "P3\n" << width << " " << height << "\n" << 255 << "\n";
  // fprintf(file, "P3\n%d %d\n%d\n", width, height, UCHAR_MAX);// file << "P3\n" << width << " " << height << "\n" << UCHAR_MAX << "\n";
  for (int i = 0; i < width*height; i++) {
      out << data[i] << "\n";
      out << data[i] << "\n";
      out << data[i] << "\n";

    }
  }

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s input(ppm) \n", argv[0]);
    return 1;
  }
  printf("--beginning of program\n");

  int width, height;
  pixel_t * ellipse_in = loadPPM3(width, height, argv[1]);
  savePPM3(ellipse_in, width, height, "ellipse_out.ppm");
  free(ellipse_in);
  return 0;
}