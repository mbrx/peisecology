#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <peiskernel/peiskernel.h>
#include <time.h>
#include <string.h>

#include "callbacks.h"
#include "support.h"
#include "tupleview.h"

#include <math.h>
#include <pthread.h>

int renderDotGraphInProgress=0;
int renderDotGraphFinished=0;
int abortRenderDotGraph=0;
int dotGraphImageOk=0;
int dotGraphImageIsNew=0;
double dotGraphImageZoom=1.0;
char dotGraphCommand[256];

char *tupleHistory[MAX_TUPLE_HISTORY];
int nTupleHistory;
int currentTupleHistory;

GtkTextTag *textTagWhiteForeground, *textTagBlackBackground, *textTagRedBackground;


void dotGraphUpdateView(GtkWidget *widgetTree);




void on_cut1_activate (GtkMenuItem     *menuitem,gpointer         user_data) {
}
void on_copy1_activate (GtkMenuItem     *menuitem,gpointer         user_data) {
}
void on_paste1_activate (GtkMenuItem     *menuitem,gpointer         user_data) {
}
void on_delete1_activate (GtkMenuItem     *menuitem,gpointer         user_data) {
}
void on_menuEcology_activate (GtkMenuItem     *menuitem,gpointer         user_data) {

}






/* Call when SOME currently viewed/edited tuple has changed due to eg. subscription notifications */
void valueChangedCallback(PeisTuple *tuple,void *userdata) {
  int len;
  char *data;
  char str[256],*date;
  GtkTextBuffer *buffer;
  int winno;

  if(tuple) {
    len=tuple->datalen;
    data=tuple->data;
  } else data="";

  if(!tuple) return;

  /* Iterate over all toplevel windows and update them */
  for(winno=0;winno<nWindows;winno++) {
    if(windows[winno].kind == eUnused) continue;
    GtkWidget *mainwindow = windows[winno].window;
    ToplevelWindow *tl = &windows[winno];
    if(tl->selectedPeisID != tuple->owner) continue;

    peisk_getTupleName(tuple,str, sizeof(str));
    if(strcmp(tl->selectedTuple, str) != 0) continue;


    /* Set the "owner" entry */
    snprintf(str,255,"%d",tl->selectedPeisID);
    GtkEntry *ownerEntry = GTK_ENTRY(lookup_widget(mainwindow,"ownerEntry"));
    gtk_entry_set_text(ownerEntry,str);
    
    /* Set the mimetype entry */
    GtkEntry *mimetypeEntry = GTK_ENTRY(lookup_widget(mainwindow,"tupleMimetypeEntry"));
    if(tuple) gtk_entry_set_text(mimetypeEntry,tuple->mimetype?tuple->mimetype:"*");
    else gtk_entry_set_text(mimetypeEntry,"");

    /* Set the "creator" entry */
    if(tuple) {
      snprintf(str,255,"%d",tuple->creator);
    } else {
      snprintf(str,255,"");
    }
    GtkEntry *creatorEntry = GTK_ENTRY(lookup_widget(mainwindow,"creatorEntry"));
    gtk_entry_set_text(creatorEntry,str);
    
    /* Assign the new text to the "text value" field */
    GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
    gtk_text_buffer_set_text(buffer,data,strlen(data));  

    /* Set tsSize entry */
    if(tuple) {
      if(tuple->datalen < 1024) snprintf(str,255,"%d bytes",tuple->datalen);
      else if(tuple->datalen < 1024*128) snprintf(str,255,"%.1f KiB (%d bytes)",tuple->datalen/1024.,tuple->datalen);
      else snprintf(str,255,"%.2f MiB (%d bytes)",tuple->datalen/(1024.*1024.),tuple->datalen);
    }
    else { snprintf(str,255,""); }
    GtkEntry *tsSizeEntry = GTK_ENTRY(lookup_widget(mainwindow,"tsSizeEntry"));
    if(tsSizeEntry) gtk_entry_set_text(tsSizeEntry,str);


    /* Set tsWrite entries */
    if(tuple) {
      snprintf(str,255,"%d.%06d",tuple->ts_write[0],tuple->ts_write[1]);
      time_t t=tuple->ts_write[0];
      date=ctime((time_t*)(&t));
      date[strlen(date)-1] = '\0';
    }
    else { snprintf(str,255,""); date=""; }
    GtkEntry *tsWriteEntry = GTK_ENTRY(lookup_widget(mainwindow,"tsWriteEntry"));
    if(tsWriteEntry) gtk_entry_set_text(tsWriteEntry,str);
    GtkEntry *tsWriteEntry2 = GTK_ENTRY(lookup_widget(mainwindow,"tsWriteEntry2"));
    if(tsWriteEntry2) gtk_entry_set_text(tsWriteEntry2,date);

    /* Set tsUser labels */
    if(tuple) {
      snprintf(str,255,"%d.%06d",tuple->ts_user[0],tuple->ts_user[1]);
      time_t t = tuple->ts_user[0];
      date=ctime((time_t*)(&t));
      date[strlen(date)-1] = '\0';
    }
    else { snprintf(str,255,""); date=""; }
    GtkEntry *tsUserEntry = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry"));
    if(tsUserEntry) gtk_entry_set_text(tsUserEntry,str);
    GtkEntry *tsUserEntry2 = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry2"));
    if(tsUserEntry2) gtk_entry_set_text(tsUserEntry2,date);

    /* Set tsExpire labels */
    if(tuple) {
      snprintf(str,255,"%d.%06d",tuple->ts_expire[0],tuple->ts_expire[1]);
      if(tuple->ts_expire[0] == 0) date = "never\0";
      else {
	time_t t=tuple->ts_expire[0];
	date=ctime((time_t*)(&t));
	date[strlen(date)-1] = '\0';
      }
    }
    else { snprintf(str,255,""); date=""; }
    GtkEntry *tsExpireEntry = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry"));
    gtk_entry_set_text(tsExpireEntry,str);
    GtkEntry *tsExpireEntry2 = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry2"));
    gtk_entry_set_text(tsExpireEntry2,date);
    
    /* Invalidate data image */
    tupleDrawingAreaOk=0;
    tupleDrawingAreaType=NULL;

    /*printf("valueChangedCallback: key='%s', data='%s'\n",key,data);*/
    GtkNotebook *tupleViewNoteBook=(GtkNotebook*)lookup_widget(mainwindow,"tupleViewNoteBook");
    
    if(tuple && tuple->mimetype && (strcmp(tuple->mimetype,"image/jpg") == 0 || strcmp(tuple->mimetype,"image/png") == 0)) {
      /* This seems to be a binary jpg coded image coded following the
	 same standard as the peiscam. Or it is a PNG image. */
      FILE *fp=fopen(tupleDrawingAreaName,"wb");
      if(fp) {
	fwrite(data,1,len,fp);
	fclose(fp);
	//printf("Wrote %s tuple to %s\n",tuple->mimetype,tupleDrawingAreaName);
	
	/* Validate data image */
	tupleDrawingAreaOk=1;
	if(strcmp(data,"image/jpg") == 0) tupleDrawingAreaType = "jpg";
	else if(strcmp(data,"image/png") == 0) tupleDrawingAreaType = "png";
	
	GtkWidget *tupleDrawingArea = lookup_widget(mainwindow,"tupleDrawingArea");      
	gtk_widget_queue_draw_area(tupleDrawingArea,0,0,5000,5000);
      }
      gtk_notebook_set_current_page(tupleViewNoteBook,1);
    } else if(strncmp(data,"digraph",7) == 0 || strncmp(data,"# timeline",10) == 0) {
      /* This tuple is a dot file, so render it */
      gtk_notebook_set_current_page(tupleViewNoteBook,2);    
      dotGraphUpdateView(mainwindow);
    } else 
      gtk_notebook_set_current_page(tupleViewNoteBook,0);    
  }
}

