#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/time.h>
#include "safeio.h"
#include "bmp.h"
#include "rand.h"

void write_value(FILE* dst, uint64_t value, int len, int write_big){
    safe_write(value, len, write_big, dst);
}

uint64_t read_value(FILE* src, int len, int read_big){
    return safe_read(len, read_big, src);
}

void Bmp_validate_dib(Bmp_dib* dib, Bitmap* bitmap){
    dib->pix_width = bitmap->width;
    dib->pix_height = bitmap->height;
    if(!(bitmap->R | bitmap->G | bitmap->B)){
        dib->compression = BI_RGB;
    }else{
        dib->compression = BI_BITFIELDS;
    }
    if(bitmap->palette != NULL && bitmap->colors<(1<<bitmap->bps)){
        dib->colors = bitmap->colors;
    }else{
        dib->colors = 0;
    }
    dib->bitmap_size = bitmap->height * bitmap->row_bytes;
    dib->important_colors = 0;
    dib->xres = 300;
    dib->yres = 300;
    dib->bpp = bitmap->bps;
    dib->dib_size = BITMAPINFOHEADER;
}

void Bmp_validate_header(Bmp_header* header, Bmp_dib* dib, Bitmap* bitmap){
    header->magic[0] = 'B';
    header->magic[1] = 'M';
    header->file_size = 14 + dib->dib_size + dib->bitmap_size;
    if(dib->compression == BI_BITFIELDS){
        header->file_size += 3*dib->bpp/8;
    }
    if(dib->bpp<=8){
        int colors = dib->colors;
        if(colors == 0){
            colors = 1<<dib->bpp;
        }
        header->file_size += colors*4;
    }
    header->img_offset = header->file_size - dib->bitmap_size;
}

void Bmp_save(Bitmap* bitmap, FILE* dst){
    Bmp_validate_dib(bitmap->dib, bitmap);
    Bmp_validate_header(&bitmap->header, bitmap->dib, bitmap);
    //write header
    fwrite(bitmap->header.magic, 1, 2, dst); //"BM"
    write_value(dst, bitmap->header.file_size, 4, 0); //file size in bytes
    write_value(dst, 0, 2, 0); //reserved 1
    write_value(dst, 0, 2, 0); //reserved 2
    write_value(dst, bitmap->header.img_offset, 4, 0); //offset of bitmap image
    //write DIB
    if(bitmap->dib->dib_size == BITMAPCOREHEADER){
        write_value(dst, BITMAPCOREHEADER,          4, 0);
        write_value(dst, bitmap->dib->pix_width,    2, 0);
        write_value(dst, bitmap->dib->pix_height,   2, 0);
        write_value(dst, 1, 2, 0);
        write_value(dst, bitmap->dib->bpp,          2, 0);
    }else if(bitmap->dib->dib_size == BITMAPINFOHEADER){
        int dib_size = BITMAPINFOHEADER;
        write_value(dst, dib_size,                  4, 0);
        write_value(dst, bitmap->dib->pix_width,    4, 0);
        write_value(dst, bitmap->dib->pix_height,   4, 0);
        write_value(dst, 1, 2, 0);
        write_value(dst, bitmap->dib->bpp,          2, 0);
        write_value(dst, bitmap->dib->compression,  4, 0);
        write_value(dst, bitmap->dib->bitmap_size,  4, 0);
        write_value(dst, bitmap->dib->xres,         4, 0);
        write_value(dst, bitmap->dib->yres,         4, 0);
        write_value(dst, bitmap->dib->colors,       4, 0);
        write_value(dst, bitmap->dib->important_colors, 4, 0);
    }
    //write color bitmasks
    if(bitmap->dib->compression == BI_BITFIELDS){
        write_value(dst, bitmap->R,                 4, 0);
        write_value(dst, bitmap->G,                 4, 0);
        write_value(dst, bitmap->B,                 4, 0);
    }
    //write palette
    if(bitmap->dib->bpp <= 8 && bitmap->palette != NULL){
        int colors = bitmap->dib->colors;
        if(colors==0){
            colors = 1<<bitmap->dib->bpp;
        }
        for(int i=0; i<colors; i++){
            write_value(dst, bitmap->palette[i],    4, 0);
        }
    }
    int preimg = ftell(dst);
    for(int y=0; y<bitmap->dib->pix_height; y++){
        if(bitmap->dib->bpp <= 8){
            fwrite(bitmap->bitmap + y*bitmap->row_bytes, bitmap->row_bytes, 1, dst);
        }else{
            int bytes = 0;
            for(int x=0; x<bitmap->dib->pix_width; x++){
                uint64_t value = 0;
                memcpy(&value, bitmap->bitmap + y*bitmap->row_bytes + x*(bitmap->dib->bpp/8), bitmap->dib->bpp/8);
                write_value(dst, value, bitmap->dib->bpp/8, 0);
                bytes += bitmap->dib->bpp/8;
            }
            for(; bytes<bitmap->row_bytes; bytes++){
                fputc(0, dst);
            }
        }
    }
    int size = ftell(dst)-preimg;
}

