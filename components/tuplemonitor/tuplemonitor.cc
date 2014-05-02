#include <cstdlib>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include <peiskernel/peiskernel_mt.h>
#include <peiskernel/tuples.h>
#include <peiskernel/peiskernel.h>
}

void printUsage(int argc, char **argv) {
  printf("tuplemonitor\n"
	 "   --monitor-values <heading> <peis-id> <tuple-name>     Adds a monitor\n"
	 "   --monitor-ticks <heading> <peis-id> <tuple-name>      Adds a monitor that doesn't show the value only ticks\n");
  peiskmt_printUsage(stderr,argc,argv);
  exit(0);
}

#define MAX_CHANGES 100
#define MAX_MONITORS 100

typedef struct MonitorData {
  int id, nchanges, showValues;
  char *heading;
  char *tupleName;
  char *status[MAX_CHANGES];
  double changeTimepoint[MAX_CHANGES];
} MonitorData;

MonitorData monitors[MAX_MONITORS];
int nmonitors;

void statusChangeCallback(PeisTuple *tuple, void *userdata) {
  int i;
  char tupleName[256];
  /* Note that we are running NOT multithreaded inside a callback */
  if(peisk_getTupleName(tuple,tupleName,sizeof(tupleName)) != 0) return;

  for(i=0;i<nmonitors;i++) {
    if(monitors[i].id == tuple->owner &&
       strcmp(monitors[i].tupleName, tupleName) == 0) {
      if(monitors[i].nchanges == MAX_CHANGES) continue; /* Too many value changes, stop tracking */
      if(monitors[i].nchanges == 0 ||
	 strncmp(monitors[i].status[monitors[i].nchanges-1],tuple->data,tuple->datalen) != 0) {
	/* This is a new value, add it to list of changes */
	monitors[i].status[monitors[i].nchanges] = strdup(tuple->data);
	monitors[i].changeTimepoint[monitors[i].nchanges] = tuple->ts_write[0] + 1e-6*tuple->ts_write[1];
	monitors[i].nchanges++;
	printf("new value %s for monitor %d\n",tuple->data,i);
      }
      break;
    }
  }
}

#define N_VALUECOLOURS 5
const char *valueColours[N_VALUECOLOURS][2]={
  {"on", "gray(0.8)"},
  {"off", "gray(0.4)"},
  {"waiting", "yellow"},
  {"running", "green"},
  {"error", "red"},
};

int main(int argc, char **argv) {
  int i,j;

  nmonitors=0;
  peiskmt_initialize(&argc, argv);	
  peiskmt_setStringTuple("status","running");
  for(i=0;i<argc;i++) {
    if(strcmp(argv[i],"--help") == 0) printUsage(argc,argv);
    else if(strcmp(argv[i],"--monitor") == 0 || strcmp(argv[i],"--monitor-values") == 0) {
      monitors[nmonitors].heading = strdup(argv[++i]);
      monitors[nmonitors].id = atoi(argv[++i]);
      monitors[nmonitors].tupleName = strdup(argv[++i]);
      monitors[nmonitors].nchanges = 0;
      monitors[nmonitors].showValues = 1;
      nmonitors++;
    }
    else if(strcmp(argv[i],"--monitor-ticks") == 0) {
      monitors[nmonitors].heading = strdup(argv[++i]);
      monitors[nmonitors].id = atoi(argv[++i]);
      monitors[nmonitors].tupleName = strdup(argv[++i]);
      monitors[nmonitors].nchanges = 0;
      monitors[nmonitors].showValues = 0;
      nmonitors++;
    }
  }
  double startingTime = peiskmt_gettimef();

  for(i=0;i<nmonitors;i++) {
    printf("Subscribing to: %d : %s\n",monitors[i].id,monitors[i].tupleName);
    peiskmt_subscribe(monitors[i].id,monitors[i].tupleName);
    peiskmt_registerTupleCallback(monitors[i].id,monitors[i].tupleName,NULL,(PeisTupleCallback*)statusChangeCallback);
  }

  char *buf, *c;
  buf=(char*) malloc(65536);

  while(peiskmt_isRunning()) {
    sleep(2);
    double timenow = peiskmt_gettimef();
    double timeElapsed = timenow - startingTime;
    double widthPerSecond = 5.0;
    double heightPerMonitor = 30.0;
    int width = 160 + (int) (timeElapsed * widthPerSecond);
    int height = 80 + (int) (heightPerMonitor * nmonitors);
    if(width > 1600) width=1600;
    if(height > 1200) height=1200;

    c=buf;
    c += sprintf(c,"# timeline\n");
    c += sprintf(c,"ImageSize = width:%d height:%d\n",width,height);
    c += sprintf(c,"PlotArea = width:%d height:%d left:%d bottom:%d\n",width-160,height-80,140,40);
    c += sprintf(c,"AlignBars = justify\n\n");
    c += sprintf(c,"Colors = \n");
    c += sprintf(c,"  id:default  value:gray(0.5)\n");
    c += sprintf(c,"  id:empty    value:white\n");
    for(i=0;i<N_VALUECOLOURS;i++) {
      c += sprintf(c,"  id:%s    value:%s\n",valueColours[i][0],valueColours[i][1]);
    }
    c += sprintf(c,"\nPeriod = from:0 till:%d\n",(int)timeElapsed+2);
    c += sprintf(c,"TimeAxis = orientation:horizontal\n");
    c += sprintf(c,"ScaleMajor = unit:year increment:60 start:0\n");
    c += sprintf(c,"ScaleMinor = unit:year increment:10 start:0\n");
    c += sprintf(c,"PlotData = \n");
    c += sprintf(c,"  align:left textcolor:black fontsize:8 mark:(line,black) width:28 shift:(3,1)\n\n");
    for(i=0;i<nmonitors;i++) {
      c += sprintf(c,"  bar:%s color:empty\n",monitors[i].heading);
      if(monitors[i].nchanges > 0) {
	int prev=(int)(monitors[i].changeTimepoint[0]-startingTime);
	if(prev<0) prev=0;
	for(j=0;j<monitors[i].nchanges;j++) {
	  int next=(j==monitors[i].nchanges-1?(int)timeElapsed:(int)(monitors[i].changeTimepoint[j+1]-startingTime));
	  const char *colour = "default";
	  for(int k=0;k<N_VALUECOLOURS;k++) {
	    if(strcasecmp(monitors[i].status[j],valueColours[k][0]) == 0) {colour=valueColours[k][0]; break;}
	  }
	  if(strlen(monitors[i].status[j]) > 0) {
	    c += sprintf(c,"  from: %d till: %d color:%s mark:(line,black)\n",prev,next,colour);
	    if(monitors[i].showValues)
	      c += sprintf(c,"  at: %d text: %s\n",prev,monitors[i].status[j]);
	  }
	  prev=next;
	}
      }
    }
    peiskmt_setStringTuple("timeline",buf);
  }
}
