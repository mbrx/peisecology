/** \file sim_generic.cc

    Implements a simple "generic" component for low-fidelity simulations of configurations and general input/ouput of tuples.

    Copyright (c) RUBICON 2012.

    Authors: Mathias Broxvall (ORU)
 */

#include <cstdlib>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern "C" {
#include <peiskernel/peiskernel_mt.h>
}

#define MAX_INPUTS 256
#define MAX_OUTPUTS 256
#define MAX_PARAMETERS 256
#define MAX_ACTUATORS 256
#define MAX_CONDITIONS 256

#define min(x,y) (x<y?x:y)

void printUsage(int argc, char **argv) {
  printf("sim-generic\n"
	 "   --input  <name> <float timeout>       Specifies a required input value, configured through the given meta-tuple\n"
	 "                                         input. Requires freshness of inputs within given time period.\n"
	 "   --output <name> <float period>        Specifies an output tuple, updated with given periodicity when all\n"
	 "                                         inputs and parameters are satisified.\n"
	 "   --parameter <name>                    Specfies a given parameter for which a paramter-value and paramter-meta-tuple\n"
	 "                                         are created. Initial defaults are empty, component requires non-empty values to run\n"
	 "   --actuator <basename> <starval> <per> Create an command with tuples: name.reqState, name.currState and name.status\n"
	 "   --equal <name1> <name2>               Require these two meta tuples to be identical for output to be generated\n"
	 , argv[0]);
  peiskmt_printUsage(stderr,argc,argv);
  exit(0);
}