Bitmap* Bmp_load(FILE* src){
    Bitmap* ret = malloc(sizeof(Bitmap));
    fread(ret->header.magic, 1, 2, src);
    ret->header.file_size = read_value(src, 4, 0);
    fseek(src, 4, SEEK_CUR);
    ret->header.img_offset = read_value(src, 4, 0);
    ret->dib = malloc(sizeof(Bmp_dib));
    ret->dib->dib_size = read_value(src, 4, 0);
    if(ret->dib->dib_size == BITMAPCOREHEADER){
        ret->width = read_value(src, 2, 0);
        ret->height = read_value(src, 2, 0);
        fseek(src, 2, SEEK_CUR);
        ret->bps = read_value(src, 2, 0);
        ret->row_bytes = (ret->width*ret->bps+7)>>3;
        ret->row_bytes = (ret->row_bytes+3)&~3;
        ret->dib->bitmap_size = ret->height*ret->row_bytes;
        ret->colors = 0;
        ret->dib->important_colors = 0;
        ret->dib->compression = BI_RGB;
    }else{
        ret->width = read_value(src, 4, 0);
        ret->height = read_value(src, 4, 0);
        fseek(src, 2, SEEK_CUR);
        ret->bps = read_value(src, 2, 0);
        ret->row_bytes = (ret->width*ret->bps+7)>>3;
        ret->row_bytes = (ret->row_bytes+3)&~3;
        ret->dib->compression = read_value(src, 4, 0);
        ret->dib->bitmap_size = read_value(src, 4, 0);
        ret->dib->xres = read_value(src, 4, 0);
        ret->dib->yres = read_value(src, 4, 0);
        ret->colors = read_value(src, 4, 0);
        ret->dib->important_colors = read_value(src, 4, 0);
    }
    ret->dib->colors = ret->colors;
    ret->dib->bpp = ret->bps;
    ret->dib->pix_width = ret->width;
    ret->dib->pix_height = ret->height;
    if(ret->dib->compression == BI_BITFIELDS){
        ret->R = read_value(src, 4, 0);
        ret->G = read_value(src, 4, 0);
        ret->B = read_value(src, 4, 0);
    }else{
        ret->R = ret->G = ret->B = 0;
    }
    if(ret->bps<=8){
        int cols = ret->colors;
        if(cols==0){
            cols = 1<<ret->bps;
        }
        ret->palette = malloc(sizeof(uint32_t)*cols);
        for(int i=0; i<cols; i++){
            ret->palette[i] = read_value(src, 4, 0);
        }
    }
	else
		ret->palette = NULL;
    ret->bitmap = malloc(ret->dib->bitmap_size);
    for(int y=0; y<ret->height; y++){
        if(ret->bps<=8){
            fread(ret->bitmap + y*ret->row_bytes, ret->row_bytes, 1, src);
        }else{
            int bytes = 0;
            for(int i=0; i<ret->width; i++){
                uint64_t value = read_value(src, ret->bps/8, 0);
                memcpy(ret->bitmap + y*ret->row_bytes + (i*ret->bps)/8, &value, ret->bps/8);
                bytes += ret->bps/8;
            }
            fread(ret->bitmap+y*ret->row_bytes + bytes, ret->row_bytes-bytes, 1, src);
        }
    }
    return ret;
}

Bitmap* Bmp_empty(int width, int height, int bps, int colors){
    Bitmap* ret = malloc(sizeof(Bitmap));
    ret->width = width;
    ret->height = height;
    ret->bps = bps;
    ret->colors = colors;
    ret->dib = malloc(sizeof(Bmp_dib));
    if(bps<=8){
        if(colors==0){
            colors = 1<<bps;
        }
        ret->palette = malloc(sizeof(uint32_t)*colors);
    }else{
        ret->palette = NULL;
    }
    int row_bytes = (width*bps+7)>>3;
    row_bytes = (row_bytes+3)&~3;
    ret->row_bytes = row_bytes;
    ret->bitmap = malloc(row_bytes*height);
	ret->R = ret->G = ret->B = 0;
    return ret;
}

