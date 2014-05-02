

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <time.h>


#include "peiskernel_mt_js.h"
#include "peiskernel_mt.h"


extern int noStopRequest;

void *ThreadFunc( void* param )
{

	int i =0;
	struct timespec interval, remainder; 	
	interval.tv_sec =1; 
	interval.tv_nsec = 1000000 *1; // 1 second * X
	
	printf("Sender is sending tuples into PeisKernelBroker...\n");


	while( noStopRequest ) {

		nanosleep(&interval, &remainder);
		
		char *key="SonarInfo";
		char *data = "01 20 02 11 This is sender message";

		peiskmt_setTuple(key, strlen(data)+1, (void*) data);

	}
	
	printf("Sender C POSIX thread returned..\n");
	return;
	
} // end of ThreadFunc


int main( int argc, char **args )
{
	
	pthread_t thread1;
	int iret, status;
    
	printf("PeisKernelProker being initialized by JavaSpacePlug.c\n");
	peiskmt_initialize(&argc,args);
	set_peis_info("Sender-PEIS");
	
	while ( !peiskmt_js_alive() ) {
		sleep(3);
	}
	
	iret = pthread_create(&thread1, NULL, ThreadFunc, NULL );
	// wait for the thread to finish 
	status = pthread_join(thread1, NULL);
	if ( status != 0 )
		printf("Join failed\n");
	else 
       printf("Join succeed\n");
       
    peiskmt_shutdown();

	//exit(0);
}