int main(int argc, char **argv) {	
  int i;
  int ninputs, noutputs, nparameters, nactuators, nconditions;
  char *inputNames[MAX_INPUTS], *outputNames[MAX_OUTPUTS], *parameterNames[MAX_PARAMETERS], *parameterMetaNames[MAX_PARAMETERS];
  double inputTimeout[MAX_INPUTS], outputPeriod[MAX_OUTPUTS];
  double inputLastValue[MAX_INPUTS], outputLastValue[MAX_OUTPUTS];
  
  char *actuatorNames[MAX_ACTUATORS], *actuatorState[MAX_ACTUATORS], *actuatorNextState[MAX_ACTUATORS];
  double actuatorPeriod[MAX_ACTUATORS], actuatorNextTimepoint[MAX_ACTUATORS];

  char *conditionNames[MAX_CONDITIONS][2];
							      
  ninputs=0; noutputs=0; nparameters=0, nactuators=0; nconditions=0;
  char str[256];

  peiskmt_initialize(&argc, argv); 
  peiskmt_setStringTuple("status", "init");
  /* Go through all commandline arguments and generate list (a) inputs, (b) paramters and (c) outputs.
   */
  for(i=0;i<argc;i++) {
    if(strcmp(argv[i],"--help") == 0) printUsage(argc,argv);
    else if(strcmp(argv[i],"--input") == 0) {
      inputNames[ninputs] = strdup(argv[++i]);
      inputTimeout[ninputs++] = atof(argv[++i]);
    } else if(strcmp(argv[i],"--output") == 0) {
      outputNames[noutputs] = strdup(argv[++i]);
      outputPeriod[noutputs++] = atof(argv[++i]);
    } else if(strcmp(argv[i],"--parameter") == 0) {
      parameterNames[nparameters] = strdup(argv[++i]);
      snprintf(str,sizeof(str),"mi-%s",parameterNames[nparameters]);
      parameterMetaNames[nparameters++] = strdup(str);      
    } else if(strcmp(argv[i],"--actuator") == 0) {
      actuatorNames[nactuators] = strdup(argv[++i]);
      actuatorState[nactuators] = strdup(argv[++i]);
      actuatorPeriod[nactuators] = atof(argv[++i]);
      actuatorNextState[nactuators] = NULL;
      nactuators++;
    } else if(strcmp(argv[i],"--equal") == 0) {
      conditionNames[nconditions][0] = strdup(argv[++i]);
      conditionNames[nconditions][1] = strdup(argv[++i]);
      nconditions++;
    }
  }
  if(noutputs == 0 && ninputs == 0 && nparameters == 0 && nactuators == 0) {
    printf("** Refusing cowardly to run with no inputs/outputs/parameters/actuators\n");
    printUsage(argc,argv);
  }
  printf("%d inputs, %d outputs, %d parameters, %d actuators recognised\n",ninputs,noutputs,nparameters,nactuators);
  /* Setup meta-tuples for all inputs */
  for(i=0;i<ninputs;i++) {
    peiskmt_declareMetaTuple(peiskmt_peisid(),inputNames[i]);
    peiskmt_subscribeIndirectly(peiskmt_peisid(),inputNames[i]);
    inputLastValue[i]=peiskmt_gettimef();
  }
  /* Setup meta-tuples + default tuples for all parameters */
  for(i=0;i<nparameters;i++) {
    peiskmt_setDefaultMetaStringTuple(parameterNames[i],"");    
  }
  /* When next to produce outputs */
  for(i=0;i<noutputs;i++) {
    outputLastValue[i]=0.0;
  }
  peiskmt_setDefaultStringTuple("provoke-error","no");  

  /* Create initial actuator tuples */
  for(i=0;i<nactuators;i++) {
    snprintf(str,sizeof(str),"%s.reqState",actuatorNames[i]);
    peiskmt_setDefaultMetaStringTuple(str,"");  
    snprintf(str,sizeof(str),"%s.mi-reqState",actuatorNames[i]);
    peiskmt_subscribeIndirectly(peiskmt_peisid(),str);
    snprintf(str,sizeof(str),"%s.currState",actuatorNames[i]);
    peiskmt_setStringTuple(str,actuatorState[i]);
    snprintf(str,sizeof(str),"%s.status",actuatorNames[i]);
    peiskmt_setStringTuple(str,"waiting");    
  }
  /* Create/subscribe to condition tuples */
  for(i=0;i<nconditions;i++) {
    peiskmt_declareMetaTuple(peiskmt_peisid(),conditionNames[i][0]);
    peiskmt_subscribeIndirectly(peiskmt_peisid(),conditionNames[i][0]);    
    peiskmt_declareMetaTuple(peiskmt_peisid(),conditionNames[i][1]);
    peiskmt_subscribeIndirectly(peiskmt_peisid(),conditionNames[i][1]);    
  }

  int lastStatus=-1, lastParam=-1;

  while(peiskmt_isRunning()) {
    usleep(100000);
    /* Verify that all parameters are ok, or mark us as waiting */
    for(i=0;i<nparameters;i++) {
      PeisTuple *tuple = peiskmt_getTupleIndirectly(peiskmt_peisid(),parameterMetaNames[i],PEISK_KEEP_OLD | PEISK_NON_BLOCKING);
      if(!tuple || tuple->datalen <= 1) {
	if(lastStatus != 1 || lastParam != i) {
	  lastStatus = 1;
	  lastParam = i;
	  peiskmt_setStringTuple("status","waiting");
	  peiskmt_setStringTuple("cause.names",parameterNames[i]);
	  peiskmt_setStringTuple("cause","bad-parameters");		
	}
	break;
      }
    }
    if(i != nparameters) continue;

    double timenow = peiskmt_gettimef();

    /* Verify that all conditions are satisfied */
    for(i=0;i<nconditions;i++) {
      PeisTuple *tuple1 = peiskmt_getTupleIndirectly(peiskmt_peisid(),conditionNames[i][0],PEISK_KEEP_OLD | PEISK_NON_BLOCKING);
      PeisTuple *tuple2 = peiskmt_getTupleIndirectly(peiskmt_peisid(),conditionNames[i][1],PEISK_KEEP_OLD | PEISK_NON_BLOCKING);
      if(!tuple1 || !tuple2 || strncmp(tuple1->data,tuple2->data,min(tuple1->datalen,tuple2->datalen)) != 0) {
	if(lastStatus != 5 || lastParam != i) {
	  lastStatus = 5;
	  peiskmt_setStringTuple("status","waiting");
	  peiskmt_setStringTuple("cause.names",conditionNames[i][0]);
	  peiskmt_setStringTuple("cause","failed-condition");		
	}
	break;
      }
    }
    if(i != nconditions) continue;

    /* Verify that we have all inputs */
    for(i=0;i<ninputs;i++) {
      PeisTuple *tuple = peiskmt_getTupleIndirectly(peiskmt_peisid(),inputNames[i],PEISK_FILTER_OLD | PEISK_NON_BLOCKING);
      if(tuple) { 
	inputLastValue[i]=fmax(inputLastValue[i],tuple->ts_write[0] + 1e-6 * tuple->ts_write[1]);
	printf("Got input from time %.3fs (now is %.3fs)\n",fmod(inputLastValue[i],1000.0),fmod(timenow,1000.0));
      }
      if(timenow - inputLastValue[i] > inputTimeout[i]) {
	printf("* Too old, last time time %.3fs (now is %.3fs)\n",fmod(inputLastValue[i],1000.0),fmod(timenow,1000.0));
	if(lastStatus != 2 || lastParam != i) {
	  lastStatus = 2;
	  lastParam = i;
	  peiskmt_setStringTuple("status","waiting");
	  peiskmt_setStringTuple("cause.names",inputNames[i]);
	  peiskmt_setStringTuple("cause","bad-input");		
	}
	break;
      }
    }
    if(i != ninputs) continue;
    
    /* See if we should fake an error */  
    {
      PeisTuple *tuple = peiskmt_getTuple(peiskmt_peisid(),"provoke-error",PEISK_KEEP_OLD | PEISK_NON_BLOCKING);
      if(tuple && strncmp(tuple->data,"no",tuple->datalen+1) != 0) {
	if(lastStatus != 3) {
	  lastStatus = 3;
	  peiskmt_setStringTuple("status","error");
	  peiskmt_setStringTuple("cause.names","");
	  peiskmt_setStringTuple("cause","none");		
	}
	continue;
      }
    }

    if(lastStatus != 4) {
      lastStatus = 4;
      peiskmt_setStringTuple("status","running");
      peiskmt_deleteTuple(peiskmt_peisid(),"cause");
      peiskmt_deleteTuple(peiskmt_peisid(),"cause.names");
    }

    /* Generate outputs, as appropriate */
    for(i=0;i<noutputs;i++) {
      if(outputLastValue[i] + outputPeriod[i] < timenow) {
	outputLastValue[i] = timenow;
	snprintf(str,sizeof(str),"%.6f",timenow);
	peiskmt_setStringTuple(outputNames[i],str);
      }
    }

    /* Update actuation status as needed */
    for(i=0;i<nactuators;i++) {
      snprintf(str,sizeof(str),"%s.reqState",actuatorNames[i]);
      PeisTuple *tuple = peiskmt_getTupleIndirectly(peiskmt_peisid(),str,PEISK_FILTER_OLD | PEISK_NON_BLOCKING);
      if(tuple && tuple->data[0]) {
	if(actuatorNextState[i]) { free(actuatorNextState[i]); }
	actuatorNextState[i] = strdup(tuple->data);
	actuatorNextTimepoint[i] = peiskmt_gettimef() + actuatorPeriod[i];
	snprintf(str,sizeof(str),"%s.status",actuatorNames[i]);
	peiskmt_setStringTuple(str,"running");
      }
      if(actuatorNextState[i] && actuatorNextTimepoint[i] < peiskmt_gettimef()) {
	free(actuatorState[i]);
	actuatorState[i] = actuatorNextState[i];
	actuatorNextState[i]=NULL;
	snprintf(str,sizeof(str),"%s.currState",actuatorNames[i]);
	peiskmt_setStringTuple(str,actuatorState[i]);
	snprintf(str,sizeof(str),"%s.status",actuatorNames[i]);
	peiskmt_setStringTuple(str,"waiting");
      }

    }

  }
}
