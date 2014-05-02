/* main.cc
   Copyright (C) 2004  Mathias Broxvall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//#define USE_V4L2

#include "general.h"
#include <getopt.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include "pthread.h"
#include <fcntl.h>
#include <string.h>

#include "unistd.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <zlib.h>
#include <dirent.h>

#ifdef USE_V4L
#include <linux/types.h>
#include <linux/videodev.h>
#endif

#ifdef USE_V4L2
#include <linux/videodev2.h>
#endif

#ifdef USE_PLAYER
#include <libplayerc/playerc.h>
#endif

#include "jpg_utils.h"
#include "peiskernel/peiskernel.h"
#include "peiskernel/peiskernel_mt.h"
#include "ccvt.h"
#include "png_utils.h"
#include "utils.h"

/*                  */
/* Global variables */
/*                  */

const char* program_name;
char *gShareDir="./";
int is_running=1;                 /* Keep system running as long as true */
int clientSockets[16];
int debug=0;
int scan;
int *integratedImage;
int palette;
unsigned char *conversionBuffer;   /* Stores converted RGB data when palette is not RGB24 */
static int totmean;

/*                  */
/*    Prototypes    */
/*                  */

void grabLoop();
void prepareCamera();
void freeCamera();
static int clip_to(int x, int low, int high);
void prepareCameraDirectoryDevice();
void freeCameraDirectoryDevice();
/** Grabs from a directory, returns pointer to decoded memory area. */
unsigned char *grabDirectoryDevice();

/** Grabs from a V4L1 device, returns pointer to decoded memory area. */
#ifdef USE_V4L
unsigned char *grabV4L1();
void prepareCameraV4L1Device();
void freeCameraV4L1Device();
#endif

#ifdef USE_V4L2
/** Grabs from a V4L2 device, returns pointer to decoded memory area. */
unsigned char *grabV4L2();
void prepareCameraV4L2Device();
void freeCameraV4L2Device();
#endif

#ifdef USE_PLAYER
/** Grabs from a Player device, returns pointed to decoded memory area
    */
unsigned char *grabPlayer();
void preparePlayerDevice();
void freePlayerDevice();
#endif

//  Brightness adjustment prototypes
#ifdef V4L1
static int clip_to(int x, int low, int high);
static int adjust_bright( struct video_picture *videopict, int fd);
static int get_pic_mean ( int width, int height, const unsigned char *buffer,
			  int is_rgb,int startx, int starty, int endx, 
			  int endy );
static int setVideoPict (struct video_picture *videopict, int fd);
#endif

/* Reads the settings for images from the tuplespace back to memory */
void readTupleImageSettings();
void writeTupleImageSettings();
int updateTupleImageSettings();


#define OPTION_NTSC             1
#define OPTION_PAL              2
#define OPTION_BGR              3
#define OPTION_PNG              4
#define OPTION_FLIPIMAGE        5
#define OPTION_ROTATEIMAGE_180  6
#define OPTION_SYNCSTART        7 
#define OPTION_V4L2             8
#define OPTION_PLAYER_HOST      9
#define OPTION_PLAYER_PORT      10
#define OPTION_PLAYER_CAMERA_INDEX 11
#define OPTION_AUTOBRIGHTNESS		12
#define OPTION_V4L              13
#define OPTION_FORCE_CAPTURE    14

/* autobright Flags */ 
#define V4L_BRIGHT_MIN 0
#define V4L_BRIGHT_MAX 65000 //65000
#define BRIGHTMEAN 128 //145
#define SPRING_CONSTANT 20
#define BRIGHTWINDOW 10

#define MAX_WIDTH 2048
#define MAX_HEIGHT 2048

/*                                   */
/* Settings used by all camera modes */
/*                                   */

const char *camera;
double wantedFPS=15.0;
int wantedWidth=640;
int wantedHeight=480;
int minQuality=50;
int pipeLength=4;
int integralSteps=1;
int wantedBrightness=32768;
int autoBrightness=0;

#ifdef USE_V4L
int videoNorm=VIDEO_MODE_NTSC;
#endif

int switchRGB=0;
int wantedChannel=0;
int flipImage=0;
int rotateImage=0;
int outputPNG=0;
int width,height;
int frame;
/** If true, forces framegrabbing even when no-one is interested in the resulting images */
int force_capture=0;
double syncStart=0.0;

/** The actual device used when opened */
int device;  

#define CAMERA_MODE_DIRECTORY     0
#define CAMERA_MODE_V4L1          1
#define CAMERA_MODE_V4L2          2
#define CAMERA_MODE_PLAYER        3
/** Determines the type of camera source used */
int cameraMode;


/*              */
/* V4L1 structs */
/*              */

#ifdef USE_V4L
struct video_capability cap;
struct video_window win;
struct video_channel channel;
struct video_mbuf mbuf;
struct video_picture pict;
struct video_mmap v_mmap;

/* The image buffer used by the device (MMAP'ed) */
unsigned char *buffer=NULL;
unsigned char *realFrame=NULL;
#endif

/*              */
/* V4L2 structs */
/*              */
#ifdef USE_V4L2
#define MAX_V4L2_BUFFERS 4
int num_v4l2_buffers=4;
struct v4l2_capability v4l2_cap;
struct v4l2_format v4l2_fmt;
struct v4l2_buffer v4l2_buf; //[MAX_V4L2_BUFFERS];
struct v4l2_requestbuffers v4l2_rb;
int v4l2_format;

unsigned char *v4l2_mem[MAX_V4L2_BUFFERS];

/** This is the buffer where the mjpeg images are received from the
    camera if operarting in mjpeg mode */
unsigned char *mjpeg_buffer=NULL;
/** And this is where the decoded images are stored when operating in
    mjpeg mode. */
unsigned char *v4l2_buffer=NULL;
unsigned char *v4l2_buffer2=NULL;
#endif

/*                       */
/* DirectoryMode globals */
/*                       */

unsigned char *camdir_jpegData=NULL;
unsigned char *camdir_fileImageBuffer=NULL;
DIR *camdir_dir;

/*                       */
/*    Player globals     */
/*                       */

#ifdef USE_PLAYER
unsigned char *plyCameraBuffer=NULL;
char *plyHost="localhost";
int plyPort=6665;
int plyCameraIndex=0;
/** Reference to player if we are grabbing from a player source */
playerc_client_t *plyClient;
playerc_camera_t *plyCamera;
#endif

/*            */
/*    CODE    */
/*            */