void renderGraphUpdateViewThread(void *data) {
  if(system((char*) data) == 0)
    printf("rendering done\n");
  else
    printf("error rendering graphs. You might need to install graphviz, perl and/or Easytimeline.pl on this computer\n");
  renderDotGraphFinished=1;
}

void dotGraphUpdateView(GtkWidget *widgetTree) {
  char *renderer; 
  char *outputFormat;
  char *tupleValue;
  int len;
  int useEasyTimeline=0;

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widgetTree));
  GtkWidget *mainwindow = tl->window;

  /* Get graphinfo from selected tuple */
  PeisTuple *tuple=peisk_getTuple(tl->selectedPeisID,tl->selectedTuple,PEISK_KEEP_OLD);
  if(tuple) {
    len=tuple->datalen;
    tupleValue=tuple->data;
  }
  if(!tuple) { 
    return;
  }

  /* Get the method from the radiobuttons */
  GtkToggleButton *dot_rb=GTK_TOGGLE_BUTTON(lookup_widget(widgetTree,"dotGraphDot_rb"));
  GtkToggleButton *fdp_rb=GTK_TOGGLE_BUTTON(lookup_widget(widgetTree,"dotGraphFdp_rb"));
  GtkToggleButton *circo_rb=GTK_TOGGLE_BUTTON(lookup_widget(widgetTree,"dotGraphCirco_rb"));
  GtkToggleButton *timeline_rb=GTK_TOGGLE_BUTTON(lookup_widget(widgetTree,"dotGraphTimeline_rb"));
  if(gtk_toggle_button_get_active(timeline_rb)) useEasyTimeline=1;
  else if(strncmp(tuple->data,"# timeline",10) == 0) useEasyTimeline=1;
  else if(gtk_toggle_button_get_active(fdp_rb)) renderer="fdp";
  else if(gtk_toggle_button_get_active(circo_rb)) renderer="circo";
  else renderer="dot";

  /* Get output format from the radiobuttons */
  GtkToggleButton *png_rb=GTK_TOGGLE_BUTTON(lookup_widget(widgetTree,"dotGraphPng_rb"));
  GtkToggleButton *svg_rb=GTK_TOGGLE_BUTTON(lookup_widget(widgetTree,"dotGraphSvg_rb"));
  if(gtk_toggle_button_get_active(fdp_rb)) outputFormat="svg";
  else outputFormat="png";

  /* Open a file to write the tuple to */
  FILE *fp=fopen(tupleDotFile,"wb");
  if(!fp) { printf("Failed to open %s for writing\n",tupleDotFile); return; }
  fprintf(fp,"%s\n", tupleValue);
  fclose(fp);

  if(renderDotGraphInProgress) { printf("Rendering in progress, aborting\n"); return; }
  renderDotGraphInProgress=1;
  printf("renderDotGraph = %d\n",renderDotGraphInProgress);
  printf("i'm the only one rendering!\n");

  /* Generate graph according to output format and renderer */
  if(useEasyTimeline) {
    snprintf(dotGraphCommand,sizeof(dotGraphCommand),"perl /usr/local/share/tupleview/EasyTimeline.pl -i %s -o %s",tupleDotFile,tupleDotGraphImageName);        
  }  else {
    snprintf(dotGraphCommand,sizeof(dotGraphCommand),"%s %s -T%s -Nfontname=courier -Efontname=courier > %s",
	     renderer,tupleDotFile,outputFormat,tupleDotGraphImageName);
  }
  printf("GraphCommand: %s\n",dotGraphCommand);

  pthread_t renderThread;
  pthread_create(&renderThread,NULL,(void * (*)(void *)) renderGraphUpdateViewThread,(void*) dotGraphCommand);

  renderDotGraphFinished=0;
  while(!renderDotGraphFinished) {
    peisk_wait(50000);
    gtk_main_iteration();
    if(abortRenderDotGraph) {
      pthread_cancel(renderThread);
      snprintf(dotGraphCommand,sizeof(dotGraphCommand),"skill -9 %s",renderer);
      if(system(dotGraphCommand) != 0) printf("Failed to kill dot\n");
      abortRenderDotGraph=1;
    }
  }
  renderDotGraphInProgress=0;
  printf("setting renderDotGraph to %d\n",renderDotGraphInProgress);

  dotGraphImageOk=1;
  dotGraphImageIsNew=1;
  printf("ok, now i want to draw it\n");
  /* Generate exposure event */
  GtkWidget *dotGraphDrawingArea = lookup_widget(widgetTree,"dotGraphDrawingArea");  
  gtk_widget_queue_draw_area(dotGraphDrawingArea,0,0,5000,5000);      
}

/* Call whenever a new tuple has been selected either by editing tuplekey or by clicking in tree view */
void newTupleSelected(GtkWidget *mainwindow,int updateKey) {
  //GtkWidget *currentTupleLabelG = lookup_widget(mainwindow,"currentTupleLabel");

  ToplevelWindow *tl = findToplevelWindow(mainwindow);
  GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
  GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
  /*GtkWidget *newTupleTextEntry=lookup_widget(mainwindow,"newTupleTextEntry");*/
  GtkTextBuffer *buffer;

  char *val;
  int  len;
  char str[256];


  /* Create subscription to the selected tuple */
  if(subscription) { peisk_unsubscribe(subscription); subscription=0; }
  if(subscriptionClicks) { peisk_unsubscribe(subscriptionClicks); subscriptionClicks=0; }
  if(subscriptionKeys) { peisk_unsubscribe(subscriptionKeys); subscriptionKeys=0; }

  subscription=peisk_subscribe(tl->selectedPeisID,tl->selectedTuple);

  /* Setup callback */
  if(hasCallback) peisk_unregisterTupleCallback(callback);
  else hasCallback=1;
  callback=peisk_registerTupleCallback(tl->selectedPeisID,tl->selectedTuple,NULL,valueChangedCallback);

  /* Update value and entry text fields  as well as the newTupleTextEntry */

  PeisTuple *tuple=peisk_getTuple(tl->selectedPeisID,tl->selectedTuple,PEISK_KEEP_OLD);
  if(tuple) { len=tuple->datalen; val=tuple->data; }
  if(!tuple) val="";

  valueChangedCallback(tuple,NULL); /* Update current value in a way consistent with the callbacks */

  /*
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
  gtk_text_buffer_set_text(buffer,val,strlen(val)); 
  */

  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
  gtk_text_buffer_set_text(buffer,val,strlen(val));     

  /* Update tuplekey */
  if(updateKey) {
    GtkEntry *tupleKey = GTK_ENTRY(lookup_widget(mainwindow,"tupleKeyEntry"));
    gtk_entry_set_text(tupleKey,tl->selectedTuple);
  }
  

  /* Reflect new selection in the top label */
  /*
  char str[256];
  snprintf(str,255,"%d.%s",selectedPeisID,selectedTuple);
  gtk_label_set_text(GTK_LABEL(currentTupleLabel),str);
  */

}

