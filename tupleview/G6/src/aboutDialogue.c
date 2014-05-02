/** \file aboutDialogue.c
    Contains all callbacks for the about Dialogue
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

typedef struct AboutCallbackData {
  GtkBuilder *builder;
  GtkWidget *aboutWindow;
} AboutCallbackData;

AboutCallbackData *create_aboutDialogue() {
  GtkWidget *aboutDialog;
  GtkBuilder *builder;
  builder = gtk_builder_new();

  char xmlfilename[512];
  snprintf(xmlfilename,sizeof(xmlfilename),"%s/%s/about.ui",PACKAGE_DATA_DIR,PACKAGE);
  gtk_builder_add_from_file(builder,xmlfilename,NULL);
  aboutDialog = GTK_WIDGET(gtk_builder_get_object(builder,"aboutDialogue"));

  AboutCallbackData *data = (AboutCallbackData *) malloc(sizeof(AboutCallbackData));
  data->aboutWindow = aboutDialog;
  data->builder = builder;

  gtk_builder_connect_signals(builder,(gpointer) data);
  gtk_widget_show (aboutDialog);

  /* Note: we do not unreference the builder since it is reachable through the PrefCallbackData structure above. */
  return data;
}


void on_about_activate(GtkMenuItem *menuitem,gpointer user_data) {
  GtkWidget *aboutMenu;
  aboutMenu = create_aboutDialogue();
  gtk_widget_show (aboutMenu);
}

void on_aboutDialogue_close (GtkDialog *dialog,AboutCallbackData *data) {
  g_object_unref(data->builder);
  free(data);
}

void on_aboutDialogue_response (GtkDialog *dialog, gint response_id, AboutCallbackData *data)
{
  /* Ok, this is most certainlty the wrong way to do it... but hey, it
     works! */
  static int rec=0;
  if(rec) return;

  GdkEvent event;
  GtkWidget *widget = GTK_WIDGET(dialog);
  event.any.type = GDK_DELETE;
  event.any.window = widget->window;
  event.any.send_event = TRUE;
  
  g_object_ref (event.any.window);
  rec++;
  gtk_main_do_event (&event);
  rec--;
  g_object_unref (event.any.window);
}