void print_usage(FILE *stream, int exit_code) {
  fprintf (stream, "Usage: %s\n", program_name);
  fprintf (stream, "  -d --debug              Print debug information\n");
  fprintf (stream, "  -c --camera             Select camera device or directory\n");
  fprintf (stream, "  -x --channel            Channel <int>\n");
  fprintf (stream, "  -f --fps                Optimal framerate\n");
  fprintf (stream, "  -w --width              Width\n");
  fprintf (stream, "  -h --height             Height\n");
  fprintf (stream, "  -q --quality            Minimum quality\n");
  fprintf (stream, "  -l --pipe-length        Length of image pipe\n");
  fprintf (stream, "  -i --integral-steps     Integrate and send only every N images\n");
  fprintf (stream, "     --sync-start <float> Synchronised starting time for first image in directory\n");
  fprintf (stream, "     --force-capture      Forces capturing and tuple generation even when no subscribers present\n");
  fprintf (stream, "     --pal                Use PAL video norm\n");
  fprintf (stream, "     --ntsc               Use NTSC video norm\n");
  fprintf (stream, "     --bgr                Switch RGB<->BGR pixels\n");
  fprintf (stream, "     --png                Output PNG instead of JPEG\n");
  fprintf (stream, "     --flip               Flip image around y-axis\n");
  fprintf (stream, "     --rotate-180         Rotate image 180 degrees\n");
#ifdef USE_V4L
  fprintf (stream, "     --v4l                Force camera mode to be V4L (1)\n");
#endif
#ifdef USE_V4L2
  fprintf (stream, "     --v4l2               Force camera mode to be V4L2\n");
#endif
#ifdef USE_PLAYER
  fprintf (stream, "     --player-host <host> Select host to connect to using player\n");
  fprintf (stream, "     --player-port <port> Port to connect to using player\n");
  fprintf (stream, "     --player-camera-index <index> Which camera to connect to\n");
#endif
  exit (exit_code);
}

int main(int argc,char **argv) {
  int i;
  float f;
  char str[256];

  camera="/dev/video0";
#ifndef USE_V4L
#ifndef USE_V4L2
  camera="./";
#endif
#endif
  wantedChannel=0;


  peiskmt_initialize(&argc,argv);

  cameraMode=CAMERA_MODE_DIRECTORY;
#ifdef USE_V4L
  cameraMode=CAMERA_MODE_V4L1;
#endif
#ifdef USE_V4L2
  cameraMode=CAMERA_MODE_V4L2;
#endif

  const char* const short_options = "Hdc:f:w:h:q:l:i:x:";
  const struct option long_options[] = {
    {"help", 0, NULL, 'H' },
    {"debug", 0, NULL, 'd' },
    {"camera", 1, NULL, 'c' },
    {"channel", 1, NULL, 'x' },
    {"fps", 1, NULL, 'f' },
    {"width", 1, NULL, 'w' },
    {"height", 1, NULL, 'h' },
    {"quality", 1, NULL, 'q' },
    {"pipe-length", 1, NULL, 'l' },
    {"integral-steps", 1, NULL, 'i' },
    {"ntsc", 0, NULL, OPTION_NTSC },
    {"pal", 0, NULL, OPTION_PAL },
    {"bgr", 0, NULL, OPTION_BGR },
    {"png", 0, NULL, OPTION_PNG },
    {"flip", 0, NULL, OPTION_FLIPIMAGE },
    {"rotate-180", 0, NULL, OPTION_ROTATEIMAGE_180 },
    {"sync-start", 1, NULL, OPTION_SYNCSTART },
    {"force-capture", 0, NULL, OPTION_FORCE_CAPTURE },
#ifdef USE_V4L
    {"v4l", 0, NULL, OPTION_V4L },
#endif
#ifdef USE_V4L2
    {"v4l2", 0, NULL, OPTION_V4L2 },
#endif
#ifdef USE_PLAYER
    {"player-host", 1, NULL, OPTION_PLAYER_HOST },
    {"player-port", 1, NULL, OPTION_PLAYER_PORT },
    {"player-camera-index", 1, NULL, OPTION_PLAYER_CAMERA_INDEX },
#endif
    { NULL, 0, NULL, 0 }
  };
  int nextOption;
  
  program_name = argv[0];
  do {
    nextOption = getopt_long (argc, argv, short_options, 
			       long_options, NULL);
    switch (nextOption) {
    case 'H':
      print_usage(stderr,0);
      peisk_printUsage(stderr,argc,argv);
      break;
    case 'd': debug=1; break;
    case 'c':
      camera=optarg;
      break;
    case 'x':
      wantedChannel=atoi(optarg);
      break;
    case 'f':
      wantedFPS=atoi(optarg);
      if(wantedFPS <= 12) pipeLength=1;
      else if(wantedFPS < 20) pipeLength=2;
      else pipeLength=4;
      break;
    case 'w':
      wantedWidth=atoi(optarg);
      break;
    case 'h':
      wantedHeight=atoi(optarg);
      break;
    case 'q':
      minQuality=atoi(optarg);
      break;
    case 'l':
      pipeLength=atoi(optarg);
      break;
    case 'i':
      integralSteps=atoi(optarg);
      break;
#ifdef USE_V4L
    case OPTION_PAL:
      videoNorm=VIDEO_MODE_PAL;
      break;
    case OPTION_NTSC:
      videoNorm=VIDEO_MODE_NTSC;
      break;
#endif
    case OPTION_BGR:
      switchRGB=1;
      break;
    case OPTION_PNG:
      outputPNG=1;
      break;
    case OPTION_FLIPIMAGE:
      flipImage=1;      
      break;
    case OPTION_ROTATEIMAGE_180:
      rotateImage=180;
      break;
    case OPTION_SYNCSTART:
      if(sscanf(optarg,"%f",&f) == 1) syncStart=f;
      else if(sscanf(optarg,"%d",&i) == 1) syncStart=i;
      else syncStart=0.0;
      break;
    case OPTION_FORCE_CAPTURE: force_capture=1; break;
#ifdef USE_V4L2
    case OPTION_V4L:
      cameraMode=CAMERA_MODE_V4L1;
      break;
#endif
#ifdef USE_V4L2
    case OPTION_V4L2:
      cameraMode=CAMERA_MODE_V4L2;
      break;
#endif
#ifdef USE_PLAYER
    case OPTION_PLAYER_HOST:
      cameraMode=CAMERA_MODE_PLAYER;
      plyHost=optarg;
      break;
    case OPTION_PLAYER_PORT:
      cameraMode=CAMERA_MODE_PLAYER;
      plyPort=atoi(optarg);
      break;
    case OPTION_PLAYER_CAMERA_INDEX:
      cameraMode=CAMERA_MODE_PLAYER;
      plyCameraIndex=atoi(optarg);
      break;
#endif
    case -1:
      break;
    }
  } while (nextOption != -1);
  
  snprintf(str,256,"peiscam%c",camera[strlen(camera)-1]);
  peiskmt_setStringTuple("name",str);
  peiskmt_setStringTuple("image","");
  snprintf(str,256,"%d",(int)wantedFPS);
  peiskmt_setDefaultStringTuple("settings.fps",str);

  /* We restart the grabLoop as long as "isRunning" is true. This way
     all parameters etc. can easily be reset by doing "return" from grabLoop */
  while(is_running && peiskmt_isRunning()) grabLoop();
  peiskmt_shutdown();
}



