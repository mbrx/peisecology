/** \file preferencesDialogue.c
   Contains all callbacks for the preferences dialogue
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
#include "tokenizer.h"
#include "preferencesDialogue.h"

PreferencesCallbackData *create_preferencesWindow() {
  GtkWidget *preferencesWindow;
  GtkBuilder *builder;
  builder = gtk_builder_new();
  char xmlfilename[512];
  snprintf(xmlfilename,sizeof(xmlfilename),"%s/%s/preferences.ui",PACKAGE_DATA_DIR,PACKAGE);
  gtk_builder_add_from_file(builder,xmlfilename,NULL);
  preferencesWindow = GTK_WIDGET(gtk_builder_get_object(builder,"preferencesWindow"));

  PreferencesCallbackData *data = (PreferencesCallbackData *) malloc(sizeof(PreferencesCallbackData));
  data->preferencesWindow = preferencesWindow;
  data->builder = builder;

  gtk_builder_connect_signals(builder,(gpointer) data);
  gtk_widget_show (preferencesWindow);

  /* Note: we do not unreference the builder since it is reachable through the PrefCallbackData structure above. */
  return data;
}

void on_preferencesMenu_activate (GtkMenuItem *menuitem,gpointer user_data) {
  PreferencesCallbackData *data = create_preferencesWindow();
  gtk_widget_show (data->preferencesWindow);
  preferences_copyToWindow(data);

}

void on_prefOK_clicked (GtkButton *button,PreferencesCallbackData *data) {
  GtkWidget *pref = data->preferencesWindow;
  //GTK_WIDGET(gtk_builder_get_object(data->builder,"preferencesWindow"));
  GdkEvent event;

  printf("prefOK clicked\n");

  preferences_copyFromWindow(data);

  event.any.type = GDK_DELETE;
  event.any.window = pref->window;
  event.any.send_event = TRUE;
  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  

  g_object_unref(data->builder);
  free(data);
}

void on_prefCancel_clicked (GtkButton *button,PreferencesCallbackData *data) {
  GtkWidget *pref = data->preferencesWindow;
  GdkEvent event;

  event.any.type = GDK_DELETE;
  event.any.window = pref->window;
  event.any.send_event = TRUE;
  
  g_object_ref (event.any.window);
  gtk_main_do_event (&event);
  g_object_unref (event.any.window);  

  g_object_unref(data->builder);
  free(data);
}

/** Enters the values into the preferences window using our currently
    used preferences. */
void preferences_copyToWindow(PreferencesCallbackData *data) {
  GtkComboBox *renderingFormat = GTK_COMBO_BOX(gtk_builder_get_object(data->builder,"prefRenderingFormat"));
  gtk_combo_box_set_active(renderingFormat,gPreferences.renderingFormat);  
  
  GtkToggleButton *excludeTupleview = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefExcludeTupleview_checkbutton"));
  gtk_toggle_button_set_active(excludeTupleview,gPreferences.excludeTupleview);
  
  GtkToggleButton *excludePeisInits = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefExcludePeisInits_checkbutton"));
  gtk_toggle_button_set_active(excludePeisInits,gPreferences.excludePeisInits);
  
  GtkToggleButton *copyImages = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefCopyTopologyImages_checkbutton"));
  gtk_toggle_button_set_active(copyImages,gPreferences.copyTopologyImages);
  
  GtkToggleButton *copyDotFiles = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefCopyDotFiles_checkbutton"));
  gtk_toggle_button_set_active(copyDotFiles,gPreferences.copyDotFiles);
  
  GtkComboBox *imageDotFileLabeling = GTK_COMBO_BOX(gtk_builder_get_object(data->builder,"prefImageDotFileLabeling_combobox"));
  gtk_combo_box_set_active(imageDotFileLabeling,gPreferences.imageDotFileLabeling);  
  
  GtkToggleButton *updateTopologyView = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefUpdateTopologyView_checkbutton"));
  gtk_toggle_button_set_active(updateTopologyView,gPreferences.updateTopologyView);  

  GtkToggleButton *detailedComponentInfo = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefDetailedComponentInfo_checkbutton"));
  gtk_toggle_button_set_active(detailedComponentInfo,gPreferences.detailedComponentInfo);  

}