/** Stores last button pressed inside the treeview for when the
    selection changed callback gets triggered so we can figure out
    when to create a popup menu. This is a _bit_ of an ugly hack. */
int peistreeview_selectionButton=-1;
gboolean on_peistreeview_button_press_event     (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
  /** \todo Use the findwindow function to find/store last selected treeview button inside instead */
  peistreeview_selectionButton=-1;
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(widget,"peistreeview")));
  if(!selection) printf("error, failed to get selection object of main tree view\n");
  gtk_tree_selection_unselect_all(selection);
  peistreeview_selectionButton=event->button;
  return FALSE;
}


void on_startComponent_activate (GtkMenuItem *menuitem,gpointer user_data) {
  char *componentName = (char*) user_data;
  char str[256];
  printf("starting %s...\n",componentName);
  snprintf(str,sizeof(str),"components.%s.reqState",componentName);
  
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(menuitem));
  //GtkWidget *mainwindow = tl->window;

  peisk_setRemoteTuple(tl->selectedPeisID,str,strlen("on")+1,"on","text/plain",PEISK_ENCODING_ASCII);
}
void on_stopComponent_activate (GtkMenuItem *menuitem,gpointer user_data) {
  char *componentName = (char*) user_data;
  char str[256];
  printf("stopping %s...\n",componentName);
  snprintf(str,sizeof(str),"components.%s.reqState",componentName);  

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(menuitem));
  //GtkWidget *mainwindow = tl->window;
  peisk_setRemoteTuple(tl->selectedPeisID,str,strlen("off")+1,"off","text/plain",PEISK_ENCODING_ASCII);
}
void on_quitComponent_activate (GtkMenuItem *menuitem,gpointer user_data) {  
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(menuitem));
  //GtkWidget *mainwindow = tl->window;

  peisk_setRemoteTuple(tl->selectedPeisID,"kernel.do-quit",4,"yes","text/plain",PEISK_ENCODING_ASCII);
  peisk_setRemoteTuple(tl->selectedPeisID,"do-quit",4,"yes","text/plain",PEISK_ENCODING_ASCII);
}

char popupComponentNames[32][256];
int nPopupComponents;

void peistree_selection_changed_cb (GtkTreeSelection *selection, GtkWidget *mainwindow) {
  char str[256];
  char *peisname;
  int len;

  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name;
  gint depth,peisid;
  GtkTextBuffer *buffer;
    
  /*GtkWidget *currentTupleLabel = lookup_widget(mainwindow,"currentTupleLabel");*/
  /*GtkWidget *newTupleTextEntry=lookup_widget(mainwindow,"newTupleTextEntry");*/

  ToplevelWindow *tl = findToplevelWindow(mainwindow);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, PTREE_DEPTH, &depth, PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);

    if(depth == 0 || depth == 1) {
      /* We have selected a PEIS Host or a PEIS Component */
      tl->selectedPeisID = peisid; 
      tl->selectedTuple[0]=0;

      /* Updated the "owner" entry */
      snprintf(str,255,"%d",tl->selectedPeisID);
      GtkEntry *ownerEntry = GTK_ENTRY(lookup_widget(mainwindow,"ownerEntry"));
      gtk_entry_set_text(ownerEntry,str);

      /* Update tuplekey */
      GtkEntry *tupleKey = GTK_ENTRY(lookup_widget(mainwindow,"tupleKeyEntry"));
      gtk_entry_set_text(tupleKey,"");

      GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
      buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
      gtk_text_buffer_set_text(buffer,"",0); 

      GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
      buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
      gtk_text_buffer_set_text(buffer,"",0); 

      if(depth == 0) {
	/* Focus on something?? */
      } else if(depth == 1) {
	/* Set focus on the tuple name field to make it easy to write a
	   new tuple */
	GtkWidget *widget = lookup_widget(mainwindow,"tupleKeyEntry");
	gtk_window_set_focus(GTK_WINDOW(mainwindow),widget);
      }
    } else if(depth == 2) {
      /* We have selected a specific tuple */
      if(peisid != tl->selectedPeisID || strcasecmp(tl->selectedTuple,name) != 0) {
	tl->selectedPeisID = peisid;
	strncpy(tl->selectedTuple,name,sizeof(tl->selectedTuple));
	newTupleSelected(mainwindow,1);
      }


      /* Set focus on the tuple edit field to make it easy to change tuple */
      GtkWidget *widget = lookup_widget(mainwindow,"editValueTextView");
      gtk_window_set_focus(GTK_WINDOW(mainwindow),widget);
    }
    g_free (name);

    /* Setup a context menu if we have right clicked an suitable
       component, tuple, host etc. */
    if(peistreeview_selectionButton == 3) {
      GtkMenu *popupMenu=NULL;
      GtkAccelGroup *accel_group;

      if(depth == 1 && peisid == 0) {
	/* No context menu for HOSTS */
      } else if(depth == 1 && peisid != 0) {
	/* Context menu for COMPONENTS */
	popupMenu = (GtkMenu*) gtk_menu_new();
	accel_group = gtk_accel_group_new ();
	GtkWidget *menuItem;
	GtkWidget *menuItemImage;
	
	peisname=NULL;
	PeisTuple *nameTuple=peisk_getTuple(tl->selectedPeisID,"kernel.name",PEISK_KEEP_OLD);
	if(nameTuple) { len=nameTuple->datalen; peisname=nameTuple->data; }

	/* Parse through the components tuple and create context menus
	   for it */

	if(peisname && strcasecmp(peisname,"peisinit") == 0) {
	  char *components;
	  char *componentName;
	  char *componentNameSafe;
	  char *componentState;
	  int cnt, turnon;
	  PeisTuple *componentsTuple = peisk_getTuple(tl->selectedPeisID,"components",PEISK_KEEP_OLD);
	  if(componentsTuple && componentsTuple->data[0] == '(') {
	    len=componentsTuple->datalen;
	    components=componentsTuple->data;
	    components+=2;
	    /*while(*components && !(isalnum(*components)|| ispunct(*components))) components++;
	      components--;*/

	    nPopupComponents=0;
	    for(;;) {
	      if(*components == 0 || *components == ')') break;
	      componentName=popupComponentNames[nPopupComponents++];
	      char *c=componentName; cnt=0;
	      while(*components && (isalnum(*components) || ispunct(*components)) && *components != ')' && cnt<255) c[cnt++] = *(components++);
	      c[cnt]=0;
	      while(*components && !(isalnum(*components)|| ispunct(*components))) components++;
	      menuItem=gtk_image_menu_item_new_with_mnemonic(componentName);	
	      gtk_widget_show(menuItem);
	      gtk_container_add(GTK_CONTAINER(popupMenu),menuItem);
	      snprintf(str,sizeof(str),"components.%s.currState",componentName);
	      turnon=1;
	      PeisTuple *componentStateTuple=peisk_getTuple(tl->selectedPeisID,str,PEISK_KEEP_OLD);
	      if(componentStateTuple) {
		len=componentStateTuple->datalen;
		componentState = componentStateTuple->data;
		if(strcasecmp(componentState ,"on") == 0) {
		  menuItemImage = gtk_image_new_from_stock("gtk-yes",GTK_ICON_SIZE_MENU);
		  turnon=0;
		} else
		  menuItemImage = gtk_image_new_from_stock("gtk-no",GTK_ICON_SIZE_MENU);
	      } else 
		menuItemImage = gtk_image_new_from_stock("gtk-dialog-question",GTK_ICON_SIZE_MENU);
	      gtk_widget_show(menuItemImage);
	      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuItem),menuItemImage);
	      if(turnon) 
		g_signal_connect ((gpointer) menuItem, "activate",
				  G_CALLBACK (on_startComponent_activate),
				  (void*)componentName);
	      else
		g_signal_connect ((gpointer) menuItem, "activate",
				  G_CALLBACK (on_stopComponent_activate),
				  (void*)componentName);
	    }
	  }
	} 
	
	/* Add a standard action to kill this component */
	menuItem=gtk_image_menu_item_new_with_mnemonic("quit");	
	gtk_widget_show(menuItem);
	gtk_container_add(GTK_CONTAINER(popupMenu),menuItem);
	menuItemImage = gtk_image_new_from_stock("gtk-quit",GTK_ICON_SIZE_MENU);
	gtk_widget_show(menuItemImage);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuItem),menuItemImage);
	g_signal_connect ((gpointer) menuItem, "activate",
			  G_CALLBACK (on_quitComponent_activate),
			  (void*)NULL);

	gtk_menu_set_accel_group (GTK_MENU (popupMenu), accel_group);
	gtk_widget_show((GtkWidget*)popupMenu);
	gtk_menu_popup(popupMenu,NULL,NULL,NULL,NULL,3,gtk_get_current_event_time());
	gtk_widget_show_all((GtkWidget*)popupMenu);
      } else {
	/* No context menu for TUPLES */
      }
    }

  }
}