void Bmp_free(Bitmap* bitmap){
    free(bitmap->dib);
    if(bitmap->palette!=NULL){
        free(bitmap->palette);
    }
    free(bitmap->bitmap);
    free(bitmap);
}

uint32_t get_pixel(Bitmap* bitmap, int x, int y){
    if(x<0 || y<0 || x>=bitmap->width || y>=bitmap->height){
        return 0;
    }
    int bps = bitmap->bps;
    if(bps<8){
        int chunks = 8/bps;
        uint16_t bitmask = 0xffff<<(bps);
        bitmask = ~bitmask;
        int index = y*bitmap->row_bytes + (x*bps)/8;
        uint8_t* pixdata = (uint8_t*)bitmap->bitmap;
        uint16_t ret = pixdata[index];
        int into = chunks - 1 - (x%chunks);
        ret >>= bps * into;
        return ret&bitmask;
    }
    void* addr = bitmap->bitmap + y*bitmap->row_bytes + x*(bps/8);
    uint32_t ret = 0;
    memcpy(&ret, addr, bps/8);
    return ret;
}

void set_pixel(Bitmap* bitmap, int x, int y, uint32_t pix){
    if(x<0 || y<0 || x>=bitmap->width || y>=bitmap->height){
        return;
    }
    int bps = bitmap->bps;
    if(bps<8){
        int chunks = 8/bps;
        uint16_t bitmask = 0xffff<<(bps);
        bitmask = ~bitmask;
        pix &= bitmask;
        int index = y*bitmap->row_bytes + (x*bps)/8;
        uint8_t* pixdata = (uint8_t*)bitmap->bitmap;
        int into = chunks - 1 - (x%chunks);
        uint8_t mask = bitmask << (bps*into);
        pixdata[index] &= ~mask;
        pixdata[index] |= pix<<(bps*into);
    }else{
        void* addr = bitmap->bitmap + y*bitmap->row_bytes + x*(bps/8);
        memcpy(addr, &pix, bps/8);
    }
}

void Bmp_dump(FILE* dst, Bitmap* bitmap){
    fprintf(dst, "Bitmap with bpp: %d\nSize: %d, %d\nCompression: %d\nImage size: %d\nPitch: %d\n",bitmap->bps,
        bitmap->width, bitmap->height, bitmap->dib->compression, bitmap->dib->bitmap_size, bitmap->row_bytes);
    if(bitmap->dib->compression == BI_BITFIELDS){
        fprintf(dst, "R: %08x, G: %08x, B: %08x\n", bitmap->R, bitmap->G, bitmap->B);
    }
    if(bitmap->bps<=8){
        int cols = bitmap->colors;
        if(cols==0)cols = 1<<bitmap->bps;
        for(int i=0; i<cols; i++){
            fprintf(dst, "Palette[%d]: %08x\n", i, bitmap->palette[i]);
        }
    }
}

uint32_t HSV2RGB(float H, float S, float V){
    if(S>1)S=1;
    if(S<0)S=0;
    if(V>1)V=1;
    if(V<0)V=0;
    float hi = V;
    float lo = V*(1-S);
    while(H<0)H+=M_PI*2;
    while(H>=M_PI*2)H-=M_PI*2;
    float weight = fmod(H, M_PI/3) * 3.0/M_PI;
    int step = (int)(floor(H*3.0/M_PI)+.5);
    weight = H-step;
    float med = (step&1)?lo*weight + hi*(1.0-weight):hi*weight + lo*(1.0-weight);
    int r, g, b;
    switch(step){
        case 0:
            r = hi*255;
            b = lo*255;
            g = med*255;
            break;
        case 1:
            g = hi*255;
            b = lo*255;
            r = med*255;
            break;
        case 2:
            g = hi*255;
            r = lo*255;
            b = med*255;
            break;
        case 3:
            b = hi*255;
            r = lo*255;
            g = med*255;
            break;
        case 4:
            b = hi*255;
            g = lo*255;
            r = med*255;
            break;
        case 5:
            r = hi*255;
            g = lo*255;
            b = med*255;
            break;
    }
    r &= 0xff;
    g &= 0xff;
    b &= 0xff;
    return (r<<16)|(g<<8)|b;
}

