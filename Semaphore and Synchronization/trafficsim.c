/**
	cs1550 Project 2: trafficsim.c
	Author: Jake Hauser 
	compile: gcc -m32 -o trafficsim -I /u/OSLab/jdh122/linux-2.6.23.1/include/ trafficsim.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <linux/unistd.h>
#include <asm/unistd.h>
#include <time.h>

//define syscall numbers
#define __NR_cs1550_down 443
#define __NR_cs1550_up 444

/* a Queue of process which is a wrapper for the task waiting to be processes*/
typedef struct queue {
	struct task_struct *current; 
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
	process_queue *head;
} csc452_sem; 

void down(struct csc452_sem *sem) {
 syscall(__NR_cs1550_down, sem);
}

void up(struct csc452_sem *sem) {
 syscall(__NR_cs1550_up, sem);
}

int main(int argc, char* argv[])
{
	int BUFF_SIZE = 10;
	//full, empty, mutex sempahores, as well
	struct csc452_sem *n_full, *s_full, *n_empty, *s_empty, *mutex;
	//ints to handle indexing of producer and consumer buffers
	int *n_prod, *n_con, *s_prod, *s_con;
	int *north_buf, *south_buf; //will hold index values specifying which index the buffer is at, as well as the buffer address

	//allocate space for semaphores by casting to struct csc452_sem. Space for 5 semaphores is needed
	struct csc452_sem *sem_ptr = (struct csc452_sem*) mmap(NULL, sizeof(struct csc452_sem)*5, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	//allocate space for the north and south buffers. Space allocated for the buffere size plus the index pointers
	int *north_ptr = (int*) mmap(NULL, sizeof(int)*(BUFF_SIZE+2), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	int *south_ptr = (int*) mmap(NULL, sizeof(int)*(BUFF_SIZE+2), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	/*
		buffer set up for simulation:
		semaphores are all initializid to their proper values
		linked list pointers initialized to null
		index pointers intialized to zero
	*/

	n_full = sem_ptr;
	n_full->value = 0;
	n_full->head = NULL;
	n_empty = sem_ptr + 1;
	n_empty->value = BUFF_SIZE;
	n_empty->head = NULL;
	n_prod = north_ptr;
	n_con = north_ptr + 1;
	north_buf = north_ptr + 2;
	*n_prod = 0;
	*n_con = 0;
	
	s_full = sem_ptr+2;
	s_full->value = 0;
	s_full->head = NULL;
	s_empty = sem_ptr + 3;
	s_empty->value = BUFF_SIZE;
	s_empty->head = NULL;
	s_prod = south_ptr;
	s_con = south_ptr + 1;
	south_buf = south_ptr + 2;
	*s_prod = 0;
	*s_con = 0;

	mutex = sem_ptr + 4;
	mutex->value = 1;
	mutex->head = NULL;

	//set start time at start of simulation 
	time_t start = time(NULL);
	printf("\n\nStarting Traffic Simulation\n");
	int con_sleep = 1; //integer which starts the flagperson sleeping (0 = awake)

	//north
	if(fork() == 0){

		int car;
		while(1){
			while((rand() % 10) <= 8){
				down(n_empty); //following the producer consumer pseudocode from slides
				down(mutex);
					
				car = *n_prod; //set car to current value of the index
				north_buf[*n_prod % BUFF_SIZE] = car; // % BUFF_SIZE to create circular buffer and prevent out of bounds allocation
				*n_prod = *n_prod + 1;
				time_t arrive = time(NULL); //get arrive time to calculate when car arrived
				printf("\nCar %d coming from the %c direction arrived in the queue at time %d", car, 'N', arrive-start);

				up(mutex);
				up(n_full);	
			}
			sleep(3);	//sleep 20 if the value of rand % 10 is greated than 8
		}
	}

	//south (all function is the same as the north producer)
	if(fork() == 0){

		int car;
		while(1){
			while((rand() % 10) <= 8){
				down(s_empty);
				down(mutex);
				
				car = *s_prod;
				south_buf[*s_prod % BUFF_SIZE] = car;
				*s_prod = *s_prod+1;
				time_t arrive = time(NULL);
				printf("\nCar %d coming from the %c direction arrived in the queue at time %d", car, 'S', arrive-start);

				up(mutex);
				up(s_full);	
			}
			sleep(1);	
		}
	} 		

	//consumer process
	if(fork() == 0){

		int car;
		while(1){
			
			//consumer north buffer cars while south buffer is not full 
			while(s_full->value < 10 && n_full->value > 0)	{
				down(n_full); //call down on full sem and lock the process
				down(mutex);
				//assign index value to car
				car = *n_con;
				*n_con = *n_con + 1;

				time_t arrive = time(NULL);
				//if flagperson is alssep, beep horn to wake them up 
				if(con_sleep == 1){
					con_sleep = 0;
					printf("\nCar %d Coming from the %c direction blew their horn at time %d", car, 'N', arrive - start);
					printf("\nThe flagperson is now awake\n");
					printf("\nCar %d Coming from the %c direction entered the construction zone at time %d", car, 'N', arrive - start);	
				}else{
					printf("\nCar %d Coming from the %c direction entered the construction zone at time %d", car, 'N', arrive - start);
				}
				//sleep 2 to represent car passing through the construction zone 
				sleep(3);

				time_t endtime = time(NULL);
				printf("\nCar %d Coming from the %c direction left the construction zone at time %d", car, 'N', endtime - start);
				up(mutex);  //release lock and call up on semaphore
				up(n_empty);

			}
			//after consumer process, if all buffers empty, set flagperson to sleep 
			if(n_full->value == 0 && s_full->value == 0 && con_sleep == 0){
				printf("\nThe flagperson is now asleep");
				con_sleep = 1;
			}

			//the entire process is the same here, except it deals with consumption of south buffer cars 
			while(n_full->value < 10 && s_full->value > 0){
				down(s_full);
				down(mutex);


				car = *s_con;
				*s_con = *s_con + 1;

				time_t arrive = time(NULL);
				if(con_sleep == 1){
					con_sleep = 0;
					printf("\nCar %d Coming from the %c direction blew their horn at time %d", car, 'S', arrive - start);
					printf("\nThe flagperson is now awake\n");
					printf("\nCar %d Coming from the %c direction entered the construction zone at time %d", car, 'S', arrive - start);
				}else{
					printf("\nCar %d Coming from the %c direction entered the construction zone at time %d", car, 'S', arrive - start);
				}

				sleep(2);

				time_t endtime = time(NULL);
				printf("\nCar %d Coming from the %c direction left the construction zone at time %d", car, 'S', endtime - start);

				up(mutex);
				up(s_empty);
			}	

			if(n_full->value == 0 && s_full->value == 0 && con_sleep == 0){
				printf("\nThe flagperson is now asleep");
				con_sleep = 1;
			}
		}
	}
    int waiting; 
    wait(&waiting); 
    return 0; 
}