/**
 * File: graphics.h 
 * Author: Quan Nguyen
 * Project: Graphic Library 
 * Class: CSC252 
 * Purpose: This is a header file for libary.c. This will contains the defininition prototypes for all the “public” functions, color_t 
 * typedef, and your RGB conversion macro.  
 */
#ifndef MYGRAPHIC
#define MYGRAPHIC
typedef unsigned short color_t;
#define RGB(r, g, b) ((color_t)((r) << 11) | (g) << 5 | (b))
/**
 * Initialize the graphic library
 */
void init_graphics();
/**
 * Exit the graphic and clean up memory
 */
void exit_graphics();
/**
 * Get a user key input
 */
char getkey();
/**
 * Sleep for a certain amount of time
 */
void sleep_ms(long ms);
/**
 * Clear off the current screen.
 */
void clear_screen(void *img);
/**
 * Draw content to a pixel
 */
void draw_pixel(void *img, int x, int y, color_t color);
/**
 * Draw content to a line
 */
void draw_line(void *img, int x1, int y1, int x2, int y2, color_t c);
/**
 * Create a second buffer
 */
void *create_buffer();
/**
 * A memory copy from our offscreen buffer to the framebuffer
 */
void blit(void *src); 
#endif
