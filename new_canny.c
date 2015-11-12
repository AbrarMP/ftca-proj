#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <iostream>
#include <fstream>
#define MAX_BRIGHTNESS 255
 
// C99 doesn't define M_PI (GNU-C99 does)
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif
/*
 * Loading part taken from
 * http://www.vbforums.com/showthread.php?t=261522
 * BMP info:
 * http://en.wikipedia.org/wiki/BMP_file_format
 *
 * Note: the magic number has been removed from the bmpfile_header_t
 * structure since it causes alignment problems
 *     bmpfile_magic_t should be written/read first
 * followed by the
 *     bmpfile_header_t
 * [this avoids compiler-specific alignment pragmas etc.]
 */
 // using namespace std;
 typedef short int pixel_t;

 static short int* loadPPM3(const char *name, int &width, int& height){
    
    char type[3] ;
    std::ifstream in;
    in.open(name);
      
    in >> type;
    std::cout << type << "\n";
    if(type[0] != 'P' || type[1] != '3'){
      fprintf(stderr, "Not a valid PPM3 file\n");
      std::cout << type;
      in.close();
      return NULL;
    }

    in >> width >> height;
    int max;
    in >> max;
    pixel_t* data = new pixel_t[width*height];
    for(int i=0;i<width*height*3;i+=3)
    	in >> data[i];

    return data;
  
}

void convolution(const pixel_t *in, pixel_t *out, const float *kernel,
                 const int nx, const int ny, const int kn,
                 const bool normalize)
{
    assert(kn % 2 == 1);
    assert(nx > kn && ny > kn);
    const int khalf = kn / 2;
    float min = FLT_MAX, max = -FLT_MAX;
 
    if (normalize)
    	// iterate through the image's pixel elements while keeping within boundaries
    	// (i.e. 3x3 kernel won't operate on edges)
        for (int m = khalf; m < nx - khalf; m++)
            for (int n = khalf; n < ny - khalf; n++) {
                float pixel = 0.0;
                size_t c = 0;

                // iterate through kernel elements and get convolution value
                for (int j = -khalf; j <= khalf; j++)
                    for (int i = -khalf; i <= khalf; i++) {
                        pixel += in[(n - j) * nx + m - i] * kernel[c];
                        c++;
                    }

                // record min and max for normalization purposes
                if (pixel < min)
                    min = pixel;
                if (pixel > max)
                    max = pixel;
                }
 
    // iterate through th eimage's pixel elements while keeping within boundaries
    for (int m = khalf; m < nx - khalf; m++)
        for (int n = khalf; n < ny - khalf; n++) {
            float pixel = 0.0;
            size_t c = 0;

            // iterate through kernel elements and get convolution value
            for (int j = -khalf; j <= khalf; j++)
                for (int i = -khalf; i <= khalf; i++) {
                    pixel += in[(n - j) * nx + m - i] * kernel[c];
                    c++;
                }
 
            // normalize value if enabled
            if (normalize)
                pixel = MAX_BRIGHTNESS * (pixel - min) / (max - min);
            out[n * nx + m] = (pixel_t)pixel;
        }
}
 
/*
 * gaussianFilter:
 * http://www.songho.ca/dsp/cannyedge/cannyedge.html
 * determine size of kernel (odd #)
 * 0.0 <= sigma < 0.5 : 3
 * 0.5 <= sigma < 1.0 : 5
 * 1.0 <= sigma < 1.5 : 7
 * 1.5 <= sigma < 2.0 : 9
 * 2.0 <= sigma < 2.5 : 11
 * 2.5 <= sigma < 3.0 : 13 ...
 * kernelSize = 2 * int(2*sigma) + 3;
 */
void gaussian_filter(const pixel_t *in, pixel_t *out,
                     const int nx, const int ny, const float sigma)
{
    const int n = 2 * (int)(2 * sigma) + 3;
    const float mean = (float)floor(n / 2.0);
    float kernel[n * n]; // variable length array
 
    fprintf(stderr, "gaussian_filter: kernel size %d, sigma=%g\n",
            n, sigma);
    size_t c = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            kernel[c] = exp(-0.5 * (pow((i - mean) / sigma, 2.0) +
                                    pow((j - mean) / sigma, 2.0)))
                        / (2 * M_PI * sigma * sigma);
            c++;
        }
 
    convolution(in, out, kernel, nx, ny, n, true);
}