void grabLoop() {
  int i,j;
  int x,y;
  int len;
  char str[256];
  char *value;
  unsigned char *p1, *p2;
  unsigned char c1, c2, c3;
  int doSend;
  char *val;
  int retval;

  /* Use to count how many images we have taken and sent, usefull for testing */
  int imageCnt=0;

  /* Use to temporarily store copy of image for various image manipulation tasks  */
  unsigned char *improcBuffer=NULL;

  /* After grabbing a frame, always points to the RGB data */
  unsigned char *thisFrame;

  /* Buffer for storing (jpg) data before sending */
  unsigned char *sendbuffer=(unsigned char*)malloc(PNG_BUFFER_SIZE);

  /* Read settings from tuples */
  readTupleImageSettings();

  /* Prepare camera depending on selected device */  
  prepareCamera();

  /* Setup tuples describing our current settings */
  writeTupleImageSettings();

  unsigned char *jpgbuffer=sendbuffer;

  double avgTTS=0.1;

  if(integralSteps > 1) {
    integratedImage = (int *) malloc(width*height*3*sizeof(int));
    memset(integratedImage,0,width*height*3*sizeof(int));
  }
  else
    integratedImage=NULL;

  int step=0;
  int framecnt=0;
  int size;

  improcBuffer = (unsigned char*) malloc(width*height*3);
  double nextFrame;

  if(syncStart <= 0.0) 
    nextFrame=timeNow();
  else {
    /* Compute when to start grabbing images */
    nextFrame=timeNow()+syncStart-fmod(timeNow()+syncStart,syncStart);
    printf("Waiting to start %3.1f seconds\n",nextFrame-timeNow());
  }

  while(is_running && peiskmt_isRunning()) {
    /* Check for changes in setting tuples, depending on severity
       possibily break loop so we can reset everything */
    if(updateTupleImageSettings()) break;

    /* Wait for next frame */
    if(nextFrame < timeNow() - 2.0) {
      printf("Warning - too slow, cannot do %f fps\n",wantedFPS);
      nextFrame=timeNow();
    }
    while(timeNow() < nextFrame) {
      struct timespec timeout;
      double t=nextFrame-timeNow(); /* Time to wait */
      timeout.tv_sec = (int) t;
      timeout.tv_nsec = (int) (fmod(t,1.0)*1e9);
      fd_set waitSet;
      FD_ZERO(&waitSet);
      pselect(0,&waitSet,&waitSet,&waitSet,&timeout, NULL);
    }
    nextFrame += 1.0 / wantedFPS;

    /* Set this timepoint as the usertime since this most accuratly
       reflects when the actual capture of the image is done. */
    double thisTime=peiskmt_gettimef();
    peiskmt_tsUser((int) thisTime, (int) (1e6*fmod(thisTime,1.0)));

    if(cameraMode == CAMERA_MODE_DIRECTORY)
      thisFrame = grabDirectoryDevice();
#ifdef USE_V4L
    else if(cameraMode == CAMERA_MODE_V4L1)
      thisFrame = grabV4L1();
#endif
#ifdef USE_V4L2
    else if(cameraMode == CAMERA_MODE_V4L2) 
      thisFrame = grabV4L2();
#endif
#ifdef USE_PLAYER
    else if(cameraMode == CAMERA_MODE_PLAYER)
      thisFrame = grabPlayer();
#endif
    else 
      thisFrame=NULL;
        /* If we fail to grab an image, reset and try again */
    if(!thisFrame) break;


    /* Note - we need to do the grabbing etc. even if the image is not used just to keep the synchronisation
       of everything in order. Therefore this has been moved down
       here. */
    /* TODO: Actually, we could just add the right time to next frame
       anyway manually - and get rid of this cpu waste here */
    if(!peiskmt_hasSubscriber("image") && !force_capture) continue;

    /* Computes averages of multiple images before sending, usefull in very low lighting condition to
       reduce colour range noise */
    if(integralSteps>1) {
      if(framecnt % integralSteps == integralSteps-1) doSend=1;
      else doSend=0;
      
      /* integrate image */
      int *intp;
      unsigned char *datap;
      intp=integratedImage;
      datap=thisFrame; //buffer+mbuf.offsets[frame];
      for(y=0;y<height;y++)
	for(x=0;x<width;x++) {
	  *(intp++) += *(datap++); // red
	  *(intp++) += *(datap++); // green
	  *(intp++) += *(datap++); // blue
	}

      double tA=timeNow();

      if(doSend) {
	/* Prepare to send image, copy from intergrated image back to image buffer */
	intp=integratedImage;
	datap=thisFrame; //buffer+mbuf.offsets[frame];
	for(y=0;y<height;y++)
	  for(x=0;x<width;x++) {
	    *(datap++) = (unsigned char) ((*(intp++))/integralSteps); // red
	    *(datap++) = (unsigned char) ((*(intp++))/integralSteps); // green
	    *(datap++) = (unsigned char) ((*(intp++))/integralSteps); // blue
	  }
	/* clear integrated image */     
	memset(integratedImage,0,width*height*3*sizeof(int));	
      }

      double tB=timeNow();
      static double totTime=0.0;
      totTime = totTime*0.95 + 0.05 * (tB - tA);
      //printf("Time 1: %f\n",totTime);
    } else doSend=1;    


    /*                 */
    /* Prepare to send */
    /*                 */
 
    doSend=1;

    if(doSend) {

      /* Create the packed image data only if we are going to send the image*/
      double tA=timeNow();
      /* If neccessary swap byte order of pixels */
      if(switchRGB) {
	unsigned char *pixels=thisFrame; //buffer+mbuf.offsets[frame];
	for(y=0;y<height;y++)
	  for(x=0;x<width;x++) {
	    unsigned char tmp=pixels[2];
	    pixels[2]=pixels[0]; 
	    pixels[0]=tmp;
	    pixels += 3;
	  }
      }
 	/* Checks autoBrightness switch, calculates total mean rgb and adjusts the brightness according to adjust_bright(&pict, device)
	   call, then updates the tuple of brightness . Else sets to default*/
#ifdef V4L1
      if (autoBrightness){

	totmean = get_pic_mean( width, height, thisFrame, 0, 0, 0,width, height );
	adjust_bright(&pict, device);
	wantedBrightness = pict.brightness;
	snprintf(str,256,"%d",(int) wantedBrightness);  
	peiskmt_setStringTuple("settings.brightness",str);
        peiskmt_getTuple(peiskmt_peisid(),"settings.brightness",NULL,NULL,0);
	
      } else {
	pict.brightness = 32768;
      }
#endif
      
      
      if(flipImage ^ (rotateImage == 180)) {
	for(y=0;y<height;y++)
	  memcpy(improcBuffer+y*3*width,thisFrame+(height-y-1)*3*width,3*width);
	memcpy(thisFrame,improcBuffer,width*height*3);
      }
      
      
      if(rotateImage == 180)
	for(y=0;y<height;y++) {
	  p1=thisFrame+width*y*3;
	  p2=thisFrame+width*y*3+(width-1)*3;
	  for(x=0;x<width/2;x++) {
	    c1=p2[0]; p2[0]=p1[0]; p1[0]=c1;
	    c2=p2[1]; p2[1]=p1[1]; p1[1]=c2;
	    c3=p2[2]; p2[2]=p1[2]; p1[2]=c3;
	    p1 += 3; p2 -= 3;
	  }
	}


      /* Conversion to packed data */
      if (outputPNG) {
        size=png_write_file((unsigned char*) sendbuffer,
                            PNG_BUFFER_SIZE, thisFrame, width, height);
      } else {
        size=jpg_output_JPEG(NULL,
			     (unsigned char*) sendbuffer,
			     (JSAMPLE*)thisFrame, //(buffer+mbuf.offsets[frame]),
			     width,height,minQuality,1);
      }

      double tB=timeNow();
      static double totTime=0.0;
      totTime = totTime*0.95 + 0.05 * (tB - tA);
      //printf("Time 2: %f\n",totTime);
      /* Send data to clients */
      printf("Writing %d bytes to tuple...\n",size);
      peiskmt_setTuple("image",size,sendbuffer,outputPNG?"image/png":"image/jpg",PEISK_ENCODING_BINARY);

      /* Increment image counter */
      imageCnt++;
      snprintf(str,255,"%d",imageCnt++);
      peiskmt_setStringTuple("image.count",str);

    }

    /* Queue frame to be grabbed again after we are finished with the buffer */
#ifdef USE_V4L
    if(cameraMode == CAMERA_MODE_V4L1) {
      v_mmap.frame=frame;
      retval=ioctl(device,VIDIOCMCAPTURE,&v_mmap);
      frame=(frame+1)%mbuf.frames;
      framecnt++;
    }
#endif
  }

  freeCamera();
  free(sendbuffer);
  if(conversionBuffer) free(conversionBuffer);
  if(integratedImage) free(integratedImage);
  if(improcBuffer) free(improcBuffer);
}

void readTupleImageSettings() {
  int i;
  int len;
  char *value;
  PeisTuple *tuple;

#define readSetting(tupleName,parsing,...) { tuple=peiskmt_getTuple(peiskmt_peisid(),tupleName,PEISK_KEEP_OLD); if(tuple) sscanf(tuple->data,parsing,__VA_ARGS__); }
#define readSetting2(tupleName) { tuple=peiskmt_getTuple(peiskmt_peisid(),tupleName,PEISK_KEEP_OLD); }

  readSetting("settings.size","(%d %d)",&wantedWidth,&wantedHeight);
  readSetting("settings.quality","%d",&minQuality);
  readSetting("settings.flip-image","%d",&flipImage);
  readSetting("settings.auto-brightness","%d",&autoBrightness);
  readSetting("settings.brightness","%d",&wantedBrightness);
  readSetting2("settings.device"); if(tuple) camera=strdup(tuple->data);
  readSetting2("settings.fps"); if(tuple) {sscanf(tuple->data,"%d",&i); wantedFPS=(double)i;}

  /*
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.size",&len,(void**)&value,PEISK_KEEP_OLD))
    sscanf(value,"(%d %d)",&wantedWidth,&wantedHeight);
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.quality",&len,(void**)&value,PEISK_KEEP_OLD))
    sscanf(value,"%d",&minQuality);
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.device",&len,(void**)&value,PEISK_KEEP_OLD))
    camera=value;
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.flip-image",&len,(void**)&value,PEISK_KEEP_OLD))
    sscanf(value,"%d",&flipImage);
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.autoBrightness",&len,(void**)&value,PEISK_KEEP_OLD))
    sscanf(value,"%d",&autoBrightness);
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.brightness",&len,(void**)&value,PEISK_KEEP_OLD))
    sscanf(value,"%d",&wantedBrightness);
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.fps",&len,(void**)&value,PEISK_KEEP_OLD)) {
    sscanf(value,"%d",&i);
    wantedFPS=(double) i;
    }*/
}

#ifdef V4L1
static int
 setVideoPict (struct video_picture *videopict, int fd)
{
	if (ioctl (fd, VIDIOCSPICT, videopict) < 0) {
		perror ("Couldnt get videopict params with VIDIOCSPICT\n");
		return -1;
	}
	printf ("VIDIOCSPICT\n");
	printf ("brightness=%d hue=%d color=%d contrast=%d whiteness=%d \n",
		videopict->brightness, videopict->hue,
		videopict->colour, videopict->contrast, videopict->whiteness);
		printf("depth=%d palette=%d \n",videopict->depth, videopict->palette);
	  
	return 0;
}
#endif




void writeTupleImageSettings() {
  char str[256];

  /* Setup tuples describing our current settings (in case they have changed) */
  /* Also make a get on the tuples so they are marked as old */
  snprintf(str,255,"(%d %d)",width,height);
  peiskmt_setStringTuple("settings.size",str); peiskmt_getTuple(peiskmt_peisid(),"settings.size",0);
  peiskmt_setStringTuple("settings.device",camera); peiskmt_getTuple(peiskmt_peisid(),"settings.device",0);
  snprintf(str,256,"%d",flipImage);  peiskmt_setStringTuple("settings.flip-image",str); peiskmt_getTuple(peiskmt_peisid(),"settings.flip-image",0);
  snprintf(str,256,"%d",minQuality);  peiskmt_setStringTuple("settings.quality",str); peiskmt_getTuple(peiskmt_peisid(),"settings.quality",0);
  snprintf(str,256,"%d",rotateImage);  peiskmt_setStringTuple("settings.rotate-image",str); peiskmt_getTuple(peiskmt_peisid(),"settings.rotate-image",0);
  snprintf(str,256,"%d",(int) wantedFPS);  peiskmt_setStringTuple("settings.fps",str); peiskmt_getTuple(peiskmt_peisid(),"settings.fps",0);
  snprintf(str,256,"%d",(int) wantedBrightness);  peiskmt_setStringTuple("settings.brightness",str); peiskmt_getTuple(peiskmt_peisid(),"settings.brightness",0);
  snprintf(str,256,"%d",(int) autoBrightness);  peiskmt_setStringTuple("settings.autoBrightness",str); peiskmt_getTuple(peiskmt_peisid(),"settings.autoBrightness",0);
}
      


int updateTupleImageSettings() {
  PeisTuple *tuple;

  /* Check for changes in settings, if so either update them or make a return so that the
     grabbing can start from scratch */
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.size",0))
    return 1;
  tuple=peiskmt_getTuple(peiskmt_peisid(),"settings.quality",PEISK_KEEP_OLD);
  if(tuple) sscanf(tuple->data,"%d",&minQuality);
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.device",0))
    return 1;
  tuple=peiskmt_getTuple(peiskmt_peisid(),"settings.flip-image",0);
  if(tuple) sscanf(tuple->data,"%d",&flipImage);
  tuple = peiskmt_getTuple(peiskmt_peisid(),"settings.rotate-image",0);
  if(tuple) sscanf(tuple->data,"%d",&rotateImage);
  if(peiskmt_getTuple(peiskmt_peisid(),"settings.fps",0))
    return 1;
  tuple=peiskmt_getTuple(peiskmt_peisid(),"settings.brightness",0);
  if(tuple) { sscanf(tuple->data,"%d",&wantedBrightness); return 1;  }
  tuple=peiskmt_getTuple(peiskmt_peisid(),"settings.autoBrightness",0);
  if(tuple) { sscanf(tuple->data,"%d",&autoBrightness); return 1;  }
  return 0;
}


void prepareCamera() {

#ifdef USE_PLAYER
  if(cameraMode == CAMERA_MODE_PLAYER) {
    preparePlayerDevice();
    return;
  }
#endif

  if(strncmp("/dev",camera,4) == 0) {

#ifdef USE_V4L
    if(cameraMode == CAMERA_MODE_V4L1) {
      prepareCameraV4L1Device();
      return;
    }
#endif

#ifdef USE_V4L2
    if(cameraMode == CAMERA_MODE_V4L2) {
      prepareCameraV4L2Device();
      return;
    }
#endif

    printf("No usable camera found. Do you have the correct camera mode (V4L/V4L2/Player/...)?\n");
    exit(1);
  }  else {
    cameraMode=CAMERA_MODE_DIRECTORY;
    prepareCameraDirectoryDevice();
    return;
  }
}

void prepareCameraDirectoryDevice() {
  char modelString[256];
  snprintf(modelString,sizeof(modelString),"file:%s",camera);
  peiskmt_setStringTuple("model",modelString);
  if(camdir_fileImageBuffer) free(camdir_fileImageBuffer);
  camdir_fileImageBuffer=(unsigned char*) malloc(MAX_WIDTH*MAX_HEIGHT*3);
  if(camdir_jpegData) free(camdir_jpegData);
  camdir_jpegData=(unsigned char*) malloc(500*1024);
  camdir_dir=NULL;
}

#ifdef USE_V4L
void prepareCameraV4L1Device() {
  int i;

  device=open(camera,O_RDWR);
  if(!device) {
    printf("Failed to open device %s\n",camera);
    exit(0);
  }
  
  /* Ask for all capabilities and print to user */
  if(debug) printf("Query capabilitiyes\n");
  int retval=ioctl(device,VIDIOCGCAP,&cap);

  peiskmt_setStringTuple("model",cap.name);

  if(debug) {
  printf("return value: %d\n",retval);
  printf("Name: %s\n",cap.name);
  printf("Type: 0x%x %s %s %s %s %s %s %s %s %s %\n",cap.type,
	 cap.type&VID_TYPE_CAPTURE?"capture":"",
	 cap.type&VID_TYPE_TUNER?"tuner":"",
	 cap.type&VID_TYPE_TELETEXT?"teletext":"",
	 cap.type&VID_TYPE_OVERLAY?"overlay":"",
	 cap.type&VID_TYPE_CHROMAKEY?"chromakey":"",
	 cap.type&VID_TYPE_CLIPPING?"clipping":"",
	 cap.type&VID_TYPE_FRAMERAM?"frameram":"",
	 cap.type&VID_TYPE_SCALES?"scales":"",
	 cap.type&VID_TYPE_MONOCHROME?"monochrome":"",
	 cap.type&VID_TYPE_SUBCAPTURE?"subcapture":""
	 );

  printf("channels: %d\n",cap.channels);
  printf("audios: %d\n",cap.audios);
  
  printf("min size: %d %d\n",cap.minwidth,cap.minheight);
  printf("max size: %d %d\n",cap.maxwidth,cap.maxheight);
  }

  /* Query channel to use */
  if(wantedChannel >= cap.channels) { 
    printf("Error - requested channel %d but the device only has %d channels\n",wantedChannel,cap.channels);
    exit(0);
  }
  channel.channel=wantedChannel;

  retval=ioctl(device,VIDIOCGCHAN,&channel);
  if(retval == -1) printf("Failed to query channel %d\n",channel.channel);
  if(debug) {
    printf("VIDIOCGCHAN -> %x\n",retval);
    printf("Channel: %d '%s'\n",channel.channel,channel.name);
    printf("Flags: %x %s %s\n",channel.flags,
	   channel.flags&VIDEO_VC_TUNER?"tuner":"",
	   channel.flags&VIDEO_VC_AUDIO?"audio":"");
    printf("Type: %x %s %s\n",channel.type,
	   channel.type&VIDEO_TYPE_TV?"tv":"",
	   channel.type&VIDEO_TYPE_CAMERA?"camera":"");
    printf("Norm: %x %s %s\n",channel.norm,
	   channel.norm & VIDEO_MODE_PAL ? "PAL" : "",
	   channel.norm & VIDEO_MODE_NTSC ? "NTSC" : "");
  }
  /* And use it */
  channel.norm=videoNorm;
  retval=ioctl(device,VIDIOCSCHAN,&channel);
  if(debug) printf("VIDIOCSCHAN -> %x\n",retval);
  
  /* Query picture/palette to use */
  retval=ioctl(device,VIDIOCGPICT,&pict);
  if(retval == -1) printf("Failed to query palette\n");
  if(debug) {
    printf("VIDIOCGPICT -> %x\n",retval);
  }
  int nKnownFormats=2, knownFormats[2] = {VIDEO_PALETTE_RGB24, VIDEO_PALETTE_YUV420P}; 
  for(i=0;i<nKnownFormats;i++) {
    pict.palette=knownFormats[i];
    pict.brightness = wantedBrightness;
    printf("Using brightness: %d\n",pict.brightness);

    if(ioctl(device,VIDIOCSPICT,&pict) != -1) {
      if(debug) { printf("Using palette: %d (index: %d)\n",knownFormats[i],i); }
      palette=pict.palette;
      break;
    } else if(debug) { printf("palette %d (index: %d) - NOT supported by videocard\n",knownFormats[i],i); }
  }
  if(i==nKnownFormats) {
    printf("Error - failed to negotiate videopalette with graphics card\n");
    exit(0);
  }

  /* Setup requested video window properties */
  win.x=0; win.y=0;
  win.width=max(cap.minwidth,min(cap.maxwidth,wantedWidth));
  win.height=max(cap.minheight,min(cap.maxheight,wantedHeight));
  win.chromakey=0;
  win.flags=0;
  win.clips=NULL;
  win.clipcount=0;
  retval=ioctl(device,VIDIOCSWIN,&win);
  if(debug) printf("VIDIOCSWIN -> %x\n",retval);

  /* And test what we got */
  retval=ioctl(device,VIDIOCGWIN,&win);
  if(debug) printf("VIDIOCGWIN -> %x\n",retval);

  width=win.width;
  height=win.height;
  if(debug) printf("Width: %d, Height. %d\n",width,height);

  /* Get mbuf properties */
  retval=ioctl(device,VIDIOCGMBUF,&mbuf);
  if(debug) {
    printf("VIDIOCGMBUF -> %x\n",retval);
    printf("mbuf.size = %d\n",mbuf.size);
    printf("mbuf.frames = %d\n",mbuf.frames);
    printf("mbuf.offsets = %d %d %d %d %d\n",mbuf.offsets[0],mbuf.offsets[1],mbuf.offsets[2],mbuf.offsets[3],mbuf.offsets[4]);
  }

  conversionBuffer = (unsigned char *) malloc(3*width*height);
  /*
  if(palette != VIDEO_PALETTE_RGB24) 
  else
    conversionBuffer = NULL;
  */
  buffer=(unsigned char*) mmap(0,mbuf.size,PROT_READ|PROT_WRITE,MAP_SHARED,device,0);  
  //buffer=(unsigned char*) mmap(0,mbuf.size,PROT_READ,MAP_SHARED,device,0);  
  if(buffer==MAP_FAILED) {
    perror("mmap");
    exit(-7);
  }
  if(debug) printf("buffer=%p\n",buffer);
  v_mmap.width=width;
  v_mmap.height=height;
  v_mmap.frame=0;
  v_mmap.format=pict.palette;
  
  /* Limit length of camera pipe */
  if(mbuf.frames > pipeLength) mbuf.frames=pipeLength;

  /* Start capturing every frame */
  for(frame=0;frame<mbuf.frames;frame++) {
    v_mmap.frame=frame;
    retval=ioctl(device,VIDIOCMCAPTURE,&v_mmap);
    if(debug) printf("VIDIOCMCAPTURE -> %x\n",retval);
    }
  frame=0;
}
#endif

#ifdef USE_V4L2
void prepareCameraV4L2Device() {
  int i;
  int ret;
  int type;
  char modelString[256];
  int force_yuyv=0;


  if(debug) printf("Opening V4L2 device...");
  device=open(camera,O_RDWR);
  if(!device) {
    printf(" failed to open V4L2 device %s\n",camera);
    exit(0);
  } else if(debug) printf(" ok\n");

  /* Ask for all capabilities and print to user */
  if(debug) printf("Quering capabilitiyes\n");
  ret=ioctl(device,VIDIOC_QUERYCAP,&v4l2_cap);
  if(ret < 0) {
    printf("Unable to query video device\n");
    goto fatal;
  }

  snprintf(modelString,sizeof(modelString),"%s:%s:%s",v4l2_cap.bus_info,v4l2_cap.driver,v4l2_cap.card);
  peiskmt_setStringTuple("model",modelString);

  if((v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
    printf("  Device is unable to capture video\n");
  else if(debug)
    printf("  Device can capture video\n");
  if(debug) {
    if((v4l2_cap.capabilities & V4L2_CAP_STREAMING) == 0)
      printf("  Device cannot stream\n");
    else
      printf("  Device can stream\n");
    if((v4l2_cap.capabilities & V4L2_CAP_READWRITE) == 0) 
      printf("  Device does not support read I/O\n");
    else
      printf("  Device supports read I/O");
  }

  width=wantedWidth; height=wantedHeight;
  v4l2_format = V4L2_PIX_FMT_MJPEG;
  memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
  v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  v4l2_fmt.fmt.pix.width = width;
  v4l2_fmt.fmt.pix.height = height;
  v4l2_fmt.fmt.pix.pixelformat = v4l2_format;
  v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;
  if(!force_yuyv) {
    if(debug) printf("Setting input format MJPEG to device... ");
    ret = ioctl(device, VIDIOC_S_FMT, &v4l2_fmt);
    if(debug) printf("%d\n",ret);
  }
  if(ret < 0 || force_yuyv) {

    // Could not use the MJPEG format, let's try the YUYV format instead
    if(debug) printf("Could not use the MJPEG format, let's try the YUYV format instead\n");
    v4l2_format = V4L2_PIX_FMT_YUYV;
    memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix.width = width;
    v4l2_fmt.fmt.pix.height = height;
    v4l2_fmt.fmt.pix.pixelformat = v4l2_format;
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(device, VIDIOC_S_FMT, &v4l2_fmt);
  }
  if (ret < 0) {
    perror("... unable to set device format\n");
    goto fatal;
  }


  if ((v4l2_fmt.fmt.pix.width != width) ||
      (v4l2_fmt.fmt.pix.height != height)) {
    printf(" Requested format unavailable. New format is %d x %d\n",
	   v4l2_fmt.fmt.pix.width, v4l2_fmt.fmt.pix.height);
    width = v4l2_fmt.fmt.pix.width;
    height = v4l2_fmt.fmt.pix.height;
  }
  if(v4l2_fmt.fmt.pix.pixelformat != v4l2_format) {
    printf("Requested pixel format (yuv/mjpeg) not available\n");
    exit(0);
  }


  /* set framerate */
  struct v4l2_streamparm* setfps;  
  setfps=(struct v4l2_streamparm *) calloc(1, sizeof(struct v4l2_streamparm));
  memset(setfps, 0, sizeof(struct v4l2_streamparm));
  setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  setfps->parm.capture.timeperframe.numerator=1;
  setfps->parm.capture.timeperframe.denominator=(int) wantedFPS; /* TODO */
  if(debug) printf("Requesting framerate %d fps\n",setfps->parm.capture.timeperframe.denominator);
  ret = ioctl(device, VIDIOC_S_PARM, setfps); 
  if(debug) printf("Got framerate %d fps\n",setfps->parm.capture.timeperframe.denominator);
  if(ret < 0) {
    perror("Failed to set framerate ");
    // goto fatal; 
  }

  /* request buffers */
  memset(&v4l2_rb, 0, sizeof(struct v4l2_requestbuffers));
  v4l2_rb.count = num_v4l2_buffers;
  v4l2_rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  v4l2_rb.memory = V4L2_MEMORY_MMAP;

  if(debug) printf("Requesting buffers\n");
  ret = ioctl(device, VIDIOC_REQBUFS, &v4l2_rb);
  if (ret < 0) {
    perror("failed to allocate buffers "); 
    goto fatal;
  }
  /* map the buffers */
  if(debug) printf("setting up buffers\n");
  for (i = 0; i < num_v4l2_buffers; i++) {
    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    v4l2_buf.index = i;
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(device, VIDIOC_QUERYBUF, &v4l2_buf);
    if (ret < 0) {
      perror("Failed to query buffer");
      goto fatal;
    }
    if (debug)
      printf("buffer %d length: %u offset: %u\n", i, v4l2_buf.length, v4l2_buf.m.offset);

    printf("Performing MMAP %d: ",i); fflush(stdout);
    v4l2_mem[i] = (unsigned char*) mmap(0, v4l2_buf.length, PROT_READ, MAP_SHARED, device, v4l2_buf.m.offset);
    printf("%p\n",v4l2_mem[i]);

    if (v4l2_mem[i] == MAP_FAILED) {
      perror("Failed to mmap buffer");
      goto fatal;
    }
    if (debug)
      printf("Buffer %d mapped at address %p.\n", i, v4l2_mem[i]);
  }

  /* Queue the buffers. */
  if(debug) printf("Queuing all buffers\n");
  for (i = 0; i < num_v4l2_buffers; ++i) {
    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    v4l2_buf.index = i;
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(device, VIDIOC_QBUF, &v4l2_buf);
    if (ret < 0) {     
      perror("Failed to queue buffer\n");
      goto fatal;
    }
  }

  if(debug) printf("attempting to use size %dx%d\n",width,height);
  /* TODO... 640k should be enough for everyone! */
  mjpeg_buffer = (unsigned char*) calloc(1,(size_t) width*height*3);
  if(!mjpeg_buffer) { printf("Failed to allocate mjpeg buffer\n"); goto fatal; }
  v4l2_buffer2 = (unsigned char*) calloc(1,(size_t) width*height*3);
  
  /* The v4l2 buffer is allocated by jpeg decoding routine as needed
     */
  /*
  v4l2_buffer = (unsigned char*) calloc(1,(size_t) width*height*3);
  if(!v4l2_buffer) { printf("Failed to allocate v4l2 buffer\n"); goto fatal; }
  */
  
  /* video enable */
  if(debug) printf("Enabling streaming from device\n");
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ret = ioctl(device,VIDIOC_STREAMON, &type);
  if(ret < 0) {
    perror("Streaming failed ");
    exit(0);
  }
  memset((void*)&v4l2_buf,0,sizeof(struct v4l2_buffer));

  return;

 fatal:
  close(device);
  exit(1);
  
}
#endif

#ifdef USE_PLAYER
void preparePlayerDevice() {
  int tries;

  printf("Attemting to connect to player server at %s:%d\n",plyHost,plyPort);
  plyClient = playerc_client_create(NULL, plyHost, plyPort);
  if(!plyClient) {
    fprintf(stderr,"ERROR - failed to create plyer client %s:%d\n",plyHost,plyPort);
    exit(1);
  }
  if(playerc_client_connect(plyClient) != 0) {
    fprintf(stderr,"ERROR - failed to connect to player server %s:%d\n%s\n",plyHost,plyPort,playerc_error_str());
    exit(1);
  }
  plyCamera = playerc_camera_create(plyClient,plyCameraIndex);
  if(!plyCamera) { fprintf(stderr,"ERROR - Could not allocate player camera proxy\n"); exit(1); }
  if(playerc_camera_subscribe(plyCamera,PLAYER_OPEN_MODE)) {
    fprintf(stderr,"ERROR - failed to subscribe to camera:%d from player\n",plyCameraIndex);
    playerc_camera_destroy(plyCamera);
    exit(1);
  }
  printf("Subscribed to camera:%d from player server %s:%d\n",plyCameraIndex,plyHost,plyPort);
  
  /* We don't have a choice here since we cannot influense the size of
     the images generated by player */
  width=wantedWidth;
  height=wantedHeight;

  plyCameraBuffer = (unsigned char *) malloc(MAX_WIDTH*MAX_HEIGHT*3);
}
#endif


void freeCamera() {
#ifdef USE_V4L
  if(cameraMode == CAMERA_MODE_V4L1)
    freeCameraV4L1Device();
#endif
#ifdef USE_V4L2
  if(cameraMode == CAMERA_MODE_V4L2)
    freeCameraV4L2Device();
#endif
#ifdef USE_PLAYER
  if(cameraMode == CAMERA_MODE_PLAYER)
    freePlayerDevice();
#endif
  if(cameraMode == CAMERA_MODE_DIRECTORY)
    freeCameraDirectoryDevice();
}

#ifdef USE_V4L
void freeCameraV4L1Device() {
  munmap(buffer,mbuf.size);
  close(device);    
}
#endif

#ifdef USE_V4L2
void freeCameraV4L2Device() {
  int i;

  if(debug) printf("Freeing camera device\n");
  for(i=0;i<num_v4l2_buffers;i++)
    munmap(v4l2_mem[i],v4l2_buf.length);
  close(device);

  if(v4l2_buffer2) free(v4l2_buffer2);
  v4l2_buffer2=NULL;
}
#endif

#ifdef USE_PLAYER
void freePlayerDevice() {
  if(plyCamera) {
    playerc_camera_destroy(plyCamera);  
    plyCamera=NULL;
  }
  if(plyCameraBuffer) {
    free(plyCameraBuffer);
    plyCameraBuffer=NULL;
  }
    
  /* TODO - make a proper disconnect to the player server also.. */
}
#endif

void freeCameraDirectoryDevice() {
  free(camdir_fileImageBuffer);
  camdir_fileImageBuffer=NULL;
  free(camdir_jpegData);
  camdir_jpegData=NULL;
  if(camdir_dir) closedir(camdir_dir);
  camdir_dir=NULL;
}
/* adjusts brightness */ 
#ifdef V4L1
static int
 adjust_bright( struct video_picture *videopict, int fd)
{
  int newbright;
  //static int oldtotalmean=-1;
  //int difference = totmean - oldtotalmean;
  //if(oldtotalmean == -1) difference=0;
  //oldtotalmean = totmean;
  newbright=videopict->brightness;
  
  if( totmean < BRIGHTMEAN - BRIGHTWINDOW|| 
      totmean > BRIGHTMEAN + BRIGHTWINDOW ){
    newbright += (BRIGHTMEAN-totmean)*SPRING_CONSTANT;
    newbright = clip_to(newbright, V4L_BRIGHT_MIN, V4L_BRIGHT_MAX);
  }
  
  printf("totmean:%03d newbright:%05d\r",totmean,newbright);
  /* Slight adjustment */
  videopict->brightness=newbright;

  /* Use a counter to adjust maximial speed with which we make increments to the brightness, 
   * this is to allow for the fact that a number of "old" images are buffered. */
  //static int refuseChange=0;
 // if(refuseChange) refuseChange--;
  //else if(newbright != wantedBrightness) {
  //	refuseChange=pipeLength+1;
    setVideoPict(videopict,fd);
   // usleep(100000);
    wantedBrightness = newbright;
  //}
}

/*
 * get_pic_mean:  Calculate the mean value of the pixels in an image.
 *                      This routine is used for adjusting the values of
 *                      the pixels to reasonable brightness.
 *
 * Arguments:           width, height = Dimensions of the picture
 *                      buffer    = Buffer to picture.
 *                      is_rgb    = 1 if the picture is rgb, else 0
 *                      start{x,y}, end{x,y} = Region to calculate the
 *                                  mean of.  This MUST be valid before
 *                                  being passed in!
 *
 * Return values:       Returns the average of all the components if rgb, else
 *                      the average whiteness of each pixel if B&W
 */
static int
 get_pic_mean( int width, int height, const unsigned char *buffer,
			  int is_rgb,int startx, int starty, int endx, 
			  int endy )
{
  double rtotal = 0, gtotal = 0, btotal = 0;
  int minrow, mincol, maxrow, maxcol, r, c;
  double bwtotal = 0, area;
  int rmean, gmean, bmean;
  const unsigned char *cp;
    
  minrow = starty;
  mincol = startx;
  maxrow = endy;
  maxcol = endx;

  area = (maxcol-mincol) * (maxrow-minrow);

  c = mincol;
  if( is_rgb ){
    for( r=minrow; r < maxrow; r++ ){
      cp = buffer + (r*width+c)*3;
      for( c=mincol; c < maxcol; c++ ) {
	rtotal += *cp++;
	gtotal += *cp++;
	btotal += *cp++;
      }
    }
    rmean = rtotal / area;
    gmean = gtotal / area;
    bmean = btotal / area;
    return (double)rmean * .299 +
	   (double)gmean * .587 +
           (double)bmean * .114;
  } else {
    for( r=minrow; r < maxrow; r++ ){
      cp = buffer + (r*width+c)*1;
      for( c=mincol; c < maxcol; c++ ) {
	bwtotal += *cp++;
      }
    }
    return (int)(bwtotal / area);
  }
}

static int clip_to(int x, int low, int high){
  if(x<low)
      return low;
  else if (x>high)
      return high;
  else
      return x;
}
#endif


/** Grabs from a directory, returns pointer to decoded memory area. */
unsigned char *grabDirectoryDevice() {
  int tries=1;
  char path[512];
  int len;
  unsigned char *jpgReturn;
  int retval;

  struct dirent *dirent;
  /* Loop until we have either found a valid image or until all files in 
     directory have been traversed atleast once. */  
  while(1) {

    if(!camdir_dir) {
      if(tries-- == 0) { printf("Error - no valid files in directory '%s' found\n",camera); exit(0); }
      camdir_dir=opendir(camera);
      if(!camdir_dir) { printf("Cannot open directory '%s'\n",camera); exit(0); }
    }
      
    dirent = readdir(camdir_dir);
    if(!dirent) { closedir(camdir_dir); camdir_dir=NULL; continue; }

    snprintf(path,sizeof(path),"%s/%s",camera,dirent->d_name);	
    char *suffix=dirent->d_name+strlen(dirent->d_name)-4;
    if(strncasecmp(".png",suffix,4) != 0 &&
       strncasecmp(".jpg",suffix,4) != 0) continue; /* bad filename */

    FILE *fp=fopen(path,"rb");
    if(fp) {
      len=fread(camdir_jpegData, 1, PNG_BUFFER_SIZE, fp);
      fclose(fp);

      if(strncasecmp(".png",suffix,4) == 0) {
	png_read_file(camdir_fileImageBuffer, len, camdir_jpegData, width, height);
	if(width > MAX_WIDTH || height > MAX_HEIGHT) {
	  printf("Warning - too large dimensions of image %s\n", path);
	  continue;
	}
	return camdir_fileImageBuffer;
      } else if(strncasecmp(".jpg",suffix,4) == 0) {
	jpg_JpegBufferToRGB((char*) camdir_jpegData, len, &width, &height, &jpgReturn);
	if(width > MAX_WIDTH || height > MAX_HEIGHT) {
	  printf("Warning - too large dimensions of image %s\n", path);
	  continue;
	}
	memcpy(camdir_fileImageBuffer, jpgReturn, 3*width*height);
	return camdir_fileImageBuffer;
      } 
    }
  }
  
  return NULL;
}

/** Grabs from a V4L1 device, returns pointer to decoded memory area. */
#ifdef USE_V4L
unsigned char *grabV4L1() {
  int retval;

  /* Get next image from the video device directly */
  retval = ioctl(device, VIDIOCSYNC, &frame);
  realFrame = buffer+mbuf.offsets[frame];
  switch(palette) {
  case VIDEO_PALETTE_RGB24:
    return realFrame;
  case VIDEO_PALETTE_YUV420P:
    ccvt_420p_rgb24(width,height,
		    realFrame,
		    realFrame+width*height,
		    realFrame+width*height+(width*height)/4,
		    conversionBuffer);
    return conversionBuffer;
  default:
    printf("ERROR - Unknown videoformat %d, see videodev.h and ccvt.h\n",palette);
    exit(0);
  }
}
#endif

/** Grabs from a V4L2 device, returns pointer to decoded memory area. */
#ifdef USE_V4L2
unsigned char *grabV4L2() {
#define HEADERFRAME1 0xaf

  int ret;
  int decoded_width, decoded_height;

  memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
  v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  v4l2_buf.memory = V4L2_MEMORY_MMAP;
  ret = ioctl(device, VIDIOC_DQBUF, &v4l2_buf);
  if (ret < 0) {
    perror("Failed to dequeue buffer\n");
    goto grab_err;
  }

  if(v4l2_format == V4L2_PIX_FMT_MJPEG) {
    if(v4l2_buf.bytesused <= HEADERFRAME1) {
      printf("Too short data frame received from camera\n");
      return 0;
    }
    memcpy(mjpeg_buffer,v4l2_mem[v4l2_buf.index],v4l2_buf.bytesused);

    ret=jpeg_decode(&v4l2_buffer,mjpeg_buffer,&decoded_width,&decoded_height);
    if(ret < 0) {
      printf("Error decoding jpeg image received from camera\n");
      goto grab_err;
    }
    if(decoded_width != width || decoded_height != height) {
      printf("Error, decoded size %dx%d mismatch requested %dx%d\n",decoded_width,decoded_height,width,height);
      goto grab_err;
    }

    ccvt_yuyv_rgb24(width,height,v4l2_buffer,v4l2_buffer2);

  } else if(v4l2_format == V4L2_PIX_FMT_YUYV) {
    ccvt_yuyv_rgb24(width,height,v4l2_mem[v4l2_buf.index],v4l2_buffer2);

    //printf("I should do this...\n");
    //exit(0);

    //memcpy(v4l2_buffer,v4l2_mem[v4l2_buf.index], (size_t) v4l2_buf.bytesused);
    //printf("We should have decoded  %d\n",v4l2_buf.bytesused);
    //printf("v4l2_buffer = %x with size = %d ",v4l2_buffer,width*height*3);
    /* TODO - decode yuyv data??? */
  }

  ret = ioctl(device, VIDIOC_QBUF, &v4l2_buf);
  if (ret < 0) {
    perror("Unable to requeue buffer (%d).\n");
    goto grab_err;
  }

  return v4l2_buffer2;

 grab_err:
  fprintf(stderr,"*** Error occurred during frame grabbing, ATTEMPTING to reset camera ***\n");
  return NULL;
}
#endif

#ifdef USE_PLAYER
unsigned char *grabPlayer() {
  /* Allow the player library to run a few steps */
  if(playerc_client_peek(plyClient, 1)) {
    void *who = playerc_client_read(plyClient);
  }
  playerc_camera_decompress (plyCamera);

  int plyWidth=plyCamera->width;
  int plyHeight=plyCamera->height;
  static int hasWarned=0;

  if(!plyWidth || !plyHeight) {
    /*printf("Camera not yet ready\n");*/
    return plyCameraBuffer;
  }

  if(!hasWarned && (plyWidth != width || plyHeight != height)) {
    hasWarned=1;
    printf("Warning, rescaling from player native size %d x %d x %dbpp to requested size %d x %d x 24bpp\n",
	   plyWidth,plyHeight,plyCamera->bpp,width,height);
  }
  if(plyCamera->bpp != 24) {
    printf("ERROR - PeisCam cannot (yet) handle %dbpp images from player\n",plyCamera->bpp);
    exit(1);
  }
  if(plyWidth == width && plyHeight == height) {
    memcpy(plyCameraBuffer,plyCamera->image,3*plyWidth*plyHeight);
  } else {
    /* TODO - check the format here also */
    /* Do a simple rescale operation using the nearest neighbour
       approach */
    int x,y,px,py;
    /* TODO A small optimisation can be done by computing the "dest"
       variable inside the for loop instead with simple +3 increments */
    for(x=0;x<width;x++)
      for(y=0;y<height;y++) {
	px=(x*plyWidth)/width;
	py=(y*plyHeight)/height;
	unsigned char *dest=&plyCameraBuffer[(x+y*width)*3];
	unsigned char *source=&plyCamera->image[(px+py*plyWidth)*3];
	dest[0]=source[0];
	dest[1]=source[1];
	dest[2]=source[2];
      }
  }

  return plyCameraBuffer;
}
#endif
