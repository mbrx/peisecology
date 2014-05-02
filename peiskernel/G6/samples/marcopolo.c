#include <unistd.h>
#include <stdio.h>
#include <peiskernel/peiskernel_mt.h>

/* This is an example callback that echos listens to the input tuples
   marco.X where X can be anything and echos the data to the tuple
   'polo' 

   Note especially that callback functions should use the
   NON-MULTITHREADED versions of the API since they are executed from
   within the peiskernel. Also they should be fast and not take more
   than 10-20ms  */ 
void myCallbackFn(PeisTuple *tuple, void *userdata) {
  PeisTuple *myTuple = peisk_cloneTuple(tuple);
  myTuple->owner = peisk_peisid();
  peisk_setTupleName(myTuple,"polo");
  peisk_insertTuple(myTuple);
  peisk_freeTuple(myTuple);
}

int main(int argc, char **argv) {
  peiskmt_initialize(&argc, argv);  
  peiskmt_setStringTuple("greeting", "Hello world!");
  PeisCallbackHandle myCallbackHandle=peiskmt_registerTupleCallback(peiskmt_peisid(),"marco.*",NULL,myCallbackFn);

  while(peiskmt_isRunning()) {
    // PeisTuple *tuple = peiskmt_getTuple(some-id, some-string-key, PEISK_FILTER_OLD);
    // peiskmt_setRemoteTuple(some-id, some-string-key, int some-datalen, void *data, "text/plain", PEISK_ENCODING_ASCII);
    sleep(1);
  }
  peiskmt_unregisterTupleCallback(myCallbackHandle);
}
