#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <stdio.h>
#include <stdlib.h>
#include "support.h"
#include "callbacks.h"
#include <sys/socket.h>
#include "peiskernel/peiskernel.h"
#include "peiskernel/peiskernel_private.h"
#include "peiskernel/hashtable.h"
#include "peiskernel/tuples.h"
#include <ctype.h>
#include <string.h>

#include "tupleview.h"
#include "tokenizer.h"

/* Defines */
#define MAX_TUPLES_PER_PEIS        512

/* Prototypes */
int deleteRemovedTuples(GtkTreeStore *store,GtkTreeIter parent);
int findTuple(GtkTreeStore *store,char *tuple,GtkTreeIter parent,GtkTreeIter *result);

/* Global variables */
Preferences gPreferences;

/* List of all top-level windows */
ToplevelWindow windows[MAX_WINDOWS];
int nWindows=0;

/* List of all stores for tuple treeviews (used within the mainwindow(s) and all meta tuple views) */
int nTupleTreeStores=0;
GtkTreeStore *tupleTreeStores[MAX_TUPLE_TREE_STORES];

/* Line number during tokenization */
int lineno;

/* Used when parsing the all-keys field of peis. We make it a global variable since it might
   otherwise overflow the stack. */
char tupleNames[MAX_TUPLES_PER_PEIS][256];
int nTuples;

char gladeXMLfilename[512];

int hasCallback=0;
PeisCallbackHandle callback;

/* shows if there has been an image writen to disk that we should use
   for the next redraw */
int tupleDrawingAreaOk=0;
char *tupleDrawingAreaType;
int tupleDrawingAreaImageSize[2]={0,0};
/* The filename used for the tupleDrawingArea */
char tupleDrawingAreaName[256];
/* Shows it there is a valid topology image on disk that can be used
   to visualize the topology */   
int topologyImageOk;
int topologyImageIsNew;
double topologyImageZoom;
/* The filename used for the topologyImages */
char topologyImageName[256];
char topologyDotFile[256];

/* The filename used for the tupleGraphImages */
char tupleDotGraphImageName[256];
char tupleDotFile[256];

PeisSubscriberHandle subscription,subscriptionClicks,subscriptionKeys;

double imageInfoCursorXY[2]={0.0,0.0};

GtkWidget *menu1;
PangoFontDescription *monofont;

void print_usage(FILE *stream, int argc, char **argv) {
  fprintf(stream, "Usage: %s\n", argv[0]);
  fprintf(stream, "\n");
  peisk_printUsage(stream,argc,argv);
  exit (0);
}

void deadhost(int id,void *user) { repopulateAllTrees(); }