void on_submitButton_pressed (GtkButton *button,gpointer user_data) {
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(button));
  GtkWidget *mainwindow = tl->window;  

  if(tl->selectedPeisID != 0 && tl->selectedTuple[0] != '0') {
    GtkEntry *tsUser = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry21"));
    GtkEntry *tsExpire = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry21"));

    const char *tsUserStr = gtk_entry_get_text(tsUser);
    const char *tsExpireStr = gtk_entry_get_text(tsExpire);

    GtkEntry *mimetypeEdit = GTK_ENTRY(lookup_widget(mainwindow,"mimetypeEdit"));
    const char *mimetype = gtk_entry_get_text(mimetypeEdit);
    if(!mimetype || mimetype[0] == 0) mimetype="text/plain";

    int timenow[2];
    peisk_gettime2(&timenow[0],&timenow[1]);
    long long int user1=0, user2=0;
    long long int expire1=0, expire2=0;
    if(tsUserStr[0] == '+') {
      sscanf(tsUserStr+1,"%Ld.%Ld",&user1,&user2);
      user1 = timenow[0] + user1;      
    } else if(tsUserStr[0] == '-') {
      sscanf(tsUserStr,"%Ld.%Ld",&user1,&user2);
      user1 = timenow[0] + user1;
    }
    else 
      sscanf(tsUserStr,"%Ld.%Ld",&user1,&user2);
    peisk_tsUser(user1,user2);

    if(tsExpireStr[0] == '+') {
      sscanf(tsExpireStr+1,"%Ld.%Ld",&expire1,&expire2);
      expire1 = timenow[0] + expire1;      
    } else if(tsExpireStr[0] == '-') {
      sscanf(tsExpireStr,"%Ld.%Ld",&expire1,&expire2);
      expire1 = timenow[0] + expire1;
    }
    else 
      sscanf(tsExpireStr,"%Ld.%Ld",&expire1,&expire2);

    /* Submit button has been pressed _and_ we have a valid peisid/tuple selection */
    GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
    GtkTextBuffer *buffer; 
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer,&start);
    gtk_text_buffer_get_end_iter(buffer,&end);
    char *newValue=gtk_text_buffer_get_text(buffer,&start,&end,FALSE);
    if(!newValue) return;

    PeisTuple tuple;
    peisk_initTuple(&tuple);
    tuple.owner = tl->selectedPeisID;
    peisk_setTupleName(&tuple,tl->selectedTuple);
    tuple.datalen=strlen(newValue)+1;
    tuple.data=newValue;
    tuple.ts_expire[0]=expire1;
    tuple.ts_expire[1]=expire2;
    tuple.mimetype=strdup(mimetype);
    printf("inserting with mimetype: %s\n",mimetype);
    peisk_insertTuple(&tuple);
    free(tuple.mimetype);

    /* Add the tuplevalue to tupleHistory, rlh*/
    if(++nTupleHistory >= MAX_TUPLE_HISTORY) nTupleHistory=0;
    if(++currentTupleHistory >= MAX_TUPLE_HISTORY) currentTupleHistory=0;
    if(tupleHistory[nTupleHistory]) free(tupleHistory[nTupleHistory]);
    tupleHistory[nTupleHistory] = strdup(newValue);

    /* Make sure than any updates get visible right away */
    peisk_wait(5000); 
    repopulateAllTrees();
  }
}


void on_appendButton_pressed(GtkButton *button,gpointer user_data) {
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(button));
  GtkWidget *mainwindow = tl->window;  

  if(tl->selectedPeisID != 0 && tl->selectedTuple[0] != '0') {

    ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(button));
    GtkWidget *mainwindow = tl->window;

    GtkEntry *tsUser = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry21"));
    GtkEntry *tsExpire = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry21"));

    const char *tsUserStr = gtk_entry_get_text(tsUser);
    const char *tsExpireStr = gtk_entry_get_text(tsExpire);

    int timenow[2];
    peisk_gettime2(&timenow[0],&timenow[1]);
    long long int user1=0, user2=0;
    long long int expire1=0, expire2=0;
    if(tsUserStr[0] == '+') {
      sscanf(tsUserStr+1,"%Ld.%Ld",&user1,&user2);
      user1 = timenow[0] + user1;      
    } else if(tsUserStr[0] == '-') {
      sscanf(tsUserStr,"%Ld.%Ld",&user1,&user2);
      user1 = timenow[0] + user1;
    }
    else 
      sscanf(tsUserStr,"%Ld.%Ld",&user1,&user2);
    peisk_tsUser(user1,user2);

    if(tsExpireStr[0] == '+') {
      sscanf(tsExpireStr+1,"%Ld.%Ld",&expire1,&expire2);
      expire1 = timenow[0] + expire1;      
    } else if(tsExpireStr[0] == '-') {
      sscanf(tsExpireStr,"%Ld.%Ld",&expire1,&expire2);
      expire1 = timenow[0] + expire1;
    }
    else 
      sscanf(tsExpireStr,"%Ld.%Ld",&expire1,&expire2);

    /* Submit button has been pressed _and_ we have a valid peisid/tuple selection */
    GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
    GtkTextBuffer *buffer; 
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer,&start);
    gtk_text_buffer_get_end_iter(buffer,&end);
    char *newValue=gtk_text_buffer_get_text(buffer,&start,&end,FALSE);
    if(!newValue) return;

    PeisTuple tuple;
    peisk_initAbstractTuple(&tuple);
    tuple.owner = tl->selectedPeisID;
    peisk_setTupleName(&tuple,tl->selectedTuple);
    peisk_appendTupleByAbstract(&tuple,strlen(newValue)+1,newValue);
    
    /* Add the tuplevalue to tupleHistory, rlh*/
    if(++nTupleHistory >= MAX_TUPLE_HISTORY) nTupleHistory=0;
    if(++currentTupleHistory >= MAX_TUPLE_HISTORY) currentTupleHistory=0;
    if(tupleHistory[nTupleHistory]) free(tupleHistory[nTupleHistory]);
    tupleHistory[nTupleHistory] = strdup(newValue);

    /* Make sure than any updates get visible right away */
    peisk_wait(5000); 
    repopulateAllTrees();
  }
}



