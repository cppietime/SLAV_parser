#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include "bmp.h"
#include "safeio.h"
#include "rand.h"
#include "splines.h"

static float lastx=0, lasty=0;

int binomial(int n, int k){
    if(k>n/2)k=n-k;
    int ret = 1;
    for(int i=n; i>n-k; i--)ret *= i;
    for(int i=1; i<=k; i++)ret /= i;
    return ret;
}

void draw_line(Bitmap* bmp, uint32_t color, float fx0, float fy0, float fx1, float fy1){
    int x0 = fx0*bmp->width,
        x1 = fx1*bmp->width,
        y0 = fy0*bmp->height,
        y1 = fy1*bmp->height;
    int dx = x1-x0;
    int dy = y1-y0;
    // float len = sqrt(dx*dx + dy*dy);
    //Single point
    if(dx==0&&dy==0){
        set_pixel(bmp, x0, y0, color);
        return;
    }
    //Incr x
    if(abs(dx)>abs(dy)){
        int xinc = dx>0?1:-1;
        int y = y0;
        float yerr = 0;
        float slope = (float)dy/(float)dx;
        for(int x=x0; x!=x1+xinc; x+=xinc){
            set_pixel(bmp, x, y, color);
            yerr += slope;
            if((int)yerr!=0){
                y += (int)yerr*xinc;
                yerr -= (int)yerr;
            }
        }
    }
    //Incr y
    else{
        int yinc = dy>0?1:-1;
        int x = x0;
        float xerr = 0;
        float slope = (float)dx/(float)dy;
        for(int y=y0; y!=y1+yinc; y+=yinc){
            set_pixel(bmp, x, y, color);
            xerr += slope;
            if((int)xerr!=0){
                x += (int)xerr*yinc;
                xerr -= (int)xerr;
            }
        }
    }
}

//Randomized Bezier curve of random order
void put_bezier(Bitmap* bmp, int samples, double gain){
    //Decide order
    int order = 2+(rand_replace()%5);
    //Generate control points
    float* points = malloc(sizeof(float)*2*order);
    points[0]=lastx;
    points[1]=lasty;
    for(int i=2; i<order*2; i++){
        float param = (float)rand_replace()/rand_max_replace;
        points[i] = perlin_gain(param, gain);
    }
    //Calculate binomial coeffs
    int* coefs = malloc(sizeof(int)*order);
    for(int i=0; i<=order/2; i++){
        coefs[i] = binomial(order-1, i);
        coefs[order-1-i] = coefs[i];
    }
    //Draw polyline
    float ox = points[0], oy = points[1];
    float incr = 1.0/samples;
    for(float t=0; t<=1.0; t+=incr){
        float nx=0, ny=0;
        for(int i=0; i<order; i++){
            float time = pow(t,i)*pow(1.0-t,order-1-i)*coefs[i];
            nx += time*points[i*2];
            ny += time*points[i*2+1];
        }
        draw_line(bmp, 1, ox, oy, nx, ny);
        ox=nx;
        oy=ny;
    }
    lastx=ox;
    lasty=oy;
    free(points);
    free(coefs);
}

