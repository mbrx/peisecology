/** \file editValueFileNameDialogue
    Dialogue for inputing a filename to the edit value field of a window.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <peiskernel/peiskernel.h>
#include <time.h>
#include <string.h>

#include "tupleview.h"
#include "callbacks.h"
#include "support.h"

#include <math.h>
#include <pthread.h>

typedef struct EditValueFileNameCallbackData {
  GtkBuilder *builder;
  GtkWidget *editValueFileNameWindow;

  GtkWidget *parentWindow;
  GtkBuilder *parentBuilder;
} EditValueFileNameCallbackData;

/** Creates a fresh filename dialog used for specifying editValue filenames and/or insertions of text. Requires a GtkWidget pointer to the window whose associated tuple is changed. */
EditValueFileNameCallbackData *create_editValueFileNameDialogue(GtkWidget *parentWindow, GtkBuilder *parentBuilder) {
  GtkWidget *dialog;
  GtkBuilder *builder;
  builder = gtk_builder_new();
  char xmlfilename[512];
  snprintf(xmlfilename,sizeof(xmlfilename),"%s/%s/editValueFileName.ui",PACKAGE_DATA_DIR,PACKAGE);
  gtk_builder_add_from_file(builder,xmlfilename,NULL);
  dialog = GTK_WIDGET(gtk_builder_get_object(builder,"editValueFileNameDialogue"));

  EditValueFileNameCallbackData *data = (EditValueFileNameCallbackData *) malloc(sizeof(EditValueFileNameCallbackData));
  data->editValueFileNameWindow = dialog;
  data->builder = builder;
  data->parentWindow = parentWindow;
  data->parentBuilder = parentBuilder;

  gtk_builder_connect_signals(builder,(gpointer) data);
  gtk_widget_show (dialog);
  return data;
}

void on_editValueFileNameDialog_close (GtkDialog  *dialog,gpointer user_data) {
}


void on_editValue_insertName_pressed(GtkButton *button, EditValueFileNameCallbackData *data) {
  char *filename;
  GtkWidget *editValueTextView = GTK_WIDGET(gtk_builder_get_object(data->parentBuilder,"editValueTextView"));
  GtkWidget *filechooser = data->editValueFileNameWindow;
  GtkTextBuffer *buffer;

  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(filechooser));
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));
   
  gtk_text_buffer_set_text(buffer, filename, strlen(filename));
  gtk_text_view_set_buffer(GTK_TEXT_VIEW(editValueTextView), buffer);

  /* Remove the dialogue */
  GdkEvent event;
  event.any.type = GDK_DELETE;
  event.any.window = filechooser->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  

  g_object_unref(data->builder);
  free(data);
}

void on_editValue_insertData_pressed(GtkButton *button, EditValueFileNameCallbackData *data) {
  char *filename;
  GtkWidget *editValueTextView = GTK_WIDGET(gtk_builder_get_object(data->parentBuilder,"editValueTextView"));
  GtkWidget *filechooser = data->editValueFileNameWindow;
  GtkTextBuffer *buffer;
  
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(filechooser));
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editValueTextView));

  FILE *fp = fopen(filename,"rb");
  fseek(fp,0,SEEK_END);
  int size=ftell(fp);
  fseek(fp,0,SEEK_SET);
  if(size > 10*(1<<20)) {
    printf("File is too large to insert!\n");
  } else {
    void *buffer2=(void*) malloc(size+1);
    if(!buffer2) printf("Error allocating %d bytes\n",size); 
    else {
      fread(buffer2,1,size,fp);
      ((char*)buffer2)[size]=0;
      gtk_text_buffer_set_text(buffer, buffer2, size-1);
      gtk_text_view_set_buffer(GTK_TEXT_VIEW(editValueTextView), buffer);
      free(buffer2);
    }
  }

  /* Remove the dialogue */
  GdkEvent event;
  event.any.type = GDK_DELETE;
  event.any.window = filechooser->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  
  g_object_unref(data->builder);
  free(data);
}

void on_editValue_cancel_pressed(GtkButton *button,EditValueFileNameCallbackData *data) {
  /* Remove the dialogue */
  GdkEvent event;
  GtkWidget *filechooser = data->editValueFileNameWindow;
  event.any.type = GDK_DELETE;
  event.any.window = filechooser->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  
  g_object_unref(data->builder);
  free(data);
}
