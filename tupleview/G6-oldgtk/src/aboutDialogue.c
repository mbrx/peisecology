/*
** aboutDialogue.c
**
** Contains all callbacks for the about Dialogue
** 
** Made by (Mathias Broxvall)
** Login   <mbl@brox>
** 
** Started on  Wed Mar 12 18:22:30 2008 Mathias Broxvall
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

GtkWidget *create_aboutDialogue() {
  GladeXML *xml;
  GtkWidget *dialogTop;
  xml = glade_xml_new(gladeXMLfilename, "aboutDialogue", NULL);
  dialogTop = glade_xml_get_widget(xml,"aboutDialogue");
  glade_xml_signal_autoconnect(xml);
  return dialogTop;
}


void on_about1_activate(GtkMenuItem *menuitem,gpointer user_data) {
  GtkWidget *aboutMenu;
  aboutMenu = create_aboutDialogue();
  gtk_widget_show (aboutMenu);
}

void on_aboutDialogue_close (GtkDialog *dialog,gpointer         user_data) {
}

void on_aboutDialogue_response (GtkDialog       *dialog, gint             response_id, gpointer         user_data)
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