/** Changes current preferences accoriding to prefernces window. */
void preferences_copyFromWindow(PreferencesCallbackData *data) {
  GtkComboBox *renderingFormat = GTK_COMBO_BOX(gtk_builder_get_object(data->builder,"prefRenderingFormat"));
  if(!renderingFormat) return;
  int format=gtk_combo_box_get_active(renderingFormat);
  gPreferences.renderingFormat = format==-1?0:format;
  
  GtkToggleButton *excludeTupleview = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefExcludeTupleview_checkbutton"));
  if(!excludeTupleview) return;
  gPreferences.excludeTupleview = gtk_toggle_button_get_active(excludeTupleview);
  
  GtkToggleButton *excludePeisInits = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefExcludePeisInits_checkbutton"));
  if(!excludePeisInits) return;
  gPreferences.excludePeisInits = gtk_toggle_button_get_active(excludePeisInits);
  
  GtkToggleButton *copyImages = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefCopyTopologyImages_checkbutton"));
  if(!copyImages) return;
  gPreferences.copyTopologyImages = gtk_toggle_button_get_active(copyImages);
  
  GtkToggleButton *copyDotFiles = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefCopyDotFiles_checkbutton"));
  if(!copyDotFiles) return;
  gPreferences.copyDotFiles = gtk_toggle_button_get_active(copyDotFiles);
  
  GtkComboBox *imageDotFileLabeling = GTK_COMBO_BOX(gtk_builder_get_object(data->builder,"prefImageDotFileLabeling_combobox"));
  if(!imageDotFileLabeling) return;
  int labeling=gtk_combo_box_get_active(imageDotFileLabeling);
  gPreferences.imageDotFileLabeling = labeling==-1?0:labeling;
  
  GtkToggleButton *updateTopologyView = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefUpdateTopologyView_checkbutton"));
  if(!updateTopologyView) return;
  gPreferences.updateTopologyView = gtk_toggle_button_get_active(updateTopologyView);

  GtkToggleButton *detailedComponentInfo = GTK_TOGGLE_BUTTON(gtk_builder_get_object(data->builder,"prefDetailedComponentInfo_checkbutton"));
  gPreferences.detailedComponentInfo = gtk_toggle_button_get_active(detailedComponentInfo);  

  savePreferences();
}


