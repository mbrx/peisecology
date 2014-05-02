/*
** connectDialogue.c
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


GtkWidget *create_connectDialogue() {
  GladeXML *xml;
  GtkWidget *dialogTop;
  xml = glade_xml_new(gladeXMLfilename, "connectDialogue", NULL);
  dialogTop = glade_xml_get_widget(xml,"connectDialogue");
  glade_xml_signal_autoconnect(xml);
  gtk_widget_show (dialogTop);
  return dialogTop;
}


void on_menuConnect_activate (GtkMenuItem *menuitem,gpointer user_data) {
  int i;

  GtkWidget *connectDialogue;
  connectDialogue = create_connectDialogue();
  gtk_widget_show(connectDialogue);

  GtkComboBoxEntry *urlEntry = GTK_COMBO_BOX_ENTRY(lookup_widget(connectDialogue,"connectDialogueURLEntry"));  
  for(i=0;i<MAX_CONNECTION_STRINGS;i++) 
    if(gPreferences.connectionStrings[i])
      gtk_combo_box_append_text (GTK_COMBO_BOX (urlEntry), gPreferences.connectionStrings[i]);

  if(!gPreferences.connectionStrings[0]) {
    gtk_combo_box_append_text (GTK_COMBO_BOX (urlEntry), _("tcp://localhost:8000"));
  }

}

void on_connectDialogueOkButton_clicked (GtkButton       *button,gpointer user_data) {
  int i;

  GtkWidget *connectDialogue = lookup_widget(GTK_WIDGET(button), "connectDialogue");
  GtkComboBoxEntry *urlEntry = GTK_COMBO_BOX_ENTRY(lookup_widget(connectDialogue,"connectDialogueURLEntry"));
  if(urlEntry) {
    const char *str =  gtk_combo_box_get_active_text(urlEntry);

    if(str[0]) {
      /* This performs a simple sort by last use of the connection
	 strings. */
      /* Remove any previous occurance of this string from preferneces
	 connection strings */
      for(i=0;i<MAX_CONNECTION_STRINGS;i++) {
	if(gPreferences.connectionStrings[i] && strcmp(gPreferences.connectionStrings[i],str) == 0) {
	  free(gPreferences.connectionStrings[i]);
	  for(;i<MAX_CONNECTION_STRINGS-1;i++)
	    gPreferences.connectionStrings[i] = gPreferences.connectionStrings[i+1];
	  break;
	}
      }
      /* Add this string to the begining */
      if(gPreferences.connectionStrings[MAX_CONNECTION_STRINGS-1]) free(gPreferences.connectionStrings[MAX_CONNECTION_STRINGS-1]);
      for(i=MAX_CONNECTION_STRINGS-2;i>=0;i--) {
	gPreferences.connectionStrings[i+1] = gPreferences.connectionStrings[i];
      }
      gPreferences.connectionStrings[0] = strdup(str);      
      savePreferences();

      printf("Connecting to: %s\n",str);
      peisk_connect((char*)str,0);
    }
  }

  /* Remove the dialogue */
  GdkEvent event;
  event.any.type = GDK_DELETE;
  event.any.window = connectDialogue->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  
  connectDialogue=NULL;
}


void on_connectDialogueCancelButton_clicked (GtkButton       *button,
                                        gpointer         user_data) {
  /* Remove the dialogue */
  GtkWidget *connectDialogue = lookup_widget(GTK_WIDGET(button), "connectDialogue");
  GdkEvent event;
  event.any.type = GDK_DELETE;
  event.any.window = connectDialogue->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  
}

void on_connectDialogue_close (GtkDialog *dialog,gpointer user_data) {
}
