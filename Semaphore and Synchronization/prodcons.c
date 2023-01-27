/**
 * File: prodcons.c 
 * Author: Quan Nguyen
 * Project:  Syscalls and IPC
 * Class: CSC252 
 * Purpose: This is the main interaction program utilizing semaphores. This will call two different 
 * forks for the north and the south side of the road. Car on the road will go on on for two side, 
 * if one side come first, it will go until the other side got 10 or more car. If there is no 
 * car, the flag person will sleep until he awake. 
 */ 
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <sys/syscall.h>    
#include <unistd.h>
#include <sys/mman.h> 
#include <time.h> 
// the buffer_size
#define N 10
// the car_queue max size 
#define MAX_QUEUE_SIZE 30
/* a Queue of process which is a wrapper for the task waiting to be processes*/
typedef struct queue {
	struct task_struct *task; 
	struct queue *next; 
} process_queue;
/**
 * The struct representing a simple semaphore.
 */
typedef struct csc452_sem
{
	// number of processes left to be wake up (or up)
	int value;
	// a Queue of process which is a wrapper for the task waiting to be processes 
	process_queue *process;
} csc452_sem;  

/**
 * Circular queue implementation for the car. Thanks to https://www.programiz.com/dsa/circular-queue for the 
 * provided implementation of the queue. This would help enqueue and dequeue car very easy. 
 */
typedef struct car_queue 
{
    int car[MAX_QUEUE_SIZE];  
    int front; 
    int rear; 
    int size; 
} car_queue; 
// Check if the queue is full
int is_full(car_queue *my_queue) {
if ((my_queue->front == my_queue->rear + 1) || (my_queue->front == 0 && my_queue->rear == MAX_QUEUE_SIZE - 1)) return 1;
  return 0;
}

// Check if the queue is empty
int is_empty(car_queue *my_queue) {
  if (my_queue->front == -1) return 1;
  return 0;
}

// Adding an element
void enqueue(car_queue *my_queue, int element) {
  if (is_full(my_queue))
    return;
  else {
    if (my_queue->front == -1) my_queue->front = 0;
      my_queue->rear = (my_queue->rear + 1) % MAX_QUEUE_SIZE;
      my_queue->car[my_queue->rear] = element; 
      my_queue->size = (my_queue->front > my_queue->rear) ? (MAX_QUEUE_SIZE - my_queue->front + \
                      my_queue->rear + 1) : (my_queue->rear - my_queue->front + 1);
  }
}

// Removing an element
int dequeue(car_queue *my_queue) {
  int element;
  if (is_empty(my_queue)) {
    return (-1);
  } else {
    element = my_queue->car[my_queue->front];
    if (my_queue->front == my_queue->rear) {
      my_queue->front = -1;
      my_queue->rear = -1;
    } else {
      my_queue->front = (my_queue->front + 1) % MAX_QUEUE_SIZE;
    }
    my_queue->size = (my_queue->front > my_queue->rear) ? (MAX_QUEUE_SIZE - my_queue->front + \
                      my_queue->rear + 1) : (my_queue->rear - my_queue->front + 1);
    return (element);
  }
}
/**
 * Make the function of syscall of calling down process of the semaphore more readable. 
 */
void down(struct csc452_sem *sem) {
    syscall(443, sem);
}

/**
 * Make the function of syscall of calling up process of the semaphore more readable. 
 */
