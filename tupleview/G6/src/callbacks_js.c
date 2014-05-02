#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <stdio.h>
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "tupleview.h"
#include "peiskernel/peiskernel.h"
#include "peiskernel/peiskernel_mt.h"


void
on_new1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_save_as1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_quit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_cut1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_copy1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_paste1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_delete1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

int selectedPeisID=0;
char selectedTuple[256];

int hasCallback=0;
struct peiskmt_callback *callback;

int tupleDrawingAreaOk=0;  /* shows if there has been an image written to disk that we should use for the next redraw */
int isSubscribed=0;
peiskmt_subscriptionHandle subscription;

typedef struct JpgMessageHeader {
  char prefix[10];
} JpgMessageHeader;


void valueChangedCallback(char *key,int owner,int len,void *data,void *userdata) {
  GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
  GtkTextBuffer *buffer;
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
  gtk_text_buffer_set_text(buffer,data,strlen(data));  

  tupleDrawingAreaOk=0;

  /*printf("valueChangedCallback: key='%s', data='%s'\n",key,data);*/


  if(strcmp(data,"image/jpg") == 0 && len > strlen(data)+1) {
    /* This seems to be a binary jpg coded image coded following the
       same standard as the peiscam. */
    FILE *fp=fopen("tupleDrawingArea.jpg","wb");
    if(fp) {
      JpgMessageHeader *header=(JpgMessageHeader *) data;
      fwrite(data+sizeof(JpgMessageHeader),1,len-sizeof(JpgMessageHeader),fp);
      fclose(fp);
      tupleDrawingAreaOk=1;

      GtkWidget *tupleDrawingArea = lookup_widget(mainwindow,"tupleDrawingArea");
      
      GdkRectangle rect;
      rect.x=0;
      rect.y=0;
      rect.width=1024;
      rect.height=768;
      gtk_widget_draw(tupleDrawingArea,&rect);
    }
  }
}

void newTupleSelected() {
  GtkWidget *currentTupleLabel = lookup_widget(mainwindow,"currentTupleLabel");
  GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
  GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
  GtkWidget *newTupleTextEntry=lookup_widget(mainwindow,"newTupleTextEntry");
  GtkTextBuffer *buffer;

  char *val;
  int  len;


  /* Create subscription to the selected tuple */
  if(isSubscribed) peiskmt_unsubscribe(subscription);
  subscription=peiskmt_subscribe(selectedTuple,selectedPeisID);
  isSubscribed=1;

  /* Setup callback */
  if(hasCallback) peiskmt_unregisterTupleCallback((peiskmt_callbackHandle)callback);
  else hasCallback=1;
  callback= peiskmt_registerTupleCallback(selectedTuple,selectedPeisID,NULL,valueChangedCallback);

  /* Update value and entry text fields  as well as the newTupleTextEntry */
  if(!peiskmt_getTuple(selectedTuple,selectedPeisID,&len,(void**)&val,PEISK_TFLAG_OLDVAL)) val="";

  valueChangedCallback("",-1,len,val,NULL); /* Update current value in a consistent way with the callbacks */
  /*
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
  gtk_text_buffer_set_text(buffer,val,strlen(val)); 
  */

  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
  gtk_text_buffer_set_text(buffer,val,strlen(val));     
  gtk_entry_set_text(GTK_ENTRY(newTupleTextEntry),selectedTuple);

  /* Reflect new selection in the top label */
  char str[256];
  snprintf(str,255,"%d.%s",selectedPeisID,selectedTuple);
  gtk_label_set_text(GTK_LABEL(currentTupleLabel),str);

}