pixel_t *canny_edge_detection(const pixel_t *in,
                              const int width, const int height,
                              const int tmin, const int tmax,
                              const float sigma)
{
    const int nx = width;
    const int ny = height;
 
    pixel_t *G = calloc(nx * ny * sizeof(pixel_t), 1);
    pixel_t *after_Gx = calloc(nx * ny * sizeof(pixel_t), 1);
    pixel_t *after_Gy = calloc(nx * ny * sizeof(pixel_t), 1);
    pixel_t *nms = calloc(nx * ny * sizeof(pixel_t), 1);
    pixel_t *out = malloc(width*height* sizeof(pixel_t));
 
    if (G == NULL || after_Gx == NULL || after_Gy == NULL ||
        nms == NULL || out == NULL) {
        fprintf(stderr, "canny_edge_detection:"
                " Failed memory allocation(s).\n");
        exit(1);
    }
 
    gaussian_filter(in, out, nx, ny, sigma);
 

    const float Gx[] = {-1, 0, 1,
                        -2, 0, 2,
                        -1, 0, 1};
 
    convolution(out, after_Gx, Gx, nx, ny, 3, false);
 
//    const float Gy[] = { 1, 2, 1,
//                         0, 0, 0,
//                        -1,-2,-1};

    const float Gy[] = {-1,-2,-1,
                         0, 0, 0,
                         1, 2, 1};
 
    convolution(out, after_Gy, Gy, nx, ny, 3, false);
 
    // Find the intensity gradient of the image
    for (int i = 1; i < nx - 1; i++)
        for (int j = 1; j < ny - 1; j++) {
            const int c = i + nx * j;
            // G[c] = abs(after_Gx[c]) + abs(after_Gy[c]);
            G[c] = (pixel_t)hypot(after_Gx[c], after_Gy[c]);
        }


    // Non-maximum suppression, straightforward implementation.
    for (int i = 1; i < nx - 1; i++)
        for (int j = 1; j < ny - 1; j++) {
            const int c = i + nx * j;
            const int nn = c - nx;
            const int ss = c + nx;
            const int ww = c + 1;
            const int ee = c - 1;
            const int nw = nn + 1;
            const int ne = nn - 1;
            const int sw = ss + 1;
            const int se = ss - 1;
 
            // Determine the edge direction
            // range of atan2: -pi < atan2(y,x) <= pi
            const float dir = (float)(fmod(atan2(after_Gy[c],
                                                 after_Gx[c]) + M_PI,
                                           M_PI) / M_PI) * 8;

            if (((dir <= 1 || dir > 7) && G[c] > G[ee] &&
                 G[c] > G[ww]) || // 0 deg
                ((dir > 1 && dir <= 3) && G[c] > G[ne] &&
                 G[c] > G[sw]) || // 45 deg
                ((dir > 3 && dir <= 5) && G[c] > G[nn] &&
                 G[c] > G[ss]) || // 90 deg
                ((dir > 5 && dir <= 7) && G[c] > G[nw] &&
                 G[c] > G[se]))   // 135 deg
                nms[c] = G[c];
            else
                nms[c] = 0;
        }

 
    // Reuse array
    // used as a stack. nx*ny/2 elements should be enough.
    int *edges = (int*) after_Gy;
    memset(out, 0, sizeof(pixel_t) * nx * ny);
    memset(edges, 0, sizeof(pixel_t) * nx * ny);
 
    // Tracing edges with hysteresis . Non-recursive implementation.
    size_t c = 1;
    for (int j = 1; j < ny - 1; j++)
        for (int i = 1; i < nx - 1; i++) {
            if (nms[c] >= tmax && out[c] == 0) { // trace edges
                out[c] = MAX_BRIGHTNESS;
                int nedges = 1;
                edges[0] = c;
 
                do {
                    nedges--;
                    const int t = edges[nedges];
 
                    int nbs[8]; // neighbours
                    nbs[0] = t - nx;     // nn
                    nbs[1] = t + nx;     // ss
                    nbs[2] = t + 1;      // ww
                    nbs[3] = t - 1;      // ee
                    nbs[4] = nbs[0] + 1; // nw
                    nbs[5] = nbs[0] - 1; // ne
                    nbs[6] = nbs[1] + 1; // sw
                    nbs[7] = nbs[1] - 1; // se
 
                    for (int k = 0; k < 8; k++)
                        if (nms[nbs[k]] >= tmin && out[nbs[k]] == 0) {
                            out[nbs[k]] = MAX_BRIGHTNESS;
                            edges[nedges] = nbs[k];
                            nedges++;
                        }
                } while (nedges > 0);
            }
            c++;
        }
 
    free(after_Gx);
    free(after_Gy);
    free(G);
    free(nms);
 
    return out;
}
 int main(const int argc, const char ** const argv)
{
    if (argc < 2) {
        printf("Usage: %s image.ppm\n", argv[0]);
        return 1;
    }
 
    static int width, height;

    const pixel_t *pixel_data = loadPPM3(argv[1], width, height);
    if (pixel_data == NULL) {
        fprintf(stderr, "main: PPM3 image not loaded.\n");
        return 1;
    }
 	

    const pixel_t *out_data =
        canny_edge_detection(pixel_data, width, height, 45, 50, 1.0f);
    // if (out_bitmap_data == NULL) {
    //     fprintf(stderr, "main: failed canny_edge_detection.\n");
    //     return 1;
    // }

    // end_time = clock();
    // double duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    // printf("Duration: %f\n", duration);

    // /*
    // const pixel_t *out_bitmap_data = load_bmp(argv[1], &ih);
    // if (out_bitmap_data == NULL) {
    //     fprintf(stderr, "main: BMP image not loaded.\n");
    //     return 1;
    // }
    // */
    // if (save_bmp("out.bmp", &ih, out_bitmap_data)) {
    //     fprintf(stderr, "main: BMP image not saved.\n");
    //     return 1;
    // }

 
    free((pixel_t*)pixel_data);
    // free((pixel_t*)out_bitmap_data);
    return 0;
}
