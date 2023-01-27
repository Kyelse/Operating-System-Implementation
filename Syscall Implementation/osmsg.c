/**
 * File: osmsg.c 
 * Author: Quan Nguyen
 * Project:  Syscalls
 * Class: CSC252 
 * Purpose: This is the main interaction program for passing small text messages from one user 
 * to another by way of the kernel by ultilizing the newly implemented two syscall in the sys.c 
 * file. 
 */ 
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <sys/syscall.h>     
#include <unistd.h>
/**
 * Make the function of syscall of sending message more readable. 
 */
int send_msg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}
/**
 * Make the function of syscall of getting message more readable. 
 */
int get_msg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}
/**
 *  Main function for handling arugment and print out the result. 
 *  The way that the program will be called is osmsg -s [userTo] [message] for sending, and 
 *  osmsg -r for receiving.  
 */ 
int main (int argc, char *argv[]) {
    if (!(argc == 2 || argc==4)) {
        printf("Please have correct number of argument(s), the current count is %d\n", argc);
        return -1; 
    } 
    if (argc == 2) {
        if (strcmp(argv[1], "-r") != 0) {
            printf("Invalid commands\n");
            return -1;
        }
    } else { // argc == 3 for sending
        if (strcmp(argv[1], "-s") != 0) {
            printf("Invalid commands\n");
            return -1;
        }
    }
    char user[33]; 
    strcpy(user, getenv("USER"));
    // handling sending message here
    if (strcmp(argv[1], "-s") == 0) {
        // the user here is the one sending the message (thus from)
        // again, the instruction is osmsg -s [userTo] [message]
        if (send_msg(argv[2], argv[3], user) == 0) {
            printf("Send successful\n"); 
            return 0; 
        } else {
            printf("Send failed\n"); 
            return -1;
        }
    } else { //strcmp(argv[1], "-r") == 0
        // in here, the user is the one who requesting the message (thus to)
        // the instruction is osmsg -r 
        char from[33]; 
        char msg[1024];
        while (get_msg(user, msg, from) == 1) { // there are message to be read
            printf("%s said: %s\n", from, msg);
        }
        
        return 0; 
    }
    // shouldn't come here at all 
    return -1; 

    
}