void RGB2HSV(uint32_t rgb, float *H, float *S, float *V){
	int r = rgb >> 16, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
	int hi = r, med = g, lo = b, sextant = 0;
	if(g > r){
		if(r > b){
			hi = g;
			med = r;
			sextant = 1;
		}else if(b > g){
			hi = b;
			med = g;
			lo = r;
			sextant = 3;
		}else{
			hi = g;
			med = b;
			lo = r;
			sextant = 2;
		}
	}else if(g < b){
		if(r > b){
			med = b;
			lo = g;
			sextant = 5;
		}else{
			hi = b;
			lo = g;
			med = r;
			sextant = 4;
		}
	}
	*V = hi / 255.0;
	*S = (hi == 0) ? 0 : (hi - lo) / (float)hi;
	float hue = (hi == lo) ? 0 : (med - lo) / (float)(hi - lo);
	if(sextant & 1)
		hue = 1 - hue;
	*H = (hue + sextant) * M_PI / 3.0;
}

void Bmp_flipH(Bitmap* bitmap){
    for(int y=0; y<bitmap->height; y++){
        for(int x=0; x<bitmap->width/2; x++){
            uint32_t tmp = get_pixel(bitmap, x, y);
            set_pixel(bitmap, x, y, get_pixel(bitmap, bitmap->width-1-x, y));
            set_pixel(bitmap, bitmap->width-1-x, y, tmp);
        }
    }
}

void Bmp_flipV(Bitmap* bitmap){
    for(int y=0; y<bitmap->height/2; y++){
        for(int x=0; x<bitmap->width; x++){
            uint32_t tmp = get_pixel(bitmap, x, y);
            set_pixel(bitmap, x, y, get_pixel(bitmap, x, bitmap->height-1-y));
            set_pixel(bitmap, x, bitmap->height-1-y, tmp);
        }
    }
}

Bitmap* Bmp_rotated(Bitmap* bitmap, float angle){
    Bitmap* ret = Bmp_empty(bitmap->width, bitmap->height, bitmap->bps, bitmap->colors);
    memset(ret->bitmap, 0, bitmap->height*bitmap->row_bytes);
    int colors = bitmap->colors;
    if(colors==0){
        colors = 1<<bitmap->bps;
    }
    if(bitmap->bps<=8){
        memcpy(ret->palette, bitmap->palette, sizeof(uint32_t)*colors);
    }
    ret->R=bitmap->R;
    ret->G=bitmap->G;
    ret->B=bitmap->B;
    // memcpy(ret->bitmap, bitmap->bitmap, bitmap->height*bitmap->row_bytes);
    // return ret;
    double sang = sin(angle);
    double cang = cos(angle);
    for(int y=0; y<bitmap->height; y++){
        float sy = (y-bitmap->height/2);
        for(int x=0; x<bitmap->width; x++){
            float sx = (x-bitmap->width/2);
            float dx = sx*cang - sy*sang;
            float dy = sx*sang + sy*cang;
            uint32_t color = get_pixel(bitmap, (int)round(dx+bitmap->width/2), (int)round(dy+bitmap->height/2));
            set_pixel(ret, x, y, color);
        }
    }
    return ret;
}

void Bmp_count(Bitmap* bitmap, int* counts){
    if(bitmap->bps>8){
        return;
    }
    int colors=bitmap->colors;
    if(colors==0){
        colors = 1<<bitmap->bps;
    }
    memset(counts, 0, sizeof(int)*colors);
    for(int y=0; y<bitmap->height; y++){
        for(int x=0; x<bitmap->width; x++){
            uint32_t color = get_pixel(bitmap, x, y);
            counts[color]++;
        }
    }
}

void Bmp_try_palette(Bitmap* bitmap, int start, int num){
    if(num==0)return;
    int index = start;
    float basehue = rand_uniform()*2*M_PI;
    float saturation = rand_uniform();
    Bmp_base_palette(bitmap, start, num, basehue, saturation, rand_uniform());
}