void peistree_selection_changed_cb (GtkTreeSelection *selection, gpointer data) {
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name;
  gint depth,peisid;
  GtkTextBuffer *buffer;
    
  GtkWidget *currentTupleLabel = lookup_widget(mainwindow,"currentTupleLabel");
  GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
  GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
  GtkWidget *newTupleTextEntry=lookup_widget(mainwindow,"newTupleTextEntry");

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, PTREE_DEPTH, &depth, PTREE_PEISID, &peisid, PTREE_NAME, &name, -1);

    if(depth == 0) {
      /* We have selected a Peis */
      selectedTuple[0]=0;
      char str[256];
      snprintf(str,255,"%d",peisid);
      gtk_label_set_text(GTK_LABEL(currentTupleLabel),str);
      selectedPeisID = peisid; 

      buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
      gtk_text_buffer_set_text(buffer,"",0); 

      buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
      gtk_text_buffer_set_text(buffer,"",0); 

   }
    else if(depth == 1) {
      /* We have selected a specific tuple */
      selectedPeisID = peisid;
      strncpy(selectedTuple,name,sizeof(selectedTuple));
      newTupleSelected();
    }
    g_free (name);
  }
}

void on_newTupleButton_pressed (GtkButton       *button,gpointer user_data) {
  if(selectedPeisID == 0) { /* There's no peis selected */ return; }
  GtkWidget *newTupleTextEntry=lookup_widget(mainwindow,"newTupleTextEntry");
  const char *key=gtk_entry_get_text(GTK_ENTRY(newTupleTextEntry));
  if(!key || key[0]==0) { printf("bad key entered\n"); return; }
  strcpy(selectedTuple,key);
  char str[256];
  snprintf(str,255,"%d.%s",selectedPeisID,selectedTuple);
  newTupleSelected();

  /*
  GtkWidget *currentTupleLabel = lookup_widget(mainwindow,"currentTupleLabel");
  gtk_label_set_text(GTK_LABEL(currentTupleLabel),str);

  GtkWidget *currentValueTextView = lookup_widget(mainwindow,"currentValueTextView");
  GtkTextBuffer *buffer; 
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(currentValueTextView));
  */
}


void on_submittButton_pressed (GtkButton       *button,gpointer         user_data) {
  if(selectedPeisID != 0 && selectedTuple[0] != '0') {
    /* Submitt button has been pressed _and_ we have a valid peisid/tuple selection */
    GtkWidget *editValueTextView = lookup_widget(mainwindow,"editValueTextView");
    GtkTextBuffer *buffer; 
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer,&start);
    gtk_text_buffer_get_end_iter(buffer,&end);
    char *newValue=gtk_text_buffer_get_text(buffer,&start,&end,FALSE);
    if(!newValue) return;
    //printf("submitting %d chars: '%s'\n",strlen(newValue),newValue);
    peiskmt_setRemoteTuple(selectedTuple,selectedPeisID,strlen(newValue)+1,newValue);

    /* Make sure than any updates get visible right away */
    //peiskmt_wait(5000); 
    usleep(5000);
    repopulateTree();
  }
}


gboolean on_mainwindow_destroy_event (GtkWidget *widget,GdkEvent *event, gpointer user_data) {
  printf("on_mainwindow_destroy_event\n");
  return FALSE;
}


void on_mainwindow_destroy (GtkObject       *object,gpointer         user_data) {
  /*printf("on_mainwindow_destroy\n");*/
}


gboolean on_mainwindow_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  /*printf("on_mainwindow_delete_event\n");*/
  peiskmt_shutdown();
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
    /*
    gdk_gc_set_foreground(gc,&colors[0]);
      gdk_draw_rectangle(mapArea_pixmap,gc,1,x0-1,y0-1,3,3);


      GdkRectangle rect;
      rect.x=event->area.x;
      rect.y=event->area.y;
      rect.width=event->area.width;
      rect.height=event->area.width;
    */
    return FALSE;
  }

  GdkPixmap *pixmap = gdk_pixmap_new_from_file("tupleDrawingArea.jpg");
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

  gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                  pixmap,
                  event->area.x, event->area.y,
                  event->area.x, event->area.y,
                  event->area.width, event->area.height);
  gdk_pixmap_unref(pixmap);

  return FALSE;
}