//Draw random Catmull-Rom Spline
void put_cr(Bitmap* bmp, int samples, double gain){
    //Generate random control points
    float points[8] = {0, 0, lastx, lasty};
    for(int i=0; i<8; i++){
        if(i<2 || i>3){
            float param = (float)rand_replace()/rand_max_replace;
            points[i] = perlin_gain(param, gain);
        }
    }
    //Random alpha in [0,1]
    float alpha = (float)rand_replace()/rand_max_replace;
    //Calculate t values
    float t[4] = {0};
    float rdt[5];
    for(int i=1; i<4; i++){
        float dx = points[i*2]-points[(i-1)*2];
        float dy = points[i*2+1]-points[(i-1)*2+1];
        float dist = dx*dx + dy*dy;
        t[i] = t[i-1] + pow(dist, alpha/2.0);
        rdt[i-1] = 1.0/(t[i]-t[i-1]);
    }
    rdt[3] = 1.0/(t[2]-t[0]);
    rdt[4] = 1.0/(t[3]-t[1]);
    //Draw polyline
    float incr = (t[2]-t[1])/samples;
    float a[6];
    float b[4];
    float ox = points[2];
    float oy = points[3];
    for(float ti = t[1]+incr; ti<incr+t[2]; ti+=incr){
        for(int i=0; i<3; i++){
            a[i*2]   = (t[i+1]-ti)*rdt[i]*points[i*2]   + (ti-t[i])*rdt[i]*points[(i+1)*2];
            a[i*2+1] = (t[i+1]-ti)*rdt[i]*points[i*2+1] + (ti-t[i])*rdt[i]*points[(i+1)*2+1];
        }
        for(int i=0; i<2; i++){
            b[i*2]   = (t[i+2]-ti)*rdt[i+3]*a[i*2]   + (ti-t[i])*rdt[i+3]*a[(i+1)*2];
            b[i*2+1] = (t[i+2]-ti)*rdt[i+3]*a[i*2+1] + (ti-t[i])*rdt[i+3]*a[(i+1)*2+1];
        }
        float nx = (t[2]-ti)*rdt[1]*b[0] + (ti-t[1])*rdt[1]*b[2];
        float ny = (t[2]-ti)*rdt[1]*b[1] + (ti-t[1])*rdt[1]*b[3];
        draw_line(bmp, 1, ox, oy, nx, ny);
        ox=nx;
        oy=ny;
    }
    lastx = points[4];
    lasty = points[5];
}

void stack_fill(Bitmap* bmp, int x0, int y0, int color, int check){
    uint32_t col = get_pixel(bmp, x0, y0);
    if(col==color)return;
    if(check>=0 && col!=check)return;
    if(check<0 && col==-check)return;
    set_pixel(bmp,x0,y0,color);
    if(x0>0)stack_fill(bmp,x0-1,y0,color,check);
    if(x0<bmp->width-1)stack_fill(bmp,x0+1,y0,color,check);
    if(y0>0)stack_fill(bmp,x0,y0-1,color,check);
    if(y0<bmp->height-1)stack_fill(bmp,x0,y0+1,color,check);
}

//scanline-based flood fill
void line_fill(Bitmap* bmp, int x0, int y0, uint32_t color, int32_t check){
    while(x0>0){
        x0--;
        uint32_t test = get_pixel(bmp, x0, y0);
        if(test==color || (check>=0&&test!=check) || (check<0&&test==-check)){
            x0++;
            break;
        }
    }
    int x1=x0;
    while(x1<bmp->width){
        set_pixel(bmp, x1++, y0, color);
        uint32_t test = get_pixel(bmp, x1, y0);
        if(test==color || (check>=0&&test!=check) || (check<0&&test==-check))break;
    }
    for(int x=x0; x<x1; x++){
        if(y0>0){
            uint32_t test = get_pixel(bmp, x, y0-1);
            if(test==color || (check>=0&&test!=check) || (check<0&&test==-check));
            else{
                line_fill(bmp, x, y0-1, color, check);
            }
        }
        if(y0+1<bmp->height){
            uint32_t test = get_pixel(bmp, x, y0+1);
            if(test==color || (check>=0&&test!=check) || (check<0&&test==-check));
            else{
                line_fill(bmp, x, y0+1, color, check);
            }
        }
    }
}

void outline(Bitmap* bmp, uint32_t target, uint32_t border, uint32_t col){
    for(int x=0; x<bmp->width; x++){
        for(int y=0; y<bmp->height; y++){
            if(get_pixel(bmp, x, y)!=target)continue;
            if(x>0 && get_pixel(bmp, x-1, y)==border){
                set_pixel(bmp, x, y, col);
            }else if(x+1<bmp->width && get_pixel(bmp,x+1,y)==border){
                set_pixel(bmp, x, y, col);
            }else if(y>0 && get_pixel(bmp,x,y-1)==border){
                set_pixel(bmp, x, y, col);
            }else if(y+1<bmp->height && get_pixel(bmp,x,y+1)==border){
                set_pixel(bmp, x, y, col);
            }
        }
    }
}

