/*
** editValueFileNameDialogue
** 
** Contains all code related to connection dialogues
** 
** Made by (Mathias Broxvall)
** Login   <mbl@brox>
** 
** Started on  Wed Mar 12 18:16:39 2008 Mathias Broxvall
** Last update Sun May 12 01:17:25 2002 Speed Blue
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

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

/* Creates a fresh filename dialog used for specifying editValue filenames and/or insertions of text. Requires a GtkWidget pointer to the window whose tuple is changed. */
void create_editValueFileNameDialogue(GtkWidget *parentWindow) {
  GladeXML *xml;
  GtkWidget *dialogTop;
  xml = glade_xml_new(gladeXMLfilename, "editValueFileNameDialogue", NULL);
  dialogTop = glade_xml_get_widget(xml,"editValueFileNameDialogue");
  glade_xml_signal_connect_data(xml,"on_editValueFileNameDialogue_close", (GCallback) on_editValueFileNameDialogue_close, NULL);
  glade_xml_signal_connect_data(xml,"on_editValue_insertName_pressed", (GCallback) on_editValue_insertName_pressed, parentWindow);
  glade_xml_signal_connect_data(xml,"on_editValue_insertData_pressed", (GCallback) on_editValue_insertData_pressed, parentWindow);
  glade_xml_signal_connect_data(xml,"on_editValue_cancel_pressed", (GCallback) on_editValue_insertName_pressed, NULL);
  gtk_widget_show (dialogTop);
}

void on_editValueFileNameDialogue_close (GtkDialog  *dialog,gpointer user_data) {
}


void on_editValue_insertName_pressed(GtkButton *button, GtkWidget *parentwindow) {
  char *filename;
  GtkWidget *editValueTextView = lookup_widget(parentwindow,"editValueTextView");
  GtkWidget *filechooser = lookup_widget(GTK_WIDGET(button),"editValueFileNameDialogue");
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
}

void on_editValue_insertData_pressed(GtkButton *button, GtkWidget *parentwindow) {
  char *filename;
  GtkWidget *editValueTextView = lookup_widget(parentwindow,"editValueTextView");
  GtkWidget *filechooser = lookup_widget(GTK_WIDGET(button),"editValueFileNameDialogue");
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
}

void on_editValue_cancel_pressed(GtkButton *button,GtkWidget *user_data) {
  /* Remove the dialogue */
  GdkEvent event;
  GtkWidget *filechooser = lookup_widget(GTK_WIDGET(button),"editValueFileNameDialogue");
  event.any.type = GDK_DELETE;
  event.any.window = filechooser->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  
}
