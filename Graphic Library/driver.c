/**
 * File: driver.c 
 * Author: Quan Nguyen
 * Project: Graphic Library 
 * Class: CSC252 
 * Purpose: This is the driver code to prove that my library.c file 
 * is indeed working. Draw three different line on three stroke of "n". 
 * The idea is taken from hilbert.c and simplifies down. 
 */ 
#include "graphics.h"
int main(int argc, char **argv)
{	 
	int i;
	init_graphics();
	//Construct an offscreen buffer to draw to
	void *buf = create_buffer(); 
	char key;
	int n = 0;
	blit(buf);
	do {
        if (n==3) {
            break;
        }
		key = getkey();
		if (key == 'q')
			break;
		else if (key == 'n') {
			n++;
			clear_screen(buf); 
            // draw three different lines based on the three stroke of n
            if (n == 1) {
                draw_line(buf, 0, 0, 120, 120, RGB(12, 58, 21));
            }
			if (n == 2) {
                draw_line(buf, 120, 67, 321, 399, RGB(0, 58, 21));
            }
            if (n == 3) {
                draw_line(buf, 31, 62, 520, 310, RGB(12, 0, 21));
            }
			blit(buf);
		}
		sleep_ms(200);
	}
	while (1);

	exit_graphics(); 
	return 0;

}