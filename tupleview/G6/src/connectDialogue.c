/** \file connectDialogue.c 
    Contains all code related to connection dialogues
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

typedef struct ConnectCallbackData {
  GtkBuilder *builder;
  GtkWidget *connectDialog;
} ConnectCallbackData;


ConnectCallbackData *create_connectDialog() {
  GtkWidget *dialogTop;  
  GtkBuilder *builder;
  builder = gtk_builder_new();
  char xmlfilename[512];
  snprintf(xmlfilename,sizeof(xmlfilename),"%s/%s/connect.ui",PACKAGE_DATA_DIR,PACKAGE);
  gtk_builder_add_from_file(builder,xmlfilename,NULL);
  dialogTop = GTK_WIDGET(gtk_builder_get_object(builder,"connectDialogue"));

  ConnectCallbackData *data = (ConnectCallbackData *) malloc(sizeof(ConnectCallbackData));
  data->connectDialog = dialogTop;
  data->builder = builder;

  gtk_builder_connect_signals(builder,(gpointer) data);
  gtk_widget_show (dialogTop);

  /* Note: we do not unreference the builder since it is reachable through the PrefCallbackData structure above. */
  return data;
}


void on_menuConnect_activate (GtkMenuItem *menuitem,gpointer user_data) {
  int i;

  ConnectCallbackData *data = create_connectDialog();

  GtkComboBoxEntry *urlEntry = GTK_COMBO_BOX_ENTRY(gtk_builder_get_object(data->builder,"connectDialogueURLEntry"));  
  for(i=0;i<MAX_CONNECTION_STRINGS;i++) 
    if(gPreferences.connectionStrings[i])
      gtk_combo_box_append_text (GTK_COMBO_BOX (urlEntry), gPreferences.connectionStrings[i]);

  if(!gPreferences.connectionStrings[0]) {
    gtk_combo_box_append_text (GTK_COMBO_BOX (urlEntry), _("tcp://localhost:8000"));
  }

  gtk_widget_show(data->connectDialog);
}

void on_connectDialogueOkButton_clicked (GtkButton *button,ConnectCallbackData *data) {
  int i;

  GtkWidget *connectDialog = data->connectDialog;
  GtkComboBox *urlEntry = GTK_COMBO_BOX(gtk_builder_get_object(data->builder,"connectDialogueURLEntry"));
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
  event.any.window = connectDialog->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  

  g_object_unref (data->builder);  
  free(data);
}


void on_connectDialogueCancelButton_clicked (GtkButton *button,ConnectCallbackData *data) {
  /* Remove the dialogue */
  GtkWidget *connectDialog = data->connectDialog;
  GdkEvent event;
  event.any.type = GDK_DELETE;
  event.any.window = connectDialog->window;
  event.any.send_event = TRUE;  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  

  g_object_unref (data->builder);  
  free(data);
}

void on_connectDialogue_close (GtkDialog *dialog,gpointer user_data) {
  /** \todo - do we need to deallocate the memory here */
}