int preferencesVersion=1;
void savePreferences() {
  FILE *fp;
  char filename[256];
  int i;

  snprintf(filename,sizeof(filename),"%s/.tupleview",getenv("HOME"));
  fp = fopen(filename,"wb");
  if(!fp) {
    printf("Warning, could not create %s to save preferences in\n",filename);
    return;
  }
  fprintf(fp,"version: %d\n",preferencesVersion);
  fprintf(fp,"renderingFormat: %d\n",gPreferences.renderingFormat);
  fprintf(fp,"excludeTupleview: %d\n",gPreferences.excludeTupleview);
  fprintf(fp,"excludePeisInits: %d\n",gPreferences.excludePeisInits);
  fprintf(fp,"copyTopologyImages: %d\n",gPreferences.copyTopologyImages);
  fprintf(fp,"copyDotFiles: %d\n",gPreferences.copyDotFiles);
  fprintf(fp,"imageDotFileLabeling: %d\n",gPreferences.imageDotFileLabeling);
  fprintf(fp,"updateTopologyView: %d\n",gPreferences.updateTopologyView);
  fprintf(fp,"connectionStrings: ");
  for(i=0;i<5;i++) if(gPreferences.connectionStrings[i]) fprintf(fp,"%s ",gPreferences.connectionStrings[i]);
  fprintf(fp,"|\n");
  fprintf(fp,"detailedComponentInfo: %d\n",gPreferences.detailedComponentInfo);

  /*fprintf(fp,"topologyUpdateInterval: %d\n",gPreferences.topologyUpdateInterval);*/
  fclose(fp);
}
void loadPreferences() {
  int i;
  int version;
  char str[256];
  FILE *fp;
  char filename[256];
  char keyword[256],value[256];

  /* Initialize default preferences */
  gPreferences.renderingFormat=FORMAT_PNG;
  gPreferences.excludeTupleview=0;
  gPreferences.excludePeisInits=0;
  gPreferences.copyTopologyImages=0;
  gPreferences.copyDotFiles=0;
  gPreferences.imageDotFileLabeling=0;
  gPreferences.updateTopologyView=1;  
  gPreferences.topologyUpdateInterval=0;

  for(i=0;i<MAX_CONNECTION_STRINGS;i++) gPreferences.connectionStrings[i]=NULL;

  /* Load preferences saved to disk */
  snprintf(filename,sizeof(filename),"%s/.tupleview",getenv("HOME"));
  fp = fopen(filename,"rb");
  lineno=0;
  while(fp) {
    if(getToken(fp,keyword,sizeof(keyword))) break;
    if(getToken(fp,value,sizeof(value))) break;
    if(strcmp(keyword,"renderingFormat:") == 0) {
      gPreferences.renderingFormat = atoi(value);
    } else if(strcmp(keyword,"excludeTupleview:") == 0) {
      gPreferences.excludeTupleview = atoi(value);
    } else if(strcmp(keyword,"excludePeisInits:") == 0) {
      gPreferences.excludePeisInits = atoi(value);
    } else if(strcmp(keyword,"copyTopologyImages:") == 0) {
      gPreferences.copyTopologyImages = atoi(value);
    } else if(strcmp(keyword,"copyDotFiles:") == 0) {
      gPreferences.copyDotFiles = atoi(value);
    } else if(strcmp(keyword,"imageDotFileLabeling:") == 0) {
      gPreferences.imageDotFileLabeling = atoi(value);
    } else if(strcmp(keyword,"detailedComponentInfo:") == 0) {
      gPreferences.detailedComponentInfo = atoi(value);
    } else if(strcmp(keyword,"updateTopologyView:") == 0) {
      gPreferences.updateTopologyView = atoi(value);
    } else if(strcmp(keyword,"connectionStrings:") == 0) {
      for(i=0;;) {
	if(strcmp(value,"|") == 0) break;
	if(i<MAX_CONNECTION_STRINGS) {
	  gPreferences.connectionStrings[i] = strdup(value);
	  i++;
	}
	else {
	  printf("Warning, preferences file contain invalid connection strings: '%s'\n",value);
	}
	if(getToken(fp,value,sizeof(value))) break;
      }
      if(i<MAX_CONNECTION_STRINGS) 
	for(;i<MAX_CONNECTION_STRINGS;i++) gPreferences.connectionStrings[i]=0;
    }

    /*
    if(fscanf(fp,"version: %d\n",&version) != 1) break;
    if(fscanf(fp,"renderingFormat: %d\n",&gPreferences.renderingFormat) != 1) break;
    if(fscanf(fp,"excludeTupleview: %d\n",&gPreferences.excludeTupleview) != 1) break;
    if(fscanf(fp,"excludePeisInits: %d\n",&gPreferences.excludePeisInits) != 1) break;    
    if(fscanf(fp,"copyTopologyImages: %d\n",&gPreferences.copyTopologyImages) != 1) break;
    if(fscanf(fp,"copyDotFiles: %d\n",&gPreferences.copyDotFiles) != 1) break;
    if(fscanf(fp,"imageDotFileLabeling: %d\n",&gPreferences.imageDotFileLabeling) !=1) break;
    if(fscanf(fp,"updateTopologyView: %d\n",&gPreferences.updateTopologyView) != 1) break;
    */
    /* if(fscanf(fp,"topologyUpdateInterval: %d\n",&gPreferences.topologyUpdateInterval) != 1) break;*/    

  } 
  /*
    printf("format: %d\n",gPreferences.renderingFormat);
    printf("ex tupleview: %d\n",gPreferences.excludeTupleview);
    printf("ex peisinits: %d\n",gPreferences.excludePeisInits);
    printf("copy topology: %d\n",gPreferences.copyTopologyImages);
    printf("update topologyview: %d\n",gPreferences.updateTopologyView);
  */
}