Bitmap* random_element(int w, int h, int compl, double gain, uint32_t col){
    Bitmap* ret = Bmp_empty(w, h, 8, 0);
    ret->R=ret->G=ret->B=0;
    memset(ret->bitmap, col, w*h);    
    float param = rand_uniform();
    lastx = perlin_gain(param, gain);
    param = rand_uniform();
    lasty = perlin_gain(param, gain);
    for(int i=0; i<compl; i++){
        if(rand_replace()&3){
            put_bezier(ret, 25, gain);
        }else{
            put_cr(ret, 25, gain);
        }
    }
    // for(int i=0; i<40; i++){
        // int x = rand_replace()%w, y=rand_replace()%h;
        // stack_fill(ret, x, y, col, 0xff);
    // }
    // printf("Prefill\n");
    line_fill(ret, 0, 0, 0xff, -1);
    line_fill(ret, 0, 0, 0, -col);
    outline(ret, 1, col, col);
    outline(ret, 0, col, 1);
    if(rand_uniform()*3<1){
        float cx, cy;
        Bmp_COM(ret, 0, &cx, &cy);
        int x0=0, x1=w/2;
        if(cx>x1){
            x0=x1;
            x1 = w;
        }
        for(int x=x0; x<x1; x++){
            for(int y=0; y<h; y++){
                uint32_t src = get_pixel(ret, x, y);
                set_pixel(ret, w-1-x, y, src);
            }
        }
    }
    return ret;
}
void overlay_element(Bitmap* dst, Bitmap* src, int xoff, int yoff, int mode, uint32_t cut){
    for(int x=0; x<src->width; x++){
        if(x+xoff<0 || x+xoff>=dst->width)continue;
        for(int y=0; y<src->height; y++){
            if(y+yoff<0 || y+yoff>=dst->height)continue;
            if(get_pixel(src,x,y)==0)continue;
            if(mode==OVERLAY_ALWAYS){
                set_pixel(dst,x+xoff,y+yoff,get_pixel(src,x,y));
            }else{
                uint32_t basecol = get_pixel(dst,x+xoff,y+yoff);
                if(mode==OVERLAY_WHEN && basecol==cut){
                    set_pixel(dst,x+xoff,y+yoff,get_pixel(src,x,y));
                }else if(mode==OVERLAY_WHENNOT && basecol!=cut){
                    set_pixel(dst,x+xoff,y+yoff,get_pixel(src,x,y));
                }
            }
        }
    }
}

splineimg_t* gen_splines(int w, int h, int layers){
    splineimg_t* ret = malloc(sizeof(splineimg_t));
    ret->layers=malloc(sizeof(Bitmap*)*layers);
    ret->modes = malloc(sizeof(int)*layers);
    ret->keys=malloc(sizeof(uint32_t)*layers);
    ret->offsets = malloc(sizeof(double)*layers*2);
    ret->len=layers;
    for(int i=0; i<layers; i++){
        ret->layers[i] = random_element(w, h, (int)(rand_uniform()*5+4), rand_uniform()*.5+.5, i+2);
        ret->modes[i] = OVERLAY_ALWAYS;
        ret->keys[i]=0;
        ret->offsets[i*2] = gaussian()*.25;
        ret->offsets[i*2+1] = gaussian()*.25;
    }
    return ret;
}

void free_splines(splineimg_t* img){
    for(int i=0; i<img->len; i++)
        Bmp_free(img->layers[i]);
    free(img->modes);
    free(img->keys);
    free(img->offsets);
    free(img);
}