void Bmp_base_palette(Bitmap* bitmap, int start, int num, float H, float S, float V){
    if(num==0)return;
    int index = start;
    H += gaussian()*M_PI/9;
    S += gaussian()*.2;
    V += gaussian()*.2;
    bitmap->palette[index++] = HSV2RGB(H, S, V);
    if(!(num&1)){
        bitmap->palette[index++] = HSV2RGB(2*M_PI-H, rand_uniform(), rand_uniform());
    }
    for(; index-start<num; index+=2){
        float huedif = gaussian()*M_PI/12;
        S += gaussian()*.2;
        V += gaussian()*.2;
        bitmap->palette[index] = HSV2RGB(H-huedif, S, V);
        bitmap->palette[index+1] = HSV2RGB(H+huedif, S, V);
    }
}

Bitmap* Bmp_resize(Bitmap* bitmap, int width, int height){
    Bitmap* ret = Bmp_empty(width,height,bitmap->bps,bitmap->colors);
    if(bitmap->bps<=8){
        int colors=bitmap->colors;
        if(!colors)colors=1<<bitmap->bps;
        memcpy(ret->palette,bitmap->palette,sizeof(uint32_t)*colors);
    }
    ret->R=bitmap->R;
    ret->G=bitmap->G;
    ret->B=bitmap->B;
    float xrat = (float)bitmap->width/(float)width, yrat = (float)bitmap->height/(float)height;
    for(int x=0; x<width; x++){
		float fx = x * xrat;
		int nx = (int)fx;
		nx = (fx - nx >= 0.5) ? nx + 1 : nx;
		nx = (nx < 0) ? 0 : (nx >= bitmap->width) ? bitmap->width - 1 : nx;
        for(int y=0; y<height; y++){
			float fy = y * yrat;
			int ny = (int)fy;
			ny = (fy - ny >= 0.5) ? ny + 1 : ny;
			ny = (ny < 0) ? 0 : (ny >= bitmap->height) ? bitmap->height - 1 : ny;
            uint32_t col = get_pixel(bitmap, nx, ny);
            set_pixel(ret,x,y,col);
        }
    }
    return ret;
}

void Bmp_COM(Bitmap* bitmap, uint32_t bg, float* dx, float* dy){
    float ax=0, ay=0;
    int cx=0, cy=0;
    for(int x=0; x<bitmap->width; x++){
        for(int y=0; y<bitmap->width; y++){
            uint32_t pix = get_pixel(bitmap,x,y);
            if(pix!=bg){
                ax += x;
                ay += y;
                cx++;
                cy++;
            }
        }
    }
    *dx = ax/cx;
    *dy = ay/cy;
}

float Bmp_STD(Bitmap* bitmap, uint32_t bg, float cenx, float ceny){
    float ax=0, ay=0;
    int ct=0;
    for(int x=0; x<bitmap->width; x++){
        for(int y=0; y<bitmap->width; y++){
            uint32_t pix = get_pixel(bitmap,x,y);
            if(pix!=bg){
                float distx = x-cenx, disty = y-ceny;
                ax += distx*distx;
                ay += disty*disty;
                ct++;
            }
        }
    }
    return sqrt((ax+ay)/ct);
}

void draw_ellipse(Bitmap* bitmap, uint32_t line, uint32_t fill, int cx, int cy, int rx, int ry){
    int lastx1=cx, lastx2=cx;
    int y;
    for(y=cy-ry; y<=cy+ry; y++){
        float rcnd = (float)(y-cy);
        rcnd *= rcnd;
        rcnd /= (ry*ry);
        float yfac = rcnd>=0?sqrt(1 - ((float)y-cy)*(y-cy)/(float)ry/ry):0;
        int rtx = yfac*rx;
        for(int x=cx-rtx; x<=lastx1; x++){
            set_pixel(bitmap, x, y-1, line);
        }
        for(int x=cx+rtx; x>=lastx2; x--){
            set_pixel(bitmap, x, y-1, line);
        }
        for(int x=lastx1; x<=cx-rtx; x++){
            set_pixel(bitmap, x, y, line);
        }
        for(int x=lastx2; x>=cx+rtx; x--){
            set_pixel(bitmap, x, y, line);
        }
        lastx1 = cx-rtx-1;
        lastx2 = cx+rtx+1;
        set_pixel(bitmap,lastx1,y,line);
        set_pixel(bitmap,lastx2,y,line);
        for(int x=lastx1+1; x<lastx2-1; x++){
            set_pixel(bitmap, x, y, fill);
        }
    }
    for(int x=lastx1; x<=lastx2; x++){
        set_pixel(bitmap, x, y, line);
    }
}