gboolean on_mainwindow_destroy_event (GtkWidget *widget,GdkEvent *event, gpointer user_data) {
  printf("on_mainwindow_destroy_event\n");
  return FALSE;
}


void on_mainwindow_destroy (GtkObject       *object,gpointer         user_data) {
  /*printf("on_mainwindow_destroy\n");*/
}


gboolean on_mainwindow_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  printf("Delete event\n");
  ToplevelWindow *tl = findToplevelWindow(widget);
  removeToplevelWindow(tl);
  return FALSE;
}


GdkPixmap* gdk_pixmap_new_from_file(const char *filename)
{
  GdkPixbuf *pixbuf;
  GdkPixmap *pixmap;

  pixbuf = gdk_pixbuf_new_from_file(filename,NULL); // _at_size
  if (!pixbuf) { printf("couldn't load %s\n",filename); return NULL; }
  gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, NULL, 1);
  gdk_pixbuf_unref(pixbuf);
  return pixmap;
}


gboolean on_tupleDrawingArea_expose_event       (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data) {

  if(!tupleDrawingAreaOk) {
    return FALSE;
  }

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widget));
  GtkWidget *mainwindow = tl->window;


  GdkPixmap *pixmap = gdk_pixmap_new_from_file(tupleDrawingAreaName);
  if(!pixmap) return FALSE;

  int width,height;
  int oldWidth,oldHeight;

  gdk_drawable_get_size(pixmap,&width,&height);
  GtkWidget *tupleDrawingArea = lookup_widget(mainwindow,"tupleDrawingArea");
  gtk_widget_get_size_request(tupleDrawingArea,&oldWidth,&oldHeight);
  if(oldWidth != width || oldHeight != height) {
    gtk_widget_set_size_request(tupleDrawingArea,width,height);    
    gtk_widget_show(tupleDrawingArea);
    //GtkWidget *tupleImageScrollWindow = lookup_widget(mainwindow,"tupleImageScrollWindow");
    
  }

  tupleDrawingAreaImageSize[0]=width;
  tupleDrawingAreaImageSize[1]=height;


  gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                  pixmap,
                  event->area.x, event->area.y,
                  event->area.x, event->area.y,
                  event->area.width, event->area.height);
  gdk_pixmap_unref(pixmap);

  return FALSE;
}


void on_tsUserEntry21_changed (GtkEditable *editable,gpointer user_data) {
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(editable));
  GtkWidget *mainwindow = tl->window;

  GtkEntry *tsUser = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry21"));
  const char *tsUserStr = gtk_entry_get_text(tsUser);
  long long int user1=0, user2=0;
  int timenow[2];
  peisk_gettime2(&timenow[0],&timenow[1]);
  if(tsUserStr[0] == '+') {
    sscanf(tsUserStr+1,"%Ld.%Ld",&user1,&user2);
    user1 = timenow[0] + user1;      
  } else if(tsUserStr[0] == '-') {
    sscanf(tsUserStr,"%Ld.%Ld",&user1,&user2);
    user1 = timenow[0] + user1;
  }
  else 
    sscanf(tsUserStr,"%Ld.%Ld",&user1,&user2);

  char *date;
  time_t t=user1;
  date=ctime((time_t*)(&t));
  date[strlen(date)-1] = '\0';

  GtkEntry *tsUser22Entry = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry22"));
  gtk_entry_set_text(tsUser22Entry,date);

}
void on_tsExpireEntry21_changed (GtkEditable *editable,gpointer user_data) {
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(editable));
  GtkWidget *mainwindow = tl->window;

  GtkEntry *tsExpire = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry21"));
  const char *tsExpireStr = gtk_entry_get_text(tsExpire);
  long long int expire1=0, expire2=0;
  int timenow[2];
  peisk_gettime2(&timenow[0],&timenow[1]);

  if(tsExpireStr[0] == '+') {
    sscanf(tsExpireStr+1,"%Ld.%Ld",&expire1,&expire2);
    expire1 = timenow[0] + expire1;      
  } else if(tsExpireStr[0] == '-') {
    sscanf(tsExpireStr,"%Ld.%Ld",&expire1,&expire2);
    expire1 = timenow[0] + expire1;
  }
  else 
    sscanf(tsExpireStr,"%Ld.%Ld",&expire1,&expire2);

  char *date;
  time_t t=expire1;
  date=ctime((time_t*)(&t));
  date[strlen(date)-1] = '\0';

  GtkEntry *tsExpire22Entry = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry22"));
  gtk_entry_set_text(tsExpire22Entry,date);

}


void on_tupleKeyEntry_changed (GtkEditable *editable,gpointer         user_data) {
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(editable));
  GtkWidget *mainwindow = tl->window;

  if(tl->selectedPeisID == 0) { /* There's no peis selected */ return; }
  GtkEntry *tupleKey = GTK_ENTRY(lookup_widget(mainwindow,"tupleKeyEntry"));
  const char *key = gtk_entry_get_text(tupleKey);

  if(!key || key[0] == 0) return;

  if(!key) {
    tl->selectedTuple[0]=0;
    newTupleSelected(mainwindow,0);
  }
  else if(strcasecmp(tl->selectedTuple,key) != 0) {
    strcpy(tl->selectedTuple,key);
    newTupleSelected(mainwindow,0);
  }
}

void saveTuple(ToplevelWindow *tl) {
  GtkWidget* fileSelect;
  const gchar *path;
  gint selection;
  GtkWidget *mainwindow = tl->window;

  char *val;
  int  len;

  PeisTuple *tuple=peisk_getTuple(tl->selectedPeisID,tl->selectedTuple,PEISK_KEEP_OLD);
  if(tuple) { len=tuple->datalen; val=tuple->data; }
  else return;

  fileSelect = gtk_file_selection_new("Save tuple...");
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileSelect),"./");   //sets the default directory
  gtk_window_set_modal(GTK_WINDOW(fileSelect),TRUE);
  selection = gtk_dialog_run(GTK_DIALOG(fileSelect));
  if(selection == GTK_RESPONSE_OK) {
    path  = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileSelect));
    FILE *fp=fopen(path,"wb");
    if(fp) {
      fwrite(val,1,len,fp);
      fclose(fp);
    }
  }
  gtk_widget_destroy(fileSelect);
}

void on_saveButton_pressed(GtkWidget *widget,gpointer userdata) {
  ToplevelWindow *tl = findToplevelWindow(widget);
  saveTuple(tl);
}

void on_save_tuple_activate (GtkMenuItem *menuitem,gpointer user_data) {
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(menuitem));
  saveTuple(tl);
}

extern GtkWidget *menu1; 
void on_quit_activate (GtkMenuItem     *menuitem,gpointer user_data) {
  printf("Quick activated!\n");
  peisk_shutdown();
}

