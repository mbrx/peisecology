
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h> 
#include <unistd.h>
#include <time.h>

#include "peiskernel_mt_js.h"
#include "peiskernel_mt.h"
#include "JavaSpacePlug.h"

extern int noStopRequest;
peiskmt_callbackHandle handle;
peiskmt_subscriptionHandle subHandle;

void actCallback(char *key,int owner,int len,void *data,void *userdata) {

	printf("Callback key : %s\n", key);
	printf("Callback owner : %d\n", owner);
	printf("Callback length : %d\n", len);
	printf("Callback data : %s\n", (char*)data);	
}



void *ThreadFunc( void* param )
{
	
	struct timespec interval, remainder; 	
	interval.tv_sec = 2; 
	interval.tv_nsec = 1000000 * 2;  // 1 second * X


	char key[256] = "SonarInfo";
	char *result;
	int len;
	
	int size, i;
	Tuple** tuples;
	
	while (noStopRequest) {
		/*
		nanosleep(&interval, &remainder);	
		peiskmt_getTuple(key, 0, &len, (void**)&result, 0);
		if ( result != NULL ) {
			printf("Result : =%s\n", result);
			printf("len = %d\n", len);
		}
		*/
		
		/*nanosleep(&interval, &remainder);
		
	
		size = 0;

		size = getPeisInfoTuples( (Tuple_ptr *)&tuples );	

		if ( size == 0 )
			return ;
			
		for ( i = 0 ; i < size ; i ++ ) {
			printf("atoi(tuples[%]->peisId = %d\n", i, atoi(tuples[i]->peisId));
			printf("tuples[%d].->value=%s\n", i, tuples[i]->value);
			free(tuples[i]);
		} // end 
		*/	
	}// end of while
	
	
	peiskmt_unsubscribe(subHandle);
	peiskmt_unregisterTupleCallback(handle);
	printf("Receiver C POSIX thread returned..\n");
	return;

} // end of ThreadFunc


void subscribeT(){
	
	char* key = "SonarInfo";
//	int owner = peiskmt_peisid();
	//printf("SubscribeT() : owner --> %d\n", peiskmt_peisid());
	subHandle = peiskmt_subscribe(key, -1);
	handle = peiskmt_registerTupleCallback(key, peiskmt_peisid(), NULL, (void *)actCallback);	
	peiskmt_hasSubscriber(key);	
	
} // end of subscribeT()


int main( int argc, char **args )
{

        pthread_t thread1;
        int iret, status ;
        
		printf("PeisKernelProker being initialized by JavaSpacePrlug.c ---->\n");
		peiskmt_initialize(&argc,args);
		set_peis_info("Receiver-PEIS");
		
		while ( !peiskmt_js_alive() ) {
			sleep(3);
		}
		
		subscribeT();  
       
     //   peiskmt_unsubscribe(subHandle);
       
        iret = pthread_create(&thread1, NULL, ThreadFunc, NULL );    
      	//wait for the thread to finish
 
  		
        status = pthread_join(thread1, NULL);        
		if ( status != 0 )
			printf("Join failed\n");
		else 
           printf("Join succeed\n");
      	
      	peiskmt_shutdown();   
        //while(1);
		//exit(0);
} // end of main