int main (int argc, char **argv)
{
  int i;
  int fd0,fd1,fd2,fd3,fd4,fd5;
  // Set this to 1 if you want to debug the generated .dot files
  int keepTopDot=0;
  PeisSubscriberHandle allkeysHandle;
  PeisSubscriberHandle subscriptionsHandle;
  char *date;
  int timenow[2];

  if(argc>1 && strcmp(argv[1],"--help") == 0) 
    print_usage(stderr,argc,argv);

  peisk_initialize(&argc,argv);
  loadPreferences();

  /* Creates temporary filenames and opens them in exclusive mode */
  snprintf(tupleDrawingAreaName,sizeof(tupleDrawingAreaName),"/tmp/tupleview.1.XXXXXX");
  fd0=mkstemp(tupleDrawingAreaName);
  snprintf(topologyImageName,sizeof(topologyImageName),"/tmp/tupleview.2.XXXXXX");
  fd1=mkstemp(topologyImageName);
  if(keepTopDot)
    snprintf(topologyDotFile,sizeof(topologyDotFile),"topology.dot");
  else {
    snprintf(topologyDotFile,sizeof(topologyDotFile),"/tmp/tupleview.3.XXXXXX");
    fd2=mkstemp(topologyDotFile);
  }
  /* For tuple graphs */
  snprintf(tupleDotGraphImageName,sizeof(tupleDotGraphImageName),"/tmp/tupleview.5.XXXXXX");
  fd4=mkstemp(tupleDotGraphImageName);
  if(keepTopDot)
    snprintf(tupleDotFile,sizeof(tupleDotFile),"tuplegraph.dot");
  else {
    snprintf(tupleDotFile,sizeof(tupleDotFile),"/tmp/tupleview.6.XXXXXX");
    fd5=mkstemp(tupleDotFile);
  }

  topologyImageOk=0;
  topologyImageIsNew=0;
  topologyImageZoom=1.0;

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gtk_set_locale ();
  gtk_init (&argc, &argv);

  add_pixmap_directory (PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
  monofont = pango_font_description_from_string("mono");
  createMainWindow();
  peisk_registerDeadhostHook(deadhost,NULL);

  /* Clear the history of old tuple values (TODO - load from file) */
  for(i = 0; i < MAX_TUPLE_HISTORY; i++)
    tupleHistory[i] = NULL;
  nTupleHistory = -1;
  currentTupleHistory = -1;


  int cnt=0;  
  allkeysHandle = peisk_subscribe(-1,"kernel.all-keys");
  subscriptionsHandle = peisk_subscribe(-1,"kernel.subscribers");

  /* Make sure we always have the PEIS-init "components" tuple ready
     so we can create context menus for it. */
  peisk_subscribe(-1,"components");
  peisk_subscribe(-1,"kernel.name");
  peisk_subscribe(-1,"kernel.hostname");
  peisk_subscribe(-1,"components.*.currState");

 
  while(peisk_isRunning()) {
    peisk_wait(10000);       /* Allow program to sleep 10ms between loops */

    /* Handle all GUI events */
    while (gtk_events_pending ())
      gtk_main_iteration ();
    
    if((gPreferences.topologyUpdateInterval > 0) && (cnt % gPreferences.topologyUpdateInterval == 0)){
      for(i=0;i<nWindows;i++)
	if(windows[i].kind != eUnused)
	  topologyUpdateView(windows[i].window);
    }
      
    if(cnt % 100 == 0) repopulateAllTrees();       
    if(cnt % 25 == 0) {
      /* For each window, if it has a clock label then update it */
      for(i=0;i<nWindows;i++)
	if(windows[i].kind != eUnused) {
	  GtkLabel *clockLabel = GTK_LABEL(lookup_widget(windows[i].window,"clockLabel"));
	  if(clockLabel) {
	    peisk_gettime2(&timenow[0],&timenow[1]);
	    time_t t=timenow[0];
	    date=ctime((time_t*)&t);
	    date[strlen(date)-1] = '\0';
	    gtk_label_set_text(clockLabel,date);
	  }
	}
    }
    cnt++;
  }

  /* Free tuple value history */
  closeTupleValueHistory();

  /* Unlink the temporary files, thus they will be deleted when we
     close the files or the program terminates */
  unlink(tupleDrawingAreaName);
  unlink(topologyImageName);
  if(!keepTopDot) unlink(topologyDotFile);
  unlink(tupleDotGraphImageName);
  if(!keepTopDot) unlink(tupleDotFile);
  

  /* Close the temporary files */
  close(fd0);
  close(fd1);
  if(!keepTopDot) close(fd2);
  close(fd4);
  if(!keepTopDot) close(fd5);

  peisk_shutdown();
  return 0;
}

/** Creates a mainwindow and populates it with widgets and tuple trees. */
void createMainWindow() {
  GtkWidget *mainwindow;
  struct GladeXML *mainwindowXML;
  GtkTreeStore *mainwindowTupleStore;

  snprintf(gladeXMLfilename,sizeof(gladeXMLfilename),"%s/%s/tupleview.glade",PACKAGE_DATA_DIR,PACKAGE);
  mainwindowXML = (struct GladeXML *) glade_xml_new(gladeXMLfilename, "mainwindow", NULL);
  if(!mainwindowXML) { printf("Error opening/reading %s\n",gladeXMLfilename); exit(0); }
  mainwindow = glade_xml_get_widget((GladeXML*)mainwindowXML, "mainwindow");  
  glade_xml_signal_autoconnect((GladeXML*)mainwindowXML);
  if(!mainwindow) { printf("Error - could not create main window\n"); exit(0); }

  /* Make the current/edit value textview buffers use fixed width font */
  gtk_widget_modify_font(lookup_widget(mainwindow,"editValueTextView"),monofont);
  gtk_widget_modify_font(lookup_widget(mainwindow,"currentValueTextView"),monofont);

  ToplevelWindow *tl = insertToplevelWindow(eMainWindow, mainwindow, mainwindowXML);
  GtkTreeStore *tree = createTupleTreeStore(mainwindow,"peistreeview");
  tl->peisTree = tree;
  repopulateTree(tree); 

  tl->selectedPeisID=0;
  tl->selectedTuple[0]=0;

  initializeTupleValueHistory(mainwindow);
  gtk_widget_show(mainwindow);
}

void repopulateAllEcologyviews() {
  int i;
  for(i=0;i<nWindows;i++)
    if(windows[i].kind != eUnused)
      repopulateEcologyview(windows[i].window); 
}
void repopulateAllTrees() {
  int i;
  for(i=0;i<nTupleTreeStores;i++)
    if(tupleTreeStores[i])
      repopulateTree(tupleTreeStores[i]); 
}
void repopulateTree(GtkTreeStore *store) {
  GtkTreeIter hostIter, hostIter2;
  GtkTreeIter compIter, compIter2;
  GtkTreeIter iter2, iter3;
  GtkTreeIter newTuple;
  gboolean success,success2;
  gchar *name;
  gint depth,peisid;

  int componentsChanged, hostsChanged, tuplesChanged;
  int i,j,k;
  int cnt;
  int len;
  char *allNames;
  int id;
  PeisHostInfo *hostInfo;
  PeisHashTableIterator iterator;
  cnt=peisk_hashTable_count(peiskernel.hostInfoHT);
  name = NULL;

  /* Keep track if we have changed hosts and/or components */
  componentsChanged = 0;
  hostsChanged = 0;
  tuplesChanged = 0;

  /* Remove all components and hosts that no longer belong to
     knownHosts */
  success=gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),&hostIter);
  while(success) {
    gtk_tree_model_get (GTK_TREE_MODEL(store), &hostIter, PTREE_DEPTH, &depth, 
			PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);    
    /* We do not use the name returned here, only the peisid integer */
    g_free(name); 
    
    cnt=0; /* Count how many children it still has */
    /* If it has children, then check if each child still belongs to
       known hosts */
    success = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&hostIter) &&
      gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&compIter,&hostIter);
    while(success) {
      gtk_tree_model_get (GTK_TREE_MODEL(store), &compIter, PTREE_DEPTH, &depth, 
			  PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);

      hostInfo = peisk_lookupHostInfo(peisid);
      if(!hostInfo) {
	/* Child does not exist in list of knownHosts, remove it and
	   restart the stepping for this PEIS */
	/*printf("Removing component %d : %s\n",peisid,name);*/
	componentsChanged=1;
	gtk_tree_store_remove(GTK_TREE_STORE(store), &compIter);
	success = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&hostIter) &&
	  gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&compIter,&hostIter);
      } else {
	/* Valid child, update counter of how many children we have
	   and keep stepping */
	cnt++;
	success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&compIter);	
      }

      g_free(name);       
    }
    if(cnt == 0) {
      /* This host has 0 components, remove it and restart the check
	 from the beginning. */
      hostsChanged=1;
      gtk_tree_store_remove(GTK_TREE_STORE(store), &hostIter);      
      success=gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),&hostIter);
    } else
      /* Children valid, keep stepping */
      success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&hostIter);
  }

  /* Add nodes for all known peis components that are not already part of the tree */
  peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator);
  for(;peisk_hashTableIterator_next(&iterator);) {
    peisk_hashTableIterator_value(&iterator,(void*)&id,(void**)&hostInfo);

      /* First, find a node with the right host name */
      success=gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),&hostIter);
      while(success) {
	gtk_tree_model_get (GTK_TREE_MODEL(store), &hostIter, PTREE_DEPTH, &depth, 
			    PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);
	if(name && strcmp(name,hostInfo->hostname) == 0) {
	  g_free(name);
	  break;
	} else if(name) g_free(name);
	success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&hostIter);
      }
      if(!success) {
	/* Add this host to the list of hosts */
	/* Iterate over the existing hosts to find a suitable place to
	   place it */
	hostsChanged=1;
	success=gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),&hostIter2);
	while(success) {
	  gtk_tree_model_get (GTK_TREE_MODEL(store), &hostIter2, PTREE_DEPTH, &depth, 
			      PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);
	  if(name && strcmp(name,hostInfo->hostname) > 0) {
	    g_free(name);
	    break;
	  } else if(name) g_free(name);
	  success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&hostIter2);
	}
	if(success)
	  gtk_tree_store_insert_before(store,&hostIter,NULL,&hostIter2);
	else
	  gtk_tree_store_append(store,&hostIter,NULL);
	gtk_tree_store_set (store, &hostIter, PTREE_DEPTH, 1, PTREE_PEISID, 0, PTREE_NAME, 
			    hostInfo->hostname, PTREE_COLOR, "black", -1);
      }

      /* Now, look for this specific component under that node */
      success = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&hostIter) &&
	gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&compIter,&hostIter);
      while(success) {
	gtk_tree_model_get (GTK_TREE_MODEL(store), &compIter, PTREE_DEPTH, &depth, 
			    PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);
	/* We do not need the name here */
	g_free(name);
	if(peisid == hostInfo->id) break;
	success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&compIter);
      }
      if(!success) {
	/* This component did not already exist, let's create it */
	char str[512];
	char *fullname=hostInfo->fullname;
	componentsChanged=1;
	if(gPreferences.detailedComponentInfo) {
	  snprintf(str,512,"%s (%d)",fullname,hostInfo->id);
	} else {
	  for(k=0;k<256&&fullname[k]!='@'&&fullname[k];k++) str[k]=fullname[k];
	  snprintf(str+k,512-k," (%d)",hostInfo->id);
	}

	success = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&hostIter) &&
	  gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&compIter2,&hostIter);
	while(success) {
	  gtk_tree_model_get (GTK_TREE_MODEL(store), &compIter2, PTREE_DEPTH, &depth, 
			      PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);	  
	  if(name && strcmp(name,str) > 0) {
	    g_free(name);
	    break;
	  } else if(name) g_free(name);
	  success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&compIter2);
	}
	if(success)
	  gtk_tree_store_insert_before(store,&compIter,NULL,&compIter2);
	else
	  gtk_tree_store_append(store,&compIter,&hostIter);
	gtk_tree_store_set (store, &compIter, PTREE_DEPTH, 1, PTREE_PEISID, hostInfo->id, 
			    PTREE_NAME, str, PTREE_COLOR, "black", -1);
      }      
    }


  /* Iterate over all HOST's  */
  success=gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),&hostIter);
  while(success) {
    if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&hostIter)) { 
      /* Iterate over all COMPONENTS and add their tuples at the
	 appropriate place in the tree */
      success = gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&compIter,&hostIter);
      while(success) {
      
	gtk_tree_model_get (GTK_TREE_MODEL(store), &compIter, PTREE_DEPTH, &depth, PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);
	g_free(name);
	PeisTuple *allKeysTuple = peisk_getTuple(peisid,"kernel.all-keys",PEISK_KEEP_OLD);
	if(allKeysTuple) {
	  len=allKeysTuple->datalen;
	  allNames=allKeysTuple->data;

	  /* We have a list of all tuples for this peis, now parse it
	     into the tupleNames/nTuples global variables. Also create a list
	     of tupleTree's, that is special tree nodes which can fold
	     out into the tuples. */
	  nTuples=0;
	  if(allNames[0]!='(') continue; /* bad format */
	  allNames++;
	  //printf("Parsing tuples\n");
	  for(;;) {
	    if(*allNames == 0 || *allNames == ')') break;
	    /* possible errors: too long tuples (>255 bytes) will be chopped up and treated as multiple other tuples */
	    char *c=&tupleNames[nTuples++][0]; cnt=0;
	    while(*allNames && (isalnum(*allNames) || ispunct(*allNames)) && *allNames != ')' && cnt<255) c[cnt++] = *(allNames++);
	    c[cnt]=0;
	    while(*allNames && !(isalnum(*allNames)|| ispunct(*allNames))) allNames++;
	    
	    /* Go over this tuple and see if any new trees need to be created */
	    int cnt2=cnt;
	    while(1) {
	      while(cnt2>0&&c[cnt2]!='.') cnt2--;
	      if(cnt2<=0) break;
	      /* c[0..cnt2] may be the name of another tuple needes as a subtree */
	      char str2[256];
	      strncpy(str2,c,cnt2);
	      str2[cnt2]=0; /* This also removes the final '.' */
	      for(j=0;j<nTuples;j++) if(strcmp(tupleNames[j],str2) == 0) break;
	      if(j == nTuples) {
		/* Add a new tuple for this subtree */
		strcpy(tupleNames[nTuples++],str2);
	      }
	      cnt2--; /* Remove one period so we can continue backwards to next subtree */
	    }	
	  }
      
	  /* Now, iterate over all existing children of this PEIS tree node and remove any unneccessary child tuples */
	  if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&compIter))
	    if(deleteRemovedTuples(store,compIter)) tuplesChanged=1;
      
	  /* Iterate over all known tuples and add them to suitable places in the tree hierachy */
	  int runagain;
	  do {
	    runagain=0;
	    for(i=0;i<nTuples;i++) {
	      char *thisTuple=tupleNames[i];
	      char parentTuple[256];
	      int cnt=strlen(thisTuple);
	      while(cnt>0&&thisTuple[cnt]!='.') cnt--;
	      strncpy(parentTuple,thisTuple,cnt);
	      parentTuple[cnt]=0;
	      if(findTuple(store,thisTuple,compIter,&iter2)) continue; /* Tuple already exists */
	      else if(cnt == 0) {
		/* Special case, we add to the "root" of this peis */
		/* Use iter3 to iterate over existing root tuples and
		   stop when we find the right place (alphabetically) to
		   insert this tuple */
		tuplesChanged=1;
		success = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&compIter) &&
		  gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&iter3,&compIter);
		while(success) {
		  gtk_tree_model_get (GTK_TREE_MODEL(store), &iter3, PTREE_DEPTH, &depth, 
				      PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);
		  if(name && strcmp(name,&tupleNames[i][0]) > 0) {
		    g_free(name);
		    break;
		  } else if(name) g_free(name);
		  success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&iter3);
		}
		if(success) 
		  gtk_tree_store_insert_before(store,&newTuple,NULL,&iter3);
		else
		  gtk_tree_store_append(store,&newTuple,&compIter);
		gtk_tree_store_set (store, &newTuple, PTREE_DEPTH, 2, PTREE_PEISID, peisid, PTREE_NAME, &tupleNames[i][0], PTREE_COLOR, "black",  -1);
	      } else if(findTuple(store,parentTuple,compIter,&iter2)) {
		/* Use iter3 to iterate over existing child tuples and
		   stop when we find the right place (alphabetically) to
		   insert this tuple */
		success = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&iter2) &&
		  gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&iter3,&iter2);
		while(success) {
		  gtk_tree_model_get (GTK_TREE_MODEL(store), &iter3, PTREE_DEPTH, &depth, 
				      PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);
		  if(name && strcmp(name,&tupleNames[i][0]) > 0) {
		    g_free(name);
		    break;
		  } else if(name) g_free(name);
		  success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&iter3);
		}
		if(success) 
		  gtk_tree_store_insert_before(store,&newTuple,NULL,&iter3);
		else
		  gtk_tree_store_append(store,&newTuple,&iter2);
		/*gtk_tree_store_append(store,&newTuple,&iter2);*/
		gtk_tree_store_set (store, &newTuple, PTREE_DEPTH, 2, PTREE_PEISID, peisid, PTREE_NAME, &tupleNames[i][0], PTREE_COLOR, "black",  -1);
	      } else runagain=1;/* If tuple is not found, we should run once again */
	    }      
	  } while(runagain);
	  
	}
	success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&compIter);
      }
    }
    success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&hostIter);
  }
  
  /** \todo We are calling update all ecology views too often */
  if(componentsChanged) repopulateAllEcologyviews();
}