void on_newWindow_activate (GtkMenuItem     *menuitem,gpointer user_data) {
  createMainWindow();
}


void
on_grabButton_pressed                  (GtkButton       *button,
                                        gpointer         user_data)
{
  int len;
  char *data;
  char str[256];
  char *date;

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(button));
  GtkWidget *mainwindow = tl->window;

  if(tl->selectedPeisID != 0 && tl->selectedTuple[0] != '0') {
    /* Grab button has been pressed _and_ we have a valid peisid/tuple selection */
    GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
    GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
    GtkTextBuffer *editBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
    GtkTextBuffer *currentBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(currentBuffer,&start);
    gtk_text_buffer_get_end_iter(currentBuffer,&end);
    gtk_text_buffer_set_text(editBuffer, gtk_text_buffer_get_text(currentBuffer,&start,&end,FALSE), -1);

    
    PeisTuple *tuple = peisk_getTuple(tl->selectedPeisID,tl->selectedTuple,PEISK_KEEP_OLD);
    if(!tuple) return;
    len=tuple->datalen;
    data=tuple->data;

    /* Set tsUser labels */
    if(tuple) {
      snprintf(str,255,"%d.%06d",tuple->ts_user[0],tuple->ts_user[1]);
      time_t t=tuple->ts_user[0];
      date=ctime(&t);
      date[strlen(date)-1] = '\0';
    }
    else { snprintf(str,255,""); date=""; }
    GtkEntry *tsUserEntry = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry21"));
    gtk_entry_set_text(tsUserEntry,str);
    GtkEntry *tsUserEntry2 = GTK_ENTRY(lookup_widget(mainwindow,"tsUserEntry22"));
    gtk_entry_set_text(tsUserEntry2,date);

    GtkEntry *mimetypeEntry = GTK_ENTRY(lookup_widget(mainwindow,"mimetypeEdit"));
    if(tuple)
      gtk_entry_set_text(tsUserEntry2,tuple->mimetype?tuple->mimetype:"*");
    else
      gtk_entry_set_text(tsUserEntry2,tuple->mimetype?tuple->mimetype:"");

    /* Set tsExpire labels */
    if(tuple) {
      snprintf(str,255,"%d.%06d",tuple->ts_expire[0],tuple->ts_expire[1]);
      if(tuple->ts_expire[0] == 0) date = "never\0";
      else {
	time_t t=tuple->ts_expire[0];
	date=ctime(&t);
	date[strlen(date)-1] = '\0';
      }
    }
    else { snprintf(str,255,""); date=""; }
    GtkEntry *tsExpireEntry = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry21"));
    gtk_entry_set_text(tsExpireEntry,str);
    GtkEntry *tsExpireEntry2 = GTK_ENTRY(lookup_widget(mainwindow,"tsExpireEntry22"));
    gtk_entry_set_text(tsExpireEntry2,date);
    
  }
}


void updateImageInfo(GtkWidget *widget) {
  char str[256];
  char *value;
  int len;

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widget));
  GtkWidget *mainwindow = tl->window;  

  GtkEntry *typetext = (GtkEntry*) lookup_widget(mainwindow,"imageInfoTypeText");
  GtkEntry *sizetext = (GtkEntry*) lookup_widget(mainwindow,"imageInfoSizeText");
  GtkEntry *xytext = (GtkEntry*) lookup_widget(mainwindow,"imageInfoXYText");
  GtkEntry *uvtext = (GtkEntry*) lookup_widget(mainwindow,"imageInfoUVText");
  GtkEntry *rgbtext = (GtkEntry*) lookup_widget(mainwindow,"imageInfoRGBText");
  GtkEntry *hsvtext = (GtkEntry*) lookup_widget(mainwindow,"imageInfoHSVText");

  GtkToggleButton *mousecb = (GtkToggleButton*) lookup_widget(mainwindow,"imageInfoClickableButton");
  GtkToggleButton *keyboardcb = (GtkToggleButton*) lookup_widget(mainwindow,"imageInfoKeyboardButton");

  snprintf(str,sizeof(str),"%s.gui.clickable",tl->selectedTuple);
  if(!subscriptionClicks) 
    /* Subscribe to the clicks tuple if we haven't already done so */
    subscriptionClicks=peisk_subscribe(tl->selectedPeisID,str);
  PeisTuple *tuple;
  tuple=peisk_getTuple(tl->selectedPeisID,str,PEISK_KEEP_OLD);
  if(tuple && strncasecmp(tuple->data,"yes",tuple->datalen) == 0)
    gtk_toggle_button_set_active(mousecb,TRUE);
  else
    gtk_toggle_button_set_active(mousecb,FALSE);
  snprintf(str,sizeof(str),"%s.gui.keyable",tl->selectedTuple);
  if(!subscriptionKeys) 
    /* Subscribe to the keys tuple if we haven't already done so */
    subscriptionKeys=peisk_subscribe(tl->selectedPeisID,str);
  tuple=peisk_getTuple(tl->selectedPeisID,str,PEISK_KEEP_OLD);
  if(tuple && strncasecmp(tuple->data,"yes",tuple->datalen) == 0)
    gtk_toggle_button_set_active(keyboardcb,TRUE);
  else
    gtk_toggle_button_set_active(keyboardcb,FALSE);

  if(!tupleDrawingAreaOk) {
    gtk_entry_set_text(typetext,"none");
    gtk_entry_set_text(sizetext,"");
    gtk_entry_set_text(xytext,"");
    gtk_entry_set_text(uvtext,"");
    gtk_entry_set_text(rgbtext,"");
    gtk_entry_set_text(hsvtext,"");
  } else {
    gtk_entry_set_text(typetext,tupleDrawingAreaType?tupleDrawingAreaType:"none");

    /* Optionally, if we allow svg images use the _at_size version of
       this command here */
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(tupleDrawingAreaName, NULL); 
    if(!pixbuf) return;

    int width,height;
    width = gdk_pixbuf_get_width(pixbuf);    
    height = gdk_pixbuf_get_height(pixbuf);
    tupleDrawingAreaImageSize[0]=width;
    tupleDrawingAreaImageSize[1]=height;

    snprintf(str,sizeof(str),"(%d %d)",width,height);
    gtk_entry_set_text(sizetext,str);
    snprintf(str,sizeof(str),"(%.1f %.1f)",imageInfoCursorXY[0],imageInfoCursorXY[1]);
    gtk_entry_set_text(xytext,str);
    double u=(imageInfoCursorXY[0]-width/2.0)/width;
    double v=(imageInfoCursorXY[1]-height/2.0)/height;
    snprintf(str,sizeof(str),"(%.4f %.4f)",u,v);
    gtk_entry_set_text(uvtext,str);


    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);

    int x=(int) imageInfoCursorXY[0]; 
    int y=(int) imageInfoCursorXY[1];
    if(x < 0 ||  x >= width || y < 0 || y >= height) {
      gtk_entry_set_text(rgbtext,"");
      gtk_entry_set_text(hsvtext,"");
    } else {
      int r=0, g=0, b=0, h=0, s=0, v=0;

      guchar *targetPixel = pixels + (x+y*width)*channels;
      r=targetPixel[0];
      g=targetPixel[1];
      b=targetPixel[2];
      snprintf(str,sizeof(str),"(%d %d %d)",r,g,b);
      gtk_entry_set_text(rgbtext,str);
      snprintf(str,sizeof(str),"(%d %d %d)",h,s,v);
      gtk_entry_set_text(hsvtext,str);      
    }

    gdk_pixbuf_unref(pixbuf);   
  }

}

