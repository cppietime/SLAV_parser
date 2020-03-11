/*
Bitmap handling library. Only can handle BITMAPCOREHEADER and BITMAPINFOHEADER DIB types, as well as BI_RGB and
BI_BITFIELDS compression (i.e. no compression, no alpha). Palettes apparently can use 1, 4, or 8 bit pixels, but
not 2.
*/

#ifndef _BMP_H
#define _BMP_H

#include <stdint.h>

typedef struct _Bmp_header{
    char magic[2]; //"BM"
    uint32_t file_size; //Size in bytes of full file
    uint32_t img_offset; //Offset in bytes of bitmap data
} Bmp_header;

#define BITMAPCOREHEADER 12 //Don't use this for now. Maybe later(?)
#define BITMAPINFOHEADER 40

#define BI_RGB 0
#define BI_BITFIELDS 3

typedef struct _Bmp_dib{
    uint32_t dib_size; //Size in bytes of DIB
    int32_t pix_width; //Width in pixels
    int32_t pix_height; //Height in pixels
    uint32_t compression; //BI_RGB or BI_BITFIELDS
    uint32_t bitmap_size; //Size in bytes of bitmap data
    int32_t xres; //Pixels per meter, horizontal
    int32_t yres; //Pixels per meter, vertical
    uint32_t colors; //Number of colors, or 0 for 2^bpp
    uint32_t important_colors; //Number of important colors, not really important
    uint16_t bpp; //Bits per pixel: 1, 4, 8, 16, 24, or 32
} Bmp_dib;

typedef struct _Bitmap{
    Bmp_dib* dib; //Pointer to DIB
    uint32_t* palette; //Pointer to palette if bps<=8, else NULL
    void* bitmap; //Pointer to bitmap data
    Bmp_header header; //Header struct
    uint32_t R, G, B; //Color bitmasks, not used for BI_RGB
    int32_t width, height; //Width and height in pixels
    uint32_t row_bytes; //pitch, i.e. bytes per row
    uint32_t colors; //Number of colors, or 0 for 2^bps
    uint8_t bps; //Bits per sample: 1, 4, 8, 16, 24, or 32
} Bitmap;

void Bmp_validate_dib(Bmp_dib*, Bitmap*); //Given a Bitmap, set its DIB to reflect properties
void Bmp_validate_header(Bmp_header*, Bmp_dib*, Bitmap*); //given a Bitmap and its DIB, validate its header
void Bmp_save(Bitmap*, FILE*); //Save a Bitmap to a FILE
Bitmap* Bmp_load(FILE*); //Allocate and load a Bitmap from a FILE

Bitmap* Bmp_empty(int, int, int, int); //Generate an empty bitmap of a given size, bit depth, and color space
void Bmp_free(Bitmap*); //Free allocated Bitmap

uint32_t get_pixel(Bitmap*, int, int); //Get the pixel value of a Bitmap at a coordinate
void set_pixel(Bitmap*, int, int, uint32_t); //Set the pixel at a coordinate in a Bitmap

void Bmp_dump(FILE*, Bitmap*); //Print some data of a Bitmap

uint32_t HSV2RGB(float, float, float); //HSV 2 RGB in X8R8G8B8 big-endian

void Bmp_flipH(Bitmap*); //Flips horizontal, in-place
void Bmp_flipV(Bitmap*); //Flips vertical, in-place
Bitmap* Bmp_rotated(Bitmap*, float); //Returns a rotated copy

void Bmp_count(Bitmap*, int*); //Get the occurrance of colors in indexed bitmaps

void Bmp_try_palette(Bitmap*, int, int);
void Bmp_base_palette(Bitmap*, int, int, float, float, float);

Bitmap* Bmp_resize(Bitmap*, int, int);

void Bmp_COM(Bitmap*, uint32_t, float*, float*); //Center of mass of non-BG bixels
float Bmp_STD(Bitmap*, uint32_t, float, float); //Deviation of non-BG pixels

void draw_ellipse(Bitmap*, uint32_t, uint32_t, int, int, int, int);

#endif