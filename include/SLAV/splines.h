#ifndef _H_SPLINES
#define _H_SPLINES

#include <stdint.h>
#include "bmp.h"

#define OVERLAY_ALWAYS 0
#define OVERLAY_WHEN 1
#define OVERLAY_WHENNOT 2

void draw_line(Bitmap* bmp, uint32_t color, float x0, float y0, float x1, float y1); /* Draw a line of color from (x0, y0) to (x1, y1) */
void put_bezier(Bitmap* bmp, int samples, double gain); /* Draw a random bezier-spline of specified gain and samples of indexed color 1 */
void put_cr(Bitmap* bmp, int samples, double gain); /* Draw a random Catmull-Rom spline of indexed color 1 */
void line_fill(Bitmap* bmp, int x, int y, uint32_t fill, int32_t replace); /* Bucket fill starting at (x, y) that replaces all contiguous pixels of replace with fill
																																						(or all colors BUT check, if negative */
void outline(Bitmap* bmp, uint32_t target, uint32_t border, uint32_t color); /* Set all pixels that border the color target and have color border to color */
Bitmap* random_element(int width, int height, int layers, double gain, uint32_t bg); /* Create a random bitmap of splines and fillings */
void overlay_element(Bitmap *dst, Bitmap *src, int xoff, int yoff, int mode, uint32_t search); /* Overlay one bitmap over another for certain pixels */

typedef struct _splineimg_t{
    Bitmap** layers;
    int* modes;
    uint32_t* keys;
    double* offsets;
    int len;
} splineimg_t;

// void draw_spline_sprites(int, int, char*, const float*);

#endif