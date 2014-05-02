#include <gtk/gtk.h>

#define  MAX_TUPLE_HISTORY 100
extern char *tupleHistory[MAX_TUPLE_HISTORY];
extern int nTupleHistory;
extern int currentTupleHistory;

/* Ugly hack to make it compilable on older GTK version */
#define gtk_about_dialog_set_wrap_license(X,Y) {}


void
on_quit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_cut1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_copy1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_paste1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_delete1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void peistree_selection_changed_cb     (GtkTreeSelection *selection, GtkWidget *mainwindow);

void
on_newTupleButton_pressed              (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_mainwindow_destroy_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_mainwindow_destroy                  (GtkObject       *object,
                                        gpointer         user_data);

gboolean
on_mainwindow_delete_event             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_tupleDrawingArea_expose_event       (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

void
on_tsUserEntry21_changed                 (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_tsExpireEntry21_changed                 (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_tupleKeyEntry_changed               (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_save_tuple_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save_image_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_submitButton_pressed                (GtkButton       *button,
                                        gpointer         user_data);

void
on_grabButton_pressed                  (GtkButton       *button,
                                        gpointer         user_data);

void on_updateTopology_pressed (GtkButton *button,gpointer user_data);


gboolean
on_topologyDrawingArea_expose_event    (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

void
on_toolbutton1_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_toolbutton2_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_topologyUpdate_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_topologyZoomIn_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_topologyZoomOut_clicked             (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_topologyCancel_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_aboutdialog_close                   (GtkDialog       *dialog,
                                        gpointer         user_data);

void
on_aboutdialog_response                (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
on_preferencesMenu_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_prefOK_clicked                      (GtkButton       *button,
                                        gpointer         user_data);

void
on_prefCancel_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_tupleDrawingArea_button_press_event (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_tupleImageViewport_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_imageInfoXYText_editing_done        (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
on_imageInfoUVText_editing_done        (GtkCellEditable *celleditable,
                                        gpointer         user_data);

void
on_imageInfoClickableButton_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_imageInfoKeyboardButton_toggled     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

gboolean
on_tupleImageViewport_key_press_event  (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

gboolean
on_tupleDrawingArea_key_press_event    (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
on_menuEcology_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menuConnect_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_connectDialogueOkButton_clicked     (GtkButton       *button,
                                        gpointer         user_data);

void
on_connectDialogueCancelButton_clicked (GtkButton       *button,
                                        gpointer         user_data);

void
on_connectDialogue_close               (GtkDialog       *dialog,
                                        gpointer         user_data);

void
on_treeviewQuit_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_peistreeview_button_press_event     (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_no1_activate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_item1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_menu1_delete_event                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_menu1_destroy_event                 (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_combobox1_changed                   (GtkComboBox     *combobox,
                                        gpointer         user_data);

void
on_configurator_cbx_changed            (GtkComboBox     *combobox,
                                        gpointer         user_data);

void
on_dotGraphDot_rb_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_dotGraphFdp_rb_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_dotGraphCirco_rb_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_dotGraphDot_rb_group_changed        (GtkRadioButton  *radiobutton,
                                        gpointer         user_data);

void
on_dotGraphDot_rb_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_dotGraphPng_rb_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_dotGraphSvg_rb_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_dotGraph_rb_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

gboolean
on_dotGraphDrawingArea_expose_event    (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

void
on_dotGraphZoomIn_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_dotGraphZoomOut_clicked             (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

gboolean
on_notebook_change_current_page        (GtkNotebook     *notebook,
                                        gint             offset,
                                        gpointer         user_data);

gboolean
on_notebook1_select_page               (GtkNotebook     *notebook,
                                        gboolean         move_focus,
                                        gpointer         user_data);

void
on_notebook1_switch_page               (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_tupleviewNotebook_switch_page       (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

gboolean
on_editValueTextView_key_release_event (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

gboolean
on_editValueTextView_key_press_event   (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
on_tupleFileNameDialog_close           (GtkDialog       *dialog,
                                        gpointer         user_data);

void
on_ok_button_tuplefilename_dialog_pressed
                                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_cancel_button_tuplefilename_dialog_pressed
                                        (GtkButton       *button,
                                        gpointer         user_data);

void create_editValueFileNameDialogue(GtkWidget *parentWindow);
void on_editValueFileNameDialogue_close (GtkDialog  *dialog,gpointer user_data);
void on_editValue_insertName_pressed(GtkButton *button, GtkWidget *parentwindow);
void on_editValue_insertData_pressed(GtkButton *button, GtkWidget *parentwindow);
void on_editValue_cancel_pressed(GtkButton *button,GtkWidget *user_data);

void initializeTupleValueHistory(GtkWidget *mainwindow);
