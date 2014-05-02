/** \file ecologyView.c */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "peiskernel/peiskernel.h"
#include "peiskernel/peiskernel_private.h"
#include "peiskernel/hashtable.h"
#include "peiskernel/tuples.h"

#include <time.h>
#include <string.h>

#include "tupleview.h"
#include "callbacks.h"
#include "support.h"
#include "tokenizer.h"

#include <math.h>
#include <pthread.h>

#define CIRCO 0
#define DOT 1
#define FDP 2

int topologyImageExecutionFinished=0;
int topologyIsUpdating=0;
int topologyAbortExecution=0;

void executeCommandThread(void *data) {
  system((char*) data);
  topologyImageExecutionFinished=1;
}

void on_topologyUpdate_clicked (GtkToolButton   *toolbutton,ToplevelWindow *tl) {
  /* Update the intervall that the topology should be automatically
     updated */
  int i=0;
  GtkComboBox *update_combo=GTK_COMBO_BOX(gtk_builder_get_object(tl->builder,"topologyUpdateInterval"));
  if(update_combo) {
    i=gtk_combo_box_get_active(update_combo);
    switch(i) {
    case 0: gPreferences.topologyUpdateInterval = 0; break;
    case 1: gPreferences.topologyUpdateInterval = 50; break;
    case 2: gPreferences.topologyUpdateInterval = 100; break;
    case 3: gPreferences.topologyUpdateInterval = 200; break;
    case 4: gPreferences.topologyUpdateInterval = 500; break;
    case 5: gPreferences.topologyUpdateInterval = 1000; break;
    case 6: gPreferences.topologyUpdateInterval = 2000; break;
    }
  }

  topologyUpdateView(tl);
}

void hostInfo2shortName(PeisHostInfo *hostInfo,char *shortname) {
  int j;

  for(j=0;j<255;j++) {
    if(hostInfo->fullname[j] == '@') break;
    if(!hostInfo->fullname[j]) break;
    shortname[j]=hostInfo->fullname[j];
  }
  shortname[j++]='\\';
  shortname[j++]='n';
  for(;j<255;j++) {
    shortname[j]=hostInfo->fullname[j-2];
    if(!hostInfo->fullname[j-2]) break;
  }
  shortname[j++]=0;
}
void hostInfo2comboName(PeisHostInfo *hostInfo,char *str) {
  char *fullname=hostInfo->fullname;
  int k;
  for(k=0;k<256&&fullname[k]!='@'&&fullname[k];k++) str[k]=fullname[k];
  snprintf(str+k,512-k," (%d)",hostInfo->id);
}

void repopulateEcologyview(ToplevelWindow *tl) {
  int i,k,id;;
  static int highestEntry=0;
  PeisHostInfo *hostInfo;
  PeisHashTableIterator iterator;
  char str[512];

  /* Repopulate the routingtable combobox */
  GtkComboBox *combo=GTK_COMBO_BOX(gtk_builder_get_object(tl->builder,"ecologyRoutingTableCombo"));
  //i=gtk_combo_box_get_active(combo);

  for(i=highestEntry;i>=0;i--)
    gtk_combo_box_remove_text(combo,i);
  highestEntry=0;

  gtk_combo_box_append_text(combo, " ");
  highestEntry++;

  peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator);
  for(;peisk_hashTableIterator_next(&iterator);) {
    peisk_hashTableIterator_value(&iterator,(void*)&id,(void**)&hostInfo); 
    hostInfo2comboName(hostInfo,str);
    gtk_combo_box_append_text(combo, str);
    highestEntry++;
  }
}

void ecologyviewPageActivated(ToplevelWindow *tl) {
  repopulateEcologyview(tl);
}