/** This iteratates over a tree and deletes all unused tuples and tuple trees, 
    returns number of deleted tuples. */
int deleteRemovedTuples(GtkTreeStore *store,GtkTreeIter parent) {
  int success;
  GtkTreeIter child;
  gchar *name;
  int i;
  int nDeleted=0;

  success=gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&child,&parent);
  while(success) {
    gtk_tree_model_get (GTK_TREE_MODEL(store), &child, PTREE_NAME, &name, -1);

    /* Recurse if needed before processing this node */
    if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&child))
      deleteRemovedTuples(store,child);

    for(i=0;i<nTuples;i++)
      if(strcmp(&tupleNames[i][0],name) == 0) break;
    /* Name not needed after this */
    g_free(name);
    if(i == nTuples) {
      /* Node is not one of the known tuples, remove it */
      gtk_tree_store_remove(GTK_TREE_STORE(store), &child);
      nDeleted++;
      /* ugly hack (inefficient!), start from scratch after deleting a node */
      success=gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&child,&parent);
    } else {
      success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&child);
    }
  }
  return nDeleted;
}

int findTuple(GtkTreeStore *store, char *tuple,GtkTreeIter parent,GtkTreeIter *result) {
  int success;
  GtkTreeIter child;
  gchar *name;
  int i;

  success=gtk_tree_model_iter_children(GTK_TREE_MODEL(store),&child,&parent);
  while(success) {

    gtk_tree_model_get (GTK_TREE_MODEL(store), &child, PTREE_NAME, &name, -1);
    if(name && strcmp(name,tuple) == 0) { *result=child; g_free(name); return TRUE; }
    if(name) g_free(name);

    /* Recurse if needed */
    if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store),&child))
      if(findTuple(store,tuple,child,result)) return TRUE;

    /* Continue with next child */
    success=gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&child);
  }
  return FALSE;
}

