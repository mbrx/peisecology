#include <gtk/gtk.h>

#define  MAX_TUPLE_HISTORY 100
extern char *tupleHistory[MAX_TUPLE_HISTORY];
extern int nTupleHistory;
extern int currentTupleHistory;

/* Ugly hack to make it compilable on older GTK version */
#define gtk_about_dialog_set_wrap_license(X,Y) {}


void updateTupleValueText(GtkWidget *widget,GdkEventKey *event, ToplevelWindow *tl);

struct EditValueFileNameCallbackData *create_editValueFileNameDialogue(GtkWidget *parentWindow,GtkBuilder *builder);
void initializeTupleValueHistory(ToplevelWindow *tl);
void peistree_selection_changed_cb (GtkTreeSelection *selection, ToplevelWindow *tl);
void metatree_selection_changed_cb (GtkTreeSelection *selection, ToplevelWindow *tl);
void dotGraphUpdateView(ToplevelWindow *tl);