void topologyUpdateView(ToplevelWindow *tl){
  int i,j;
  char *arrow;
  GtkToggleToolButton *cb;
  int subscriber;
  int owner;
  char key[256];
  int retval;
  char *buffer;
  PeisTuple *tuple;
  PeisTuple prototype;
  PeisTupleResultSet *resultSet;
  int method=CIRCO;
  char shortname[256];
  char tmp[512];
  char peisinit_string[] = "peisinit";
  int peisinit_ids[50]; /* Assumes that we will never have more than
			   50 peisinits */
  int peisinit_counter = 0;
  int tupleLen;
  void *tupleVal;
  GtkProgressBar *progress;
  static int prevDotFileSize = 0;
  time_t now; 

  int undirected=0;
  int useSimpleNodes=0;
  int showSubscriptions=0;
  int showConnections=1;
  int showTraffic=1;
  int clusterComponents=0;
  PeisSubscriberHandle trafficSubscription;
  PeisSubscriberHandle routingSubscription;
  PeisSubscriberHandle subscription;

  int id;
  PeisHostInfo *hostInfo;
  PeisHashTableIterator iterator,iterator2;

  printf("Re-rendering ecologyview for window %d\n",(int)(tl-windows));

  if(topologyIsUpdating) return; /* Avoid recursion */
  progress = GTK_PROGRESS_BAR(gtk_builder_get_object(tl->builder,"ecologyProgressBar"));
  
  cb = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(tl->builder,"ecologyDetailedNodes"));
  useSimpleNodes = !gtk_toggle_tool_button_get_active(cb);
  cb = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(tl->builder,"ecologyShowSubscriptions"));
  showSubscriptions = gtk_toggle_tool_button_get_active(cb);
  cb = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(tl->builder,"ecologyShowConnections"));
  showConnections = gtk_toggle_tool_button_get_active(cb);
  cb = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(tl->builder,"ecologyShowTraffic"));
  showTraffic = gtk_toggle_tool_button_get_active(cb);
  cb = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(tl->builder,"ecologyClusterComponents"));
  clusterComponents = gtk_toggle_tool_button_get_active(cb);

  GtkComboBox *combo=GTK_COMBO_BOX(gtk_builder_get_object(tl->builder,"renderingEngine"));
  if(combo) {
    i=gtk_combo_box_get_active(combo);
    switch(i) {
    case 0: method=CIRCO; break;
    case 1: method=DOT; break;
    case 2: method=FDP; break;
    }
  }

  /* Initialize peisinit_ids */
  for(i=0;i<50;i++){
    peisinit_ids[i]=0;
  }
    
  gtk_progress_bar_set_text(progress,"Updating topology");
  gtk_progress_bar_pulse(progress);

  /* If we need the network traffic, subscribe to it and unsubscribe
     immediatly (guarantees to only get the traffics last reading, not
     including any change induced by the transmission of itself). */
  if(showTraffic || showConnections) {
    trafficSubscription = peisk_subscribe(-1,"kernel.connections");
    peisk_unsubscribe(trafficSubscription);
  }

  /* If we want detailed notes, subscribe and unsubscribe to detail tuples */
  if(!useSimpleNodes) {
    subscription = peisk_subscribe(-1,"kernel.user");
    peisk_unsubscribe(subscription);
    subscription = peisk_subscribe(-1,"kernel.version");
    peisk_unsubscribe(subscription);
  }

  static int statusSubscription=-1;
  if(statusSubscription == -1) statusSubscription = peisk_subscribe(-1,"status");


  /*                           */
  /*    Print report to file   */
  /*                           */
  FILE *fp=fopen(topologyDotFile,"wb");
  if(!fp) { printf("Failed to open %s for writing\n",topologyDotFile); return; }

  /* What type of graphs to create */
  fprintf(fp,"digraph {\n  mindist=1.0; splines=true;\n");
  arrow = "->";

  /* Give detailed information for all known hosts */
  int cluster;
  
  PeisHashTable *reportedHosts = peisk_hashTable_create(PeisHashTableKey_String);
  int clusterNameLen; char *clusterName;
  int hostnameLen; char *hostname;
  void *vtmp=NULL;

  /* Iterate over all hosts, these are potential "clusters" of components (eg. belonging to the same host) */
  peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator);
  for(;peisk_hashTableIterator_next(&iterator);) {
    peisk_hashTableIterator_value(&iterator,(void*)&id,(void**)&hostInfo);
    if(peisk_hashTable_getValue(reportedHosts,(void*)hostInfo->hostname,(void**)&vtmp) != 0) {
      /* Host have not already been reported */
      /* Mark as reported */
      peisk_hashTable_insert(reportedHosts,(void*)hostInfo->hostname,(void*)vtmp);
      /* Start this DOT group */
      clusterName = hostInfo->hostname;
      if(clusterComponents)
	fprintf(fp," subgraph \"cluster_%s\" { label=\"%s\" style=filled; color=lightgrey; \n",clusterName,clusterName);
      /* Iterate over all components (again), report those that belong to this host. */
      peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator2);
      for(;peisk_hashTableIterator_next(&iterator2);) {
	peisk_hashTableIterator_value(&iterator2,(void*)&id,(void**)&hostInfo);
	/* Skip hosts that do not belong to this group */
	if(strcasecmp(hostInfo->hostname,clusterName) != 0) continue;
	/* Skip ourself if preferences say so */
	if(gPreferences.excludeTupleview && hostInfo->id == peisk_id) continue;
	/* Skip peisinits and all tupleviews if preferences say so */
	if(!showConnections && !showTraffic &&
	   ((gPreferences.excludePeisInits && strncmp(peisinit_string, hostInfo->fullname, 8) == 0) ||
	    (gPreferences.excludeTupleview && strncmp("tupleview", hostInfo->fullname, 9) == 0))) {
	  peisinit_ids[peisinit_counter]=hostInfo->id;
	  peisinit_counter++;
	  continue;
	}
	/* Generate the shortName for this host */
	hostInfo2shortName(hostInfo,shortname);
	
	/* Generate this node */
	if(useSimpleNodes) {	    
	  fprintf(fp,"  %d [label=\"%d\\n%s\" ",hostInfo->id,hostInfo->id,shortname);
	} else {
	  fprintf(fp,"  %d [shape=record label=\"%d | %s ",hostInfo->id,hostInfo->id,shortname);	    
	  tuple = peisk_getTuple(hostInfo->id,"kernel.version",PEISK_KEEP_OLD);
	  if(tuple)
	    fprintf(fp,"|%s",tuple->data);
	  tuple = peisk_getTuple(hostInfo->id,"kernel.user",PEISK_KEEP_OLD);
	  if(tuple)
	    fprintf(fp,"|%s",tuple->data);
	  fprintf(fp,"\"");
	}
	fprintf(fp," style=filled ");
	PeisTuple *statusTuple=peisk_getTuple(hostInfo->id,"status",PEISK_KEEP_OLD|PEISK_NON_BLOCKING);
	if(hostInfo->id == peisk_id) {
	  /* Mark our own host lightgreen */
	  fprintf(fp,"fillcolor=\"palegreen\" ");
	} else if(!peisk_getTuple(hostInfo->id,"kernel.all-keys",PEISK_KEEP_OLD|PEISK_NON_BLOCKING)) {
	  /* Nodes where we have no working tuple communication red  */
	  fprintf(fp,"fillcolor=\"palevioletred\" ");
	} else if(!statusTuple) {
	  /* Make components without status tuples pink to "encourage" change... */
	  fprintf(fp,"fillcolor=\"deeppink\" ");
	} else if(strcmp(statusTuple->data,"waiting") == 0) {
	  fprintf(fp,"fillcolor=\"gold\" ");
	} else if(strcmp(statusTuple->data,"running") == 0) {
	  fprintf(fp,"fillcolor=\"limegreen\" ");
	}  else {		  
	  /* And all other hosts lightblue */
	  fprintf(fp,"fillcolor=\"orangered\" ");
	}
	fprintf(fp,"pos=\"%d %d\"];\n",(i%3)*30,(i/3)*30);
      }
      if(clusterComponents) 
	fprintf(fp,"}\n");
    }
  }

  /* Free the hashtable we used to cluster the hosts */
  peisk_hashTable_delete(reportedHosts);
  
  /* Draw traffic arrows */
  if(showTraffic || showConnections) {
    GtkSpinButton *thresholdButton = GTK_SPIN_BUTTON(gtk_builder_get_object(tl->builder,"ecologyTrafficThreshold"));   
    int dataThreshold = gtk_spin_button_get_value(thresholdButton) * 1000;
    PeisTupleResultSet *rs = peisk_createResultSet();
    PeisTuple prototype;
    peisk_initAbstractTuple(&prototype);
    prototype.owner = -1;
    peisk_setTupleName(&prototype,"kernel.connections");
    peisk_getTuplesByAbstract(&prototype,rs);
    for(;peisk_resultSetNext(rs);) {
      PeisTuple *tuple = peisk_resultSetValue(rs);
      if(tuple) {
	int from = tuple->owner;
	int to, inData,outData;
	float dropRate;
	/* Parse content of this tuple */
	lineno=0;
	char *data = tuple->data;
	char token[256];
	if(sgetToken(&data,token,sizeof(token))) continue;
	if(strcmp(token,"(") != 0) {
	  printf("Warning (1), Invalid connections tuple from %d. At %d: %s\n",tuple->owner,lineno,tuple->data);
	}
	while(1) {
	  if(sgetToken(&data,token,sizeof(token))) break;
	  if(strcmp(token,")") == 0) break;
	  if(strcmp(token,"(") == 0) {
	    /* TO */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning (2), Invalid connections tuple from %d. At %d: %s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    to = atoi(token);

	    /* IN */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning (3), Invalid connections tuple from %d. At %d: %s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    inData = atoi(token);

	    /* OUT */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning (4), Invalid connections tuple from %d. At %d: %s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    outData = atoi(token);

	    /* TYPE */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning (4b), Invalid connections tuple from %d. At %d: %s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    int conntype = atoi(token);

	    /* DROP */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning (4), Invalid connections tuple from %d. At %d: %s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    dropRate = atof(token);

	    /* Final paranthesis */
	    if(sgetToken(&data,token,sizeof(token)) || strcmp(token,")")) {
	      printf("Warning (6), Invalid connections tuple from %d. At %d: %s\ntoken=%s\n",tuple->owner,lineno,tuple->data,token);	      
	      break;
	    }

	    double hue = (log(outData)/log(2) - log(dataThreshold)/log(2) + 1) / 10.0;
	    if(hue < 0.0) hue = 0.0;
	    else if(hue > 1.0) hue = 1.0;

	    if(showTraffic) {
	      if(outData > dataThreshold)
		fprintf(fp," %d -> %d [label=\" %d kBps \",color=\"%.3f 1.0 1.0\",fontsize=9,style=bold];\n",
			from,to,outData/1000,hue);
	    }
	    if(showConnections) {
	      /* Skip ourself if preferences say so */
	      if(gPreferences.excludeTupleview && (from == peisk_id || to == peisk_id)) continue;
	      if(to < from)
		fprintf(fp,"  %d %s %d [arrowhead=none,arrowtail=none,style=bold,color=\"gray\"];\n",from,arrow,to);
	    }
	  } else {
 	    printf("Warning, Invalid connections tuple from %d. At %d: %s\n",tuple->owner,lineno,tuple->data);
	    break;
	  }
	}
      } else {
	printf("wtf?\n");
      }      
    }
    peisk_deleteResultSet(rs);
  }

  /* Add subscription arrows */
  if(showSubscriptions) {
    /* We do this in two steps, first iterate over all tuples and
       rows. Adding the subscriber,owner to the list of edges and key
       name into the label. If the edge already exists, then append
       this key to the label at a new line. */
    int nSubscriptions=0;
    char *labels[1024];
    int edges[1024][2];

    /* Step 1, iterate over all tuples and all rows in the tuples */
    resultSet=peisk_createResultSet();
    peisk_initAbstractTuple(&prototype);
    peisk_setTupleName(&prototype,"kernel.subscribers");
    peisk_getTuplesByAbstract(&prototype,resultSet);
    while(peisk_resultSetNext(resultSet)) {
      tuple = peisk_resultSetValue(resultSet);
      buffer = tuple->data;
      if(!buffer || buffer[0] != '(' || buffer[1] != '\n') continue;
      buffer+=2; // Get rid of leading (\n 
      while(*buffer == ' ') {
	retval=sscanf(buffer," ( %d %d.%32s )",&subscriber,&owner,key);
	if((owner == -1 && subscriber == tuple->owner) || owner == tuple->owner) {
	  /* Fix to make sure we have one wildcard per subscriber (when
	     needed). Makes graph cleaner */
	  if(owner == -1) owner = -subscriber; 
	  //printf("input: %s, owner=%d
	  //subscriber=%d\n",buffer,owner,subscriber);

	  if(retval == 3) {
	    /* See if edge already exists */
	    for(i=0;i<nSubscriptions;i++)
	      if(edges[i][0] == subscriber && edges[i][1] == owner) break;
	    if(i == nSubscriptions) {
	      /* It was a new edge */
	      labels[i]=strdup(key);
	      strcpy(key,labels[i]);
	      edges[i][0]=subscriber;
	      edges[i][1]=owner;
	      nSubscriptions++;
	    } else {
	      labels[i]=realloc(labels[i],strlen(labels[i])+1+strlen(key)+2);
	      strcat(labels[i],"\\n");
	      strcat(labels[i],key);
	    }
	  }
	}
	/* Skip to end of line */
	while(*buffer && *buffer != '\n') buffer++;
	buffer++;
      }
    }
    /* Step 2, generete the output of edges */
    for(i=0;i<nSubscriptions;i++) {

      /* Skip ourself is preferences say so */
      if(gPreferences.excludeTupleview &&  edges[i][0] == peisk_id) continue;

      /* Skip peisinits and all tupleviews if preferences say so */
      if(((gPreferences.excludeTupleview && strncmp("tupleview", hostInfo->fullname, 9) == 0))) continue;


      /* Skip peisinits if preferences say so, not when connections active */
      if(gPreferences.excludePeisInits){
	int peisinit_exist = 0;
	for(j=0;j<peisinit_counter;j++){
	  /*	  printf("\npeisinit_ids[%d] = %d  edges[%d][1] = %d",
		  j, peisinit_ids[j], i, edges[i][1]); */
	  if(edges[i][1] == peisinit_ids[j]) {
	    /*printf("This is a peisinit");*/
	    peisinit_exist = 1;	  
	  }
	}
	if (peisinit_exist) continue;
      }

      /* Check if we should create a wildcard node for this edge */
      if(edges[i][1] < 0) {
	for(j=0;j<i;j++) if(edges[j][1] == edges[i][1]) break;
	if(j == i) {
	  if(edges[i][1] < 0) {
	    fprintf(fp,"  %d [label=\"*\",style=filled,fillcolor=gray];\n",edges[i][1]);
	  }
	}
      }

      /* Output edges */
      fprintf(fp,"  %d -> %d [label=\"%s\",color=\"blue\",fontcolor=\"blue\",fontsize=8,shape=none,style=bold];\n",edges[i][1],edges[i][0],labels[i]);
    }
    peisk_deleteResultSet(resultSet);
  }

  /* Draw routingTables, if requested */
  GtkComboBox *ecologyRoutingTable=GTK_COMBO_BOX(gtk_builder_get_object(tl->builder,"ecologyRoutingTableCombo"));
  i=gtk_combo_box_get_active(ecologyRoutingTable);

  int routingDestination=-1;
  if(i>0) {
    char *text = gtk_combo_box_get_active_text(ecologyRoutingTable);
    routingDestination=-1;
    /* User has selected a routing entry in the dropdown list, figure out which PEIS it corresponds to */
    peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator);
    for(;peisk_hashTableIterator_next(&iterator)&&i>0;i--) { 
      peisk_hashTableIterator_value(&iterator,(void*)&id,(void**)&hostInfo); 
      hostInfo2comboName(hostInfo,tmp);
      if(strcmp(tmp,text) == 0) routingDestination=id;
    }
  }
  else if(i == -1) {
    /* User has manually given a PEIS id - parse it */
    int manualDest;
    char *text = gtk_combo_box_get_active_text(ecologyRoutingTable);
    if(text) 
    if(sscanf(text,"%d",&manualDest) == 1)
      routingDestination = manualDest;
  }
  if(routingDestination != -1) {
    /* Routing destination given, trace arrows and hops to it */
    /*printf("Tracing route to %d\n",routingDestination);*/

    routingSubscription = peisk_subscribe(-1,"kernel.routingTable");
    peisk_unsubscribe(routingSubscription);
    PeisTupleResultSet *rs = peisk_createResultSet();
    PeisTuple prototype;
    peisk_initAbstractTuple(&prototype);
    prototype.owner = -1;
    peisk_setTupleName(&prototype,"kernel.routingTable");
    peisk_getTuplesByAbstract(&prototype,rs);
    for(;peisk_resultSetNext(rs);) {
      PeisTuple *tuple = peisk_resultSetValue(rs);
      if(tuple && tuple->owner != routingDestination) {
	int from = tuple->owner;
	char *data = tuple->data;
	char token[256];
	lineno=1;
	if(data[0] != '(') continue;
	data++;
	while(1) {
	  int dest, hops, via;
	  if(sgetToken(&data,token,sizeof(token))) break;
	  if(strcmp(token,")") == 0) break;
	  if(strcmp(token,"(") == 0) {
	    /* DEST */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning, Invalid routing tuple from %d. At %d:\n%s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    dest = atoi(token);

	    /* HOPS */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning, Invalid routing tuple from %d. At %d:\n%s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    hops = atoi(token);

	    /* VIA */
	    if(sgetToken(&data,token,sizeof(token))) {
	      printf("Warning, Invalid routing tuple from %d. At %d:\n%s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }
	    via = atoi(token);

	    /* Final paranthesis */
	    if(sgetToken(&data,token,sizeof(token)) || strcmp(token,")")) {
	      printf("Warning, Invalid routingTable tuple from %d. At %d:\n%s\n",tuple->owner,lineno,tuple->data);	      
	      break;
	    }

	  } else {
	    printf("Unexpected data while parsing\n");
	    break;  /* Unexpected data while parsing */
	  }

	  if(dest == routingDestination) {
	    fprintf(fp,"%d -> %d [label=\"%d hops\",color=\"gray\"];\n",from,via,hops);
	  }
	}
	
      }
    }
  }




  /* Finished creating .dot file */
  fprintf(fp,"}\n");
  fclose(fp);

  /* ONLY render new png/svg and display it (and save a copy of .dot 
     and .png/.svg if this is selected in the preferences,
     and if the filesize of the dot files are different */

  struct stat stat_p;
  if ( -1 ==  stat(topologyDotFile, &stat_p))	
  {
    printf(" Error occoured attempting to stat %s\n", topologyDotFile);
    exit(0);
  }
  int currDotFileSize = stat_p.st_size;

  if(currDotFileSize != prevDotFileSize || gPreferences.topologyUpdateInterval == 0){
    /*printf("-- New topology --\n");*/


    char command[256];
    /* Select between png and svg */
    char *outputFormat;
    switch(gPreferences.renderingFormat) {
    case FORMAT_PNG: outputFormat="png"; break;
    case FORMAT_SVG: outputFormat="svg"; break;
    default: outputFormat="png";
    }
  
    if(gPreferences.updateTopologyView || gPreferences.copyTopologyImages){      
      char *renderer;
          
      if(method==DOT)
	renderer="dot";
      else if(method == FDP) 
	renderer="fdp";
      else 
	renderer="circo";
      snprintf(command,sizeof(command),"%s %s -T%s -Nfontname=courier -Efontname=courier > %s",
	       renderer,topologyDotFile,outputFormat,topologyImageName);

      pthread_t executionThread;
      topologyIsUpdating=1;
      topologyImageExecutionFinished=0;
      topologyAbortExecution=0;
      // todo. enable/disable showing of this button
      //cancelButton = GTK_TOOL_BUTTON(gtk_builder_get_object(tl->builder,"topologyCancel")); 
      pthread_create(&executionThread,NULL,(void * (*)(void *)) executeCommandThread,(void*) command);
      gtk_progress_bar_set_pulse_step(progress,0.01);

      while(!topologyImageExecutionFinished) {
	peisk_wait(50000);
	gtk_progress_bar_pulse(progress);
	gtk_main_iteration();
	if(topologyAbortExecution) {
	  pthread_cancel(executionThread);
	  snprintf(command,sizeof(command),"skill -9 %s",renderer);
	  system(command);
	  topologyAbortExecution=0;
	  break;
	}
      } 
      topologyImageOk=1;
      topologyImageIsNew=1;
      topologyIsUpdating=0;
      
      /* Generate exposure event */
      if(gPreferences.updateTopologyView){
	GtkWidget *topologyDrawingArea = GTK_WIDGET(gtk_builder_get_object(tl->builder,"topologyDrawingArea"));  
	gtk_widget_queue_draw_area(topologyDrawingArea,0,0,5000,5000);
      }
    }
    
    if(gPreferences.copyTopologyImages || gPreferences.copyDotFiles) {
      char date[20];
      static int topologyImageCount=0;
      switch(gPreferences.imageDotFileLabeling){	
      case 0:
	/* Use Date labeling if the preferences say so */	
	time(&now);
	strftime(date, sizeof date, "%Y-%m-%d_%T\n", localtime(&now));    
	/* If user has asked to keep copies of these images then save them
	   at the local directory */     
	if(gPreferences.copyTopologyImages) {
	  /*	snprintf(command,sizeof(command),"cp %s topology-%04d.%s",topologyImageName,topologyImageCount,outputFormat);*/
	  snprintf(command,sizeof(command),"cp %s topology_%s.%s",topologyImageName,date,outputFormat);
	  system(command);
	}
	
	/* If user has asked to keep copies of dot files, then save them
	   at the local directory */
	if(gPreferences.copyDotFiles) {
	  snprintf(command,sizeof(command),"cp %s topology_%s.dot",topologyDotFile,date);
	  system(command);          
	}
	break;
      
      case 1:
	/* Use number labeling if the preferences say so */

	/* If user has asked to keep copies of these images then save them
	   at the local directory */     
	if(gPreferences.copyTopologyImages) {
	  snprintf(command,sizeof(command),"cp %s topology-%04d.%s",topologyImageName,topologyImageCount,outputFormat);	  
	  system(command);
	}
	
	/* If user has asked to keep copies of dot files, then save them
	   at the local directory */
	if(gPreferences.copyDotFiles) {
	  snprintf(command,sizeof(command),"cp %s topology_%04d.dot",topologyDotFile,topologyImageCount);
	  system(command);          
	}
	topologyImageCount++;
	break;
      }      
      
    }

    /* Generate exposure event 
    GtkWidget *topologyDrawingArea = lookup_widget(mainwindow,"topologyDrawingArea");  
    gtk_widget_queue_draw_area(topologyDrawingArea,0,0,5000,5000);
    */
  }
  prevDotFileSize = currDotFileSize;
  

  gtk_progress_bar_set_fraction(progress,1.0);
  gtk_main_iteration ();

  /* Finished generating it, progress 0% (we are finished) */
  gtk_progress_bar_set_fraction(progress,0.0);
  gtk_progress_bar_set_text(progress,"");

  /* Trigger a reset, and a new update here... so it will be ready */
  /**peisk_resetTopology();
     peisk_updateTopology();*/

}




gboolean on_topologyDrawingArea_expose_event (GtkWidget *widget,GdkEventExpose  *event,ToplevelWindow *tl) {
  int width,height;
  int oldWidth=0,oldHeight=0;
  GtkWidget *topologyDrawingArea;
  static double zoom=1.0;
  static GdkPixbuf *pixbuf;

  if(!topologyImageOk) return FALSE;
  topologyDrawingArea = GTK_WIDGET(gtk_builder_get_object(tl->builder,"topologyDrawingArea"));
  if(!topologyDrawingArea) { printf("Error, cannot find widget 'topologyDrawingArea'\n"); exit(0); }

  int windowWidth=event->area.width;
  int windowHeight=event->area.height;
  int windowSize=windowWidth<windowHeight?windowWidth:windowHeight;
  
  if(pixbuf == NULL || topologyImageIsNew || topologyImageZoom != zoom) {
    zoom = topologyImageZoom;
    topologyImageIsNew=0;
    if(pixbuf != NULL) gdk_pixbuf_unref(pixbuf);
    printf("Attempting to read pixbuf from: %s\n",topologyImageName);
    GError *error=NULL;
    pixbuf = gdk_pixbuf_new_from_file_at_scale(topologyImageName,windowSize*zoom,windowSize*zoom,TRUE,&error);    
    printf("result: %p\n",pixbuf);
    if (!pixbuf) {
      if(gPreferences.renderingFormat == FORMAT_SVG && error->code == GDK_PIXBUF_ERROR_UNKNOWN_TYPE)  {
	printf("Changing rendering format since SVG is not supported by your GDK implementation\n");
	gPreferences.renderingFormat = FORMAT_PNG;
	topologyUpdateView(tl); return;
      }
      else printf("Failed to load '%s', error was: %s (%d)\n",topologyImageName,error?error->message:"",error->code);
      return FALSE;
    }
  }

  //gdk_drawable_get_size(pixbuf,&width,&height);
  width=gdk_pixbuf_get_width(pixbuf);
  height=gdk_pixbuf_get_height(pixbuf);

  gtk_widget_get_size_request(topologyDrawingArea,&oldWidth,&oldHeight);
  if(oldWidth != width || oldHeight != height) {
    gtk_widget_set_size_request(topologyDrawingArea,width,height);    
    gtk_widget_show(topologyDrawingArea);
  }

  int screenX = event->area.x;
  int screenY = event->area.y;
  /* Only draw if the screen is actually showing a part of the pixbuf */
  if(screenX >= width || screenY >= height) return FALSE;
  int pixbufX = screenX;
  int pixbufY = screenY;
  int drawWidth = MIN(width-pixbufX,event->area.width);
  int drawHeight = MIN(height-pixbufY,event->area.height);

  gdk_draw_rectangle(widget->window,widget->style->white_gc,
		     1,event->area.x,event->area.y,event->area.width,event->area.height);
  gdk_draw_pixbuf(widget->window,
		  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		  pixbuf,
		  screenX, screenY, pixbufX, pixbufY, drawWidth, drawHeight,
		  GDK_RGB_DITHER_NORMAL,
		  0, 0);
  
  

  /*gdk_draw_rectangle(pixmap,widget->style->white_gc,
		     1,event->area.x,event->area.y,event->area.width,event->area.height);
  */

  /*
  double zoom=0.5;
  GdkPixmap *pixmap2 = gdk_pixmap_new(widget->window,width*zoom,height*zoom,-1);
  gdk_pixbuf_scale(pixmap);...*/

  /*
  gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                  pixmap,
                  event->area.x, event->area.y,
                  event->area.x, event->area.y,
                  event->area.width, event->area.height);
  gdk_pixmap_unref(pixmap);
  */

  return FALSE;
}




void on_ecologyZoomIn_clicked (GtkToolButton   *toolbutton,ToplevelWindow *tl)
{
  topologyImageZoom *= sqrt(2.0);
  /* Refuse to zoom too much since it uses a lot of memory/cpu */
  if(topologyImageZoom > 4.0) topologyImageZoom=4.0;
  /* Generate exposure event */
  GtkWidget *topologyDrawingArea = GTK_WIDGET(gtk_builder_get_object(tl->builder,"topologyDrawingArea"));  
  gtk_widget_queue_draw_area(topologyDrawingArea,0,0,5000,5000);
}


void on_ecologyZoomOut_clicked (GtkToolButton *toolbutton,ToplevelWindow *tl)
{
  topologyImageZoom /= sqrt(2.0);
  /* Generate exposure event */
  GtkWidget *topologyDrawingArea = GTK_WIDGET(gtk_builder_get_object(tl->builder,"topologyDrawingArea"));  
  gtk_widget_queue_draw_area(topologyDrawingArea,0,0,5000,5000);
}


void on_topologyCancel_clicked (GtkToolButton *toolbutton,ToplevelWindow *tl) {
  topologyAbortExecution=1;
  gPreferences.topologyUpdateInterval=0;
  GtkComboBox *update_combo=GTK_COMBO_BOX(gtk_builder_get_object(tl->builder,"topologyUpdateInterval"));
  if(update_combo) {
    gtk_combo_box_set_active(update_combo, 0);
  } 
}


