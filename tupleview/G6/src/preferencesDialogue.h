/** \file preferencesDialogue.h
   Contains all callbacks for the preferences dialogue
*/

typedef struct PreferencesCallbackData {
  GtkBuilder *builder;
  GtkWidget *preferencesWindow;
} PreferencesCallbackData;

void preferences_copyToWindow(struct PreferencesCallbackData *data);
void preferences_copyFromWindow(struct PreferencesCallbackData *data);