gboolean on_tupleImageViewport_button_press_event (GtkWidget *widget,GdkEventButton  *event,gpointer user_data) {
  //printf("x: %f y: %f state: %d button: %d xroot: %f yroot: %f\n",event->x,event->y,event->state,event->button,event->x_root,event->y_root);
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widget));
  GtkWidget *mainwindow = tl->window;


  GtkScrolledWindow *swindow = (GtkScrolledWindow*) lookup_widget(mainwindow,"tupleImageScrollWindow");
  GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(swindow);
  GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(swindow);
  /* We have to remove 3 pixels from x/y since there is a small
  padding on screen. Ugly hack. We should realy figure out how to
  compute the padding exactly instead */
  imageInfoCursorXY[0] = event->x + gtk_adjustment_get_value(hadj) - 3.0;
  imageInfoCursorXY[1] = event->y + gtk_adjustment_get_value(vadj) - 3.0;
  updateImageInfo(widget); 

  char *value, str[256], str2[256];
  int len;
  snprintf(str,sizeof(str),"%s.gui.clickable",tl->selectedTuple);

  PeisTuple *tuple=peisk_getTuple(tl->selectedPeisID,str,PEISK_KEEP_OLD);
  if(tuple && strncasecmp(tuple->data,"yes",tuple->datalen) == 0) {
    snprintf(str2,sizeof(str2),"(%.1f %.1f %d)",imageInfoCursorXY[0],imageInfoCursorXY[1],event->button);
    snprintf(str,sizeof(str),"%s.gui.click",tl->selectedTuple);
    peisk_setRemoteTuple(tl->selectedPeisID,str,strlen(str2)+1,str2,"text/plain",PEISK_ENCODING_ASCII);
  }

  /* Move focus to this window */
  gtk_window_set_focus(GTK_WINDOW(mainwindow),widget);

  return FALSE;
}


void on_imageInfoXYText_editing_done (GtkCellEditable *celleditable,gpointer user_data) {  
  GtkEntry *xytext = (GtkEntry*) lookup_widget(GTK_WIDGET(celleditable),"imageInfoXYText");
  const gchar *string=gtk_entry_get_text(xytext);
  float f1=imageInfoCursorXY[0],f2=imageInfoCursorXY[1];

  sscanf((const char*)string,"(%f %f)",&f1,&f2);
  imageInfoCursorXY[0]=f1; imageInfoCursorXY[1]=f2;
  updateImageInfo(GTK_WIDGET(celleditable));   
}


void
on_imageInfoUVText_editing_done        (GtkCellEditable *celleditable,
                                        gpointer         user_data)
{
  double u,v;
  GtkEntry *uvtext = (GtkEntry*) lookup_widget(GTK_WIDGET(celleditable),"imageInfoUVText");
  const gchar *string=gtk_entry_get_text(uvtext);
  float f1,f2;
  if(sscanf((const char*)string,"(%f %f)",&f1,&f2) != 2) { //&u,&v) != 2) {
    u=0.0;
    v=0.0;
  } else {
    u=f1;
    v=f2;
  }
  imageInfoCursorXY[0] = (u+0.5) * tupleDrawingAreaImageSize[0];
  imageInfoCursorXY[1] = (v+0.5) * tupleDrawingAreaImageSize[1];
  updateImageInfo(GTK_WIDGET(celleditable));
}


void on_imageInfoClickableButton_toggled (GtkToggleButton *mousecb,gpointer user_data) {
  char str[256];
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(mousecb));
  //GtkWidget *mainwindow = tl->window;  

  snprintf(str,sizeof(str),"%s.gui.clickable",tl->selectedTuple);
  if(gtk_toggle_button_get_active(mousecb)) 
    peisk_setRemoteTuple(tl->selectedPeisID,str,4,"yes","text/plain",PEISK_ENCODING_ASCII);
  else
    peisk_setRemoteTuple(tl->selectedPeisID,str,3,"no","text/plain",PEISK_ENCODING_ASCII);
}


void on_imageInfoKeyboardButton_toggled (GtkToggleButton *keycb,gpointer user_data) {
  char str[256];
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(keycb));
  //GtkWidget *mainwindow = tl->window;  

  snprintf(str,sizeof(str),"%s.gui.keyable",tl->selectedTuple);
  if(gtk_toggle_button_get_active(keycb)) 
    peisk_setRemoteTuple(tl->selectedPeisID,str,4,"yes","text/plain",PEISK_ENCODING_ASCII);
  else
    peisk_setRemoteTuple(tl->selectedPeisID,str,3,"no","text/plain",PEISK_ENCODING_ASCII);
}


gboolean on_tupleImageViewport_key_press_event (GtkWidget *widget,GdkEventKey *event,gpointer user_data) {
  char *value, str[256], str2[256];
  int len;
  PeisTuple *tuple;

  ToplevelWindow *tl = findToplevelWindow(widget);
  //GtkWidget *mainwindow = tl->window;  

  snprintf(str,sizeof(str),"%s.gui.keyable",tl->selectedTuple);
  tuple=peisk_getTuple(tl->selectedPeisID,str,PEISK_KEEP_OLD);
  if(tuple && strncasecmp(tuple->data,"yes",tuple->datalen) == 0) {
    snprintf(str2,sizeof(str2),"%s",event->string);
    snprintf(str,sizeof(str),"%s.gui.key",tl->selectedTuple);
    peisk_setRemoteTuple(tl->selectedPeisID,str,strlen(str2)+1,str2,"text/plain",PEISK_ENCODING_ASCII);
  }
  return FALSE;
}



void on_treeviewQuit_activate (GtkMenuItem *menuitem, gpointer user_data)
{

}


void on_no1_activate (GtkMenuItem *menuitem,gpointer user_data)
{

}


void
on_item1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


gboolean
on_menu1_delete_event                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_menu1_destroy_event                 (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{

  return FALSE;
}




void on_dotGraph_rb_toggled(GtkToggleButton *togglebutton,gpointer user_data)
{
    ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(togglebutton));
    GtkWidget *mainwindow = tl->window;

    if(gtk_toggle_button_get_active(togglebutton)) {
      /* redraw dot graph */      
      dotGraphUpdateView(mainwindow);
    }
}