void up(struct csc452_sem *sem) {
    syscall(444, sem);
}

 
int main (int argc, char *argv[]) {
  srand(time(NULL));   // Initialization for the random 
  int *total_car = (int *) mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0); 
  *total_car = 1; // initialize the total car number
  car_queue *north_queue = (car_queue *) mmap(NULL, sizeof(car_queue *), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0); 
  car_queue *south_queue = (car_queue *) mmap(NULL, sizeof(car_queue *), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0); 
  // initialize the two queue value 
  north_queue->front = -1; north_queue->rear = -1; 
  south_queue->front = -1; south_queue->rear = -1; 
  // initialize the mutex, value should be 0 
  csc452_sem *mutex = (csc452_sem *) mmap(NULL, sizeof(csc452_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  mutex->value = 1; 
  mutex->process = NULL;  
  // initialize the empty and the full semaphore of both North and South
  csc452_sem *north_empty = (csc452_sem *) mmap(NULL, sizeof(csc452_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  csc452_sem *north_full = (csc452_sem *) mmap(NULL, sizeof(csc452_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  north_empty->value = N; 
  north_empty->process = NULL; 
  north_full->value = 0; 
  north_full->process = NULL; 
  csc452_sem *south_empty = (csc452_sem *) mmap(NULL, sizeof(csc452_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  csc452_sem *south_full = (csc452_sem *) mmap(NULL, sizeof(csc452_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  south_empty->value = N; 
  south_empty->process = NULL; 
  south_full->value = 0; 
  south_full->process = NULL;
  time_t begin = time(NULL);
  // variable to keep track if the flag person is sleeping. 0 if he is not and 1 if he is 
  int is_sleep = 1; 
  printf("The flagperson is now sleep.\n");
  if(fork() == 0){
    while (1) {
        // lockdown the north queue semaphore in order to enter critical region
        down(north_empty); 
        down(mutex); 
        // get a car in the north queue 
        if (is_sleep == 1) {
          is_sleep = 0;
          printf("The flagperson is now awake\n"); 
          printf("Car %d coming from the %c direction, blew their horn at time %ld.\n",*total_car,'N',(int) time(NULL) - begin);
        } 
        printf("Car %d coming from the %c direction arrived in the queue at time %ld.\n",*total_car,'N',(int) time(NULL) - begin);
        enqueue(north_queue, *total_car); 
        // release the lock 
        up(mutex); 
        up(north_full);
        // no car coming
        if (rand()%10 >= 8) {
            sleep(20);
        } 
    }    
  } else { // I think that I should function since total_car have to be accesses
    if (fork() == 0){
      /* Producing South car */ 
      while (1) {
        // lockdown the north queue semaphore in order to enter critical region
          down(south_empty); 
          down(mutex); 
          if (is_sleep == 1) {
            is_sleep = 0;
            printf("Car %d coming from the %c direction, blew their horn at time %ld.\n", *total_car, 'S', time(NULL) - begin);
          } 
          // get a car in the south queue 
          printf("Car %d coming from the %c direction arrived in the queue at time %ld.\n", *total_car, 'S', time(NULL) - begin);
          enqueue(south_queue, *total_car++); 
          *total_car = *total_car + 1;  
          // release the lock 
          up(mutex); 
          up(south_full);
           // no car coming
          if (rand()%10 >= 8) {
            sleep(20);
          } 
      }
      } else {
      //flagperson();
      while(1) {
        // check to see if this is the right time for the man to sleep 
        if (north_queue->size == 0 && south_queue->size == 0) {
          if (is_sleep == 0) {
            is_sleep = 1; 
            printf("The flagperson is now asleep.\n"); 
          } 
        }
        // release the north side until north queue is empty or the south side is more than 10 
        while (north_queue->size > 0 && south_queue->size < 10) {
          down(north_full);
          down(mutex); 
          // release a car from the south queue 
          printf("Car %d coming from the %c direction left the construction zone at time %ld.\n", dequeue(north_queue), 'N', time(NULL) - begin);     
          // take two seconds to go through  
          sleep(2); 
          up(mutex);
          up(north_empty);
        }
         // release the south side until south queue is empty or the north side is more than 10 
        while (north_queue->size < 10 && south_queue->size > 0) {
          // consume south 
          down(south_full);
          down(mutex); 
          // release a car from the south queue
          printf("Car %d coming from the %c direction left the construction zone at time %ld.\n", dequeue(south_queue), 'S', time(NULL) - begin); 
          // take two seconds to go through
          sleep(2); 
          up(mutex);
          up(south_empty);
        }

      }
    } 
  }
  return 0; 
}