ToplevelWindow *insertToplevelWindow(EToplevelWindowKind kind, GtkWidget *widget, struct GladeXML *xml) {
  ToplevelWindow *window;
  int i;

  /* Find an empty toplevel window structure to used, or grab a new one */
  for(i=0;i<nWindows;i++)
    if(windows[i].kind == eUnused) break;
  if(i == MAX_WINDOWS) { fprintf(stderr,"Too many windows open\n"); return NULL; }
  if(i==nWindows) nWindows++;
  /* Fill in the given values */
  window=&windows[i];
  window->kind=kind;
  window->window=widget;
  window->xml=xml;

  return window;
}
void removeToplevelWindow(ToplevelWindow *window) { 
  int i,highestUsed;

  freeTupleTreeStore(window->peisTree);

  window->kind = eUnused; 
  for(i=0,highestUsed=-1;i<nWindows;i++) {
    if(windows[i].kind != eUnused) highestUsed=i;
  }
  nWindows=highestUsed+1;
  printf("nWindows: %d\n",nWindows);
  if(nWindows <= 0) peisk_shutdown();
}
ToplevelWindow *findToplevelWindow(GtkWidget *anyWidget) {
  int i;
  GtkWidget *asMainWindow = lookup_widget(anyWidget, "mainwindow");
  for(i=0;i<nWindows;i++) {
    if(windows[i].window == asMainWindow) return &windows[i];
  }
  return NULL;
}