gboolean on_dotGraphDrawingArea_expose_event    (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{

  printf("dotGraphDrawingArea exposed %f\n",dotGraphImageZoom);

  int width,height;
  int oldWidth=0,oldHeight=0;
  GtkWidget *dotGraphDrawingArea;
  static GdkPixbuf *pixbuf;
  static double zoom=1.0;

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widget));
  GtkWidget *mainwindow = tl->window;

  if(!dotGraphImageOk) return FALSE;
  dotGraphDrawingArea = lookup_widget(mainwindow,"dotGraphDrawingArea");
  if(!dotGraphDrawingArea) { printf("Error, cannot find widget 'dotGraphDrawingArea'\n"); exit(0); }

  int windowWidth=event->area.width;
  int windowHeight=event->area.height;
  int windowSize=windowWidth<windowHeight?windowWidth:windowHeight;
  static int oldWindowSize=0;
  
  if(pixbuf == NULL || dotGraphImageIsNew || zoom != dotGraphImageZoom || oldWindowSize != windowSize) {
    printf("regenerating pixbuf at scale: %f\n",zoom);
    zoom = dotGraphImageZoom;
    dotGraphImageIsNew=0;
    if(pixbuf != NULL) gdk_pixbuf_unref(pixbuf);
    pixbuf = gdk_pixbuf_new_from_file_at_scale(tupleDotGraphImageName,
					       windowSize*dotGraphImageZoom,windowSize*dotGraphImageZoom,TRUE,NULL);    
    if (!pixbuf) return FALSE;
    oldWindowSize=windowSize;
  }

  width=gdk_pixbuf_get_width(pixbuf);
  height=gdk_pixbuf_get_height(pixbuf);

  gtk_widget_get_size_request(dotGraphDrawingArea,&oldWidth,&oldHeight);
  if(oldWidth != width || oldHeight != height) {
    gtk_widget_set_size_request(dotGraphDrawingArea,width,height);    
    gtk_widget_show(dotGraphDrawingArea);
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
  return FALSE;
}


void
on_dotGraphZoomIn_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(toolbutton));
  GtkWidget *mainwindow = tl->window;

  printf("zooming in on dotgraph\n");
  dotGraphImageZoom *= sqrt(2.0);
  if(dotGraphImageZoom > 4.0) dotGraphImageZoom=4.0;
  GtkWidget *dotGraphDrawingArea = lookup_widget(mainwindow,"dotGraphDrawingArea");  
  gtk_widget_queue_draw_area(dotGraphDrawingArea,0,0,5000,5000);
}


void on_dotGraphZoomOut_clicked             (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(toolbutton));
  GtkWidget *mainwindow = tl->window;

  dotGraphImageZoom /= sqrt(2.0);
  GtkWidget *dotGraphDrawingArea = lookup_widget(mainwindow,"dotGraphDrawingArea");  
  gtk_widget_queue_draw_area(dotGraphDrawingArea,0,0,5000,5000);
}


void initializeTupleValueHistory(GtkWidget *mainwindow){
  int i;
  GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
  GtkTextBuffer *buffer;
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));

  /* Use a tag to change the color for just one part of the widget */
  textTagWhiteForeground = gtk_text_buffer_create_tag (buffer, "white_foreground",
	   		            "foreground", "white", NULL);
  textTagBlackBackground = gtk_text_buffer_create_tag (buffer, "black_background",
	   		            "background", "black", NULL);
  textTagRedBackground = gtk_text_buffer_create_tag (buffer, "red_background",
	   		            "background", "red", NULL);
  
}

void closeTupleValueHistory(){
  int i;
  for(i = 0; i <= nTupleHistory; i++)
    free(tupleHistory[i]);
}

/* Function that display the right tuple value from history */
void updateTupleValueText(GtkWidget       *widget,
			  GdkEventKey     *event){

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widget));
  GtkWidget *mainwindow = tl->window;

  GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
  GtkTextBuffer *buffer;
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
   
  if(event->keyval == GDK_Page_Up) { 
    currentTupleHistory--;
    if(currentTupleHistory < 0) {
      /* Search for highest numbered, last used tuple history
	 field. Wrap curentTupleHistory to this entry. */
      for(currentTupleHistory=MAX_TUPLE_HISTORY-1;!tupleHistory[currentTupleHistory]&&currentTupleHistory;currentTupleHistory--) {}
    }
  }
  else if(event->keyval == GDK_Page_Down) {
    currentTupleHistory++;
    if(currentTupleHistory >= MAX_TUPLE_HISTORY || !tupleHistory[currentTupleHistory]) {
      currentTupleHistory=0;
    }
    //if(currentTupleHistory > nTupleHistory) currentTupleHistory = 0;  
  }
  gtk_text_buffer_set_text(buffer, tupleHistory[currentTupleHistory], strlen(tupleHistory[currentTupleHistory]));
  gtk_text_view_set_buffer(GTK_TEXT_VIEW(editValueTextView), buffer);
  
}

/* Find the prev symbol of this kind */
int FindPrevSymbol(GtkWidget *widget , char theSymbol, char findSymbol) {

  int i,j;  
  GdkColor color;  

  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widget));
  GtkWidget *mainwindow = tl->window;

  GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
  GtkTextBuffer *buffer;
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer,&start);
  gtk_text_buffer_get_end_iter(buffer,&end);
  char *theText=gtk_text_buffer_get_text(buffer,&start,&end,FALSE);
  if(!theText) return;

  j=0;

  for(i = strlen(theText); i >= 0; i--){

    if(theText[i] == theSymbol) j++;
    if(theText[i] == findSymbol) j--;
    if(theText[i] == findSymbol && j == 0) break;
  }

  if(i>=0){
    gtk_text_buffer_get_iter_at_offset (buffer, &start, i);
    gtk_text_buffer_get_iter_at_offset (buffer, &end, i+1);
    gtk_text_buffer_apply_tag (buffer, textTagBlackBackground, &start, &end);
    gtk_text_buffer_apply_tag (buffer, textTagWhiteForeground, &start, &end);  
  } else {
    gtk_text_buffer_get_iter_at_offset (buffer, &start, strlen(theText)-1);
    gtk_text_buffer_get_iter_at_offset (buffer, &end, strlen(theText));
    gtk_text_buffer_apply_tag (buffer, textTagRedBackground, &start, &end);
    gtk_text_buffer_apply_tag (buffer, textTagWhiteForeground, &start, &end);  
  }    
}

void removeTupleValueTag(GtkWidget *widget) {   
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(widget));
  GtkWidget *mainwindow = tl->window;

  GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
  GtkTextBuffer *buffer;
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer,&start);
  gtk_text_buffer_get_end_iter(buffer,&end);

  gtk_text_buffer_remove_all_tags(buffer, &start, &end);
}

gboolean on_editValueTextView_key_press_event   (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data) {  
  if((event->keyval == GDK_Page_Up || event->keyval == GDK_Page_Down ) && nTupleHistory >= 0) { 
    updateTupleValueText(widget,event);
  }
  else { currentTupleHistory = nTupleHistory; removeTupleValueTag(widget);}

  return FALSE;
}


gboolean
on_editValueTextView_key_release_event (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
  if(event->keyval == GDK_bracketright) FindPrevSymbol(widget, ']', '[');
  else if (event->keyval == GDK_parenright) FindPrevSymbol(widget, ')', '(');
  else if (event->keyval == 125) FindPrevSymbol(widget, '}', '{'); 
  /* use F12 to give a filename to a tuple */
  else if (event->keyval == 65481) {
    //printf("calling create_editValueFIleNameDIalogue with %x\n",widget);
    create_editValueFileNameDialogue((GtkWidget*) widget);
  }
  return FALSE;
}
void on_loadButton_pressed(GtkButton *button,gpointer user_data) {
  create_editValueFileNameDialogue((GtkWidget*) button);
}

void on_tupleviewNotebook_switch_page (GtkNotebook     *notebook,
				       GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
  ToplevelWindow *tl = findToplevelWindow(GTK_WIDGET(notebook));
  GtkWidget *mainwindow = tl->window;

  switch(page_num) {
  case 0:
    tupleviewPageActivated();
    break;
  case 1:
    ecologyviewPageActivated(mainwindow);
    break; 
  }
}

void tupleviewPageActivated() {
  /* Nothing needed to be done */
}


