/**
 * File: library.c 
 * Author: Quan Nguyen
 * Project: Graphic Library 
 * Class: CSC252 
 * Purpose: This is the main file for library. This will implement the prototypes 
 * previously defined in graphic.h. This small graphics library that can set a pixel 
 * to a particular color, draw some basic shapes, and read keypresses. This graphic 
 * library will not use any function orginated from C library but rather use Linux 
 * system call directly. 
 */
#include "graphics.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h> 
#include <linux/fb.h> 
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <sys/select.h> 
#include <unistd.h>
// global pointer which take the pointer from mmap in order for us to manipulate
color_t *frameBuffer;  
// the fileDescriptor we would get when open a file
int fileDescriptor;
// the size/length of the map 
int yLength, bitDepth, size;
// struct of the terminal. There are two in order for us to restore the previous one
struct termios old, new;
/**
 * Initialize the graphic library. Open up a file, /dev/fb0, that represents 
 * the first (zero-th) framebuffer attached to the computer. Then, use mmap to get 
 * information of the memory mapping to use pointer arithmetic or array subscripting
 * to set each individual pixel. Finally, we use ioctl syscall to get the screen 
 * size and bits per pixels, plus managing some input and output such as disable keypress 
 * echo and buffering the keypresses. 
 */
void init_graphics() {
    // open the file and get it to read and write
    fileDescriptor = open("/dev/fb0", O_RDWR);
    // get the mapping size information
    struct fb_var_screeninfo virReso; 
    struct fb_fix_screeninfo bitDept;  
    ioctl(fileDescriptor, FBIOGET_VSCREENINFO, &virReso);  
    ioctl(fileDescriptor, FBIOGET_FSCREENINFO, &bitDept);
    yLength = virReso.yres_virtual; 
    bitDepth = bitDept.line_length;
    size = yLength*bitDepth; 
    // get map pointer to the memory mapping information with read and write right. 
    frameBuffer = (color_t*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fileDescriptor, 0);  
    // get the terminal state right now 
    ioctl(STDIN_FILENO, TCGETS, &new); 
    old = new;                              // backup
    new.c_lflag &= ~(ICANON | ECHO);        // disable line buffering and feedback
    ioctl(STDIN_FILENO, TCSETS, &new); 
}
/**
 * Exit the graphic and clean up memory. Also, this will also reenable key press echoing and buffering as 
 * as before. 
 */
void exit_graphics() {
    clear_screen(frameBuffer);
    munmap(frameBuffer, yLength*bitDepth); 
    ioctl(STDIN_FILENO, TCSETS, &old);
    close(fileDescriptor);
}
/**
 * Get a user key input. Get only one single key stroke.
 */
char getkey() {
    fd_set rfds;
    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(0, &rfds); 
    select(1, &rfds, NULL, NULL, NULL);
    char buff;
    read(STDIN_FILENO, &buff, 1); // get only one press 
    return buff;
}
/**
 * Sleep for a certain amount of time. This ms will be passed in and using 
 * nanosleep. The time sleep will be stored in tim.nanosec fields of the time
 * spec. 
 */
void sleep_ms(long ms) {
    nanosleep((const struct timespec[]){{0, ms*1000000L}}, NULL);
}
/**
 * Clear off the current screen.
 */
void clear_screen(void *img) {
    color_t *charImg = (color_t *)img; 
    int i = 0; 
    while (i < (yLength*bitDepth/2)) {
        charImg[i] = 0; 
        i++;
    } 
}
/**
 * Draw content to a pixel. 
 */
void draw_pixel(void *img, int x, int y, color_t color) { 
    if (x <0 || y < 0) {
        return;
    }
    if (x > bitDepth/2 || y > yLength) {
        return; 
    }
    color_t *castImg = (color_t *)img;
    castImg[y*(bitDepth/2)+x] = color; 
}
/**
 * Get the absolute value of a number and return it. 
 * This is implement to avoid using the c standard library. 
 */
int absoluteVal(int number) {
    if (number >= 0) {
        return number;
    } else {
        return (-1)*number;
    }
}
/**
 * Draw content to a line. Thanks to http://members.chello.at/easyfilter/bresenham.html. 
 */
void draw_line(void *img, int x1, int y1, int x2, int y2, color_t c)
{
   int dx =  absoluteVal(x2-x1), sx = x1<x2 ? 1 : -1;
   int dy = -absoluteVal(y2-y1), sy = y1<y2 ? 1 : -1; 
   int err = dx+dy, e2; /* error value e_xy */
 
   for(;;){  /* loop */
      draw_pixel(img, x1,y1, c);
      if (x1==x2 && y1==y2) break;
      e2 = 2*err;
      if (e2 >= dy) { err += dy; x1 += sx; } /* e_xy+e_x > 0 */
      if (e2 <= dx) { err += dx; y1 += sy; } /* e_xy+e_y < 0 */
   }
}
/**
 * Create a second buffer. Return the pointer to that buffer. The size of the buffer is 
 * identical to the frameBuffer. 
 */
void *create_buffer() {

    void *ptr = mmap(NULL, size, PROT_READ|PROT_WRITE,  MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    return ptr; 
}
/**
 * A memory copy from our offscreen buffer to the frameBuffer. The frameBuffer is 
 * the original stuff 
 */
void blit(void *src) { 
    // casting the src pointer in order to get byte access 
    color_t *castSrc = (color_t *)src;  
    int i = 0; 
    for (i = 0; i < (size/2); i++) {
        frameBuffer[i] = castSrc[i];
    }
} 