GtkTreeStore *createTupleTreeStore(GtkWidget *window,const char *widgetName) {
  int i;
  GtkTreeStore *store;

  for(i=0;i<nTupleTreeStores;i++) 
    if(tupleTreeStores[i] == NULL) break;
  if(i == MAX_TUPLE_TREE_STORES) { fprintf(stderr,"Too many tuple tree stores (too many windows open?)\n"); return NULL; }
  if(i == nTupleTreeStores) nTupleTreeStores++;

  GtkWidget *treeview;
  store=gtk_tree_store_new(PTREE_NCOLUMNS,G_TYPE_INT,G_TYPE_INT,G_TYPE_STRING,G_TYPE_STRING);
  tupleTreeStores[i]=store;

  /* Insert the treestore into the treeview */
  treeview=lookup_widget(window,widgetName);
  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview),GTK_TREE_MODEL(store));

   /* The view now holds a reference.  We can get rid of our own reference */
  g_object_unref (G_OBJECT (store));

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  renderer = gtk_cell_renderer_text_new();  
  column = gtk_tree_view_column_new_with_attributes ("Name",
						     renderer,
						     "text", PTREE_NAME,
						     "foreground", PTREE_COLOR,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* Setup the selection handler */
  GtkTreeSelection *select;  
  select = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (select), "changed",
		    G_CALLBACK (peistree_selection_changed_cb),
		    window);    

  return store;
}

void freeTupleTreeStore(GtkTreeStore *treeStore) {
  int i;
  for(i=0;i<nTupleTreeStores;i++)
    if(tupleTreeStores[i] == treeStore) 
      tupleTreeStores[i]=NULL;
}
