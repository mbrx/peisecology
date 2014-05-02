/** \file tupleview.h 
    Contains all common prototypes, structs etc. for the tupleview program 
*/

/*                  */
/*    TYPEDEF's     */
/*                  */


/* This is the columns used in the tree víew */
enum {
  PTREE_DEPTH,  
  PTREE_PEISID,  
  PTREE_NAME,
  PTREE_COLOR,
  PTREE_NCOLUMNS,
};

typedef enum { FORMAT_PNG, FORMAT_SVG } e_RenderingFormat;

#define MAX_CONNECTION_STRINGS 5
#define MAX_WINDOWS 50
#define MAX_TUPLE_TREE_STORES (MAX_WINDOWS*2)

typedef struct Preferences {
  e_RenderingFormat renderingFormat;
  char excludeTupleview;
  char excludePeisInits;
  char copyTopologyImages;
  char copyDotFiles;
  char imageDotFileLabeling;
  char updateTopologyView;
  int topologyUpdateInterval;
  char *connectionStrings[MAX_CONNECTION_STRINGS];
  int detailedComponentInfo;
} Preferences;

typedef enum { eUnused=0, eMainWindow=1 } EToplevelWindowKind;
typedef struct ToplevelWindow {
  EToplevelWindowKind kind;
  GtkWidget *window;
  struct GladeXML *xml;
  
  /* Used by eMainWindow windows */
  GtkTreeStore *peisTree;

  /* Used by all windows that can show tuple contents */
  int selectedPeisID;
  char selectedTuple[256];
} ToplevelWindow;


/*                  */
/* GLOBAL VARIABLES */
/*                  */

extern Preferences gPreferences;
extern char gladeXMLfilename[512];

extern hasCallback;
extern PeisCallbackHandle callback;

/* shows if there has been an image writen to disk that we should use
   for the next redraw */
extern int tupleDrawingAreaOk;
extern char *tupleDrawingAreaType;
extern int tupleDrawingAreaImageSize[2];
/* Shows it there is a valid topology image on disk that can be used
   to visualize the topology */   
extern int topologyImageOk;
extern int topologyImageIsNew;
extern double topologyImageZoom;

/* The filename used for the tupleDrawingArea */
extern char tupleDrawingAreaName[256];

/* The filename used for the topologyImages */
extern char topologyImageName[256];
extern char topologyDotFile[256];

/* Filenames used for the dotGraph drawing area */
extern char tupleDotGraphImageName[256];
extern char tupleDotFile[256];

extern int isSubscribed;
extern PeisSubscriberHandle subscription,subscriptionClicks,subscriptionKeys;

extern double imageInfoCursorXY[2];

extern ToplevelWindow windows[MAX_WINDOWS];
extern int nWindows;

/*                  */
/*    PROTOTYPES    */
/*                  */

void initializeTree();
void repopulateAllTrees();
void repopulateTree(GtkTreeStore*);
void updateImageInfo();
void savePreferences();
void loadPreferences();
void repopulateEcologyview(GtkWidget *widget);
void repopulateAllEcologyviews();

void topologyUpdateView(GtkWidget *mainwindow);
void tupleviewPageActivated();
void ecologyviewPageActivated(GtkWidget *window);

void createMainWindow();

/** Inserts a new window into the list of toplevel windows and returns the corresponding toplevel window datastructure */
ToplevelWindow *insertToplevelWindow(EToplevelWindowKind kind, GtkWidget *widget, struct GladeXML *xml);
/** Removes the window from the list of toplevel windows */
void removeToplevelWindow(ToplevelWindow *window);
/** Find the corresponding toplevel window structure given *any* widget that belongs to it */
ToplevelWindow *findToplevelWindow(GtkWidget *anyWidget);

/** Creates and maintains a list of active tuple tree stores for use within mainwindow tuple selections and meta tuple target selections */
GtkTreeStore *createTupleTreeStore();
/** Removes a given tupletree store from list of active tuple tree stores */
void freeTupleTreeStore(GtkTreeStore *store);