void draw_eyes(Bitmap* bitmap, int i){
    float cx, cy;
    Bmp_COM(bitmap, 0, &cx, &cy);
    float dev = Bmp_STD(bitmap, 0, cx, cy);
    cx += gaussian()*bitmap->width*.125;
    cy += gaussian()*bitmap->width*.125;
    float rad = dev/4;
    float radx = rad*(1+gaussian()*.5);
    rad *= (1+gaussian()*.5);
    float dist = dev*(.15+rand_uniform()*.7);
    draw_ellipse(bitmap, 1, i, cx-dist, cy, radx, rad);
    draw_ellipse(bitmap, 1, i, cx+dist, cy, radx, rad);
    if(rad>radx)rad=radx;
    rad *= (.15+rand_uniform()*.4);
    float pupx = dist + gaussian()*radx/4;
    float pupy = gaussian()*rad/4;
    float puph = rad * (1+gaussian()*.5);
    draw_ellipse(bitmap, 1, 1, cx-pupx, cy+pupy, rad, puph);
    draw_ellipse(bitmap, 1, 1, cx+pupx, cy+pupy, rad, puph);
}

Bitmap* render_fwd(splineimg_t* img){
    Bitmap* ret = Bmp_empty(img->layers[0]->width, img->layers[0]->height, img->layers[0]->bps, 0);
    ret->R=ret->G=ret->B=0;
    memset(ret->bitmap, 0, ret->width*ret->height*ret->bps/8);
    for(int i=0; i<img->len; i++){
        overlay_element(ret, img->layers[i], (int)(img->offsets[i*2])*ret->width, (int)(img->offsets[i*2+1])*ret->height, img->modes[i], img->keys[i]);
    }
    draw_eyes(ret, img->len+2);
    return ret;
}

Bitmap* render_bwd(splineimg_t* img){
    Bitmap* ret = Bmp_empty(img->layers[0]->width, img->layers[0]->height, img->layers[0]->bps, 0);
    ret->R=ret->G=ret->B=0;
    memset(ret->bitmap, 0, ret->width*ret->height*ret->bps/8);
    for(int i=img->len-1; i>=0; i--){
        int mode = img->modes[i];
        uint32_t key = img->keys[i];
        overlay_element(ret, img->layers[i], (int)(img->offsets[i*2])*ret->width, (int)(img->offsets[i*2+1])*ret->height, mode, key);
    }
    Bmp_flipH(ret);
    return ret;
}

// void draw_spline_sprites(int w, int h, char* filename, const float* cols){
    // char name[256], bname[256], sname[256], sbname[256];
    // uint32_t bg=0xffffff, fg=0;
    // sprintf(name, "%s.bmp", filename);
    // sprintf(bname, "%sb.bmp", filename);
    // sprintf(sname, "%ss.bmp", filename);
    // sprintf(sbname, "%ssb.bmp", filename);
    // splineimg_t* splines = gen_splines(w, h, (int)(rand_uniform()*5+3));
    // Bitmap* front = render_fwd(splines);
    // Bitmap* back = render_bwd(splines);
    // front->palette[0]=bg;
    // front->palette[1]=fg;
    // front->palette[splines->len+2]=HSV2RGB(rand_uniform()*2*M_PI, rand_uniform()*.2, 1-rand_uniform()*.2);
    // //Bmp_try_palette(front, 2, splines->len);
    // Bmp_base_palette(front, 2, splines->len, cols[0], cols[1], cols[2]);
    // memcpy(back->palette, front->palette, (splines->len+3)*sizeof(uint32_t));
    // FILE* file = fopen(name,"wb");
    // Bmp_save(front,file);
    // fclose(file);
    // file = fopen(bname, "wb");
    // Bmp_save(back,file);
    // fclose(file);
    // Bmp_try_palette(front, 2, splines->len);
    // memcpy(back->palette, front->palette, (splines->len+2)*sizeof(uint32_t));
    // file = fopen(sname, "wb");
    // Bmp_save(front, file);
    // fclose(file);
    // file = fopen(sbname, "wb");
    // Bmp_save(back, file);
    // fclose(file);
    // Bmp_free(front);
    // Bmp_free(back);
    // free_splines(splines);
// }
