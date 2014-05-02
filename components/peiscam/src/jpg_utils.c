/*********************************************************************
 *
 *              Copyright (C) 2001  Zbigniew Wasik
 *  
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "jpg_utils.h"


static unsigned char* jpg_data = NULL ;  // this should be removed on exit
static int jpg_data_size = 0 ;

/*********************************************************************
 * 
 *  Functions implemented in this file:
 */
int jpg_ReadJpegToRGB(  char *fname,
			       int *w_ret, int *h_ret,
			       unsigned char **rgb_return) ;
int jpg_output_JPEG(FILE *outfile, unsigned char *obuffer, 
		    JSAMPLE *image_buffer, int image_width, int image_height, int quality, int isColor) ;

void jpg_exit() ;

/**************************************************************/
static void   jp_error_exit(j_common_ptr);
static void   jp_error_output(j_common_ptr);
/**************************************************************/
typedef struct my_error_mgr_
{
  struct jpeg_error_mgr pub;
  jmp_buf   setjmp_buffer;
} my_error_mgr;

typedef struct my_error_mgr_ *my_error_ptr;

/**************************************************************/  
static void jp_error_exit( j_common_ptr cinfo)
{
  my_error_ptr myerr;

  myerr = (my_error_ptr) cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);  
}
/**************************************************************/  
static void jp_error_output( j_common_ptr cinfo)
{
  my_error_ptr myerr;
  char         buffer[JMSG_LENGTH_MAX];

  myerr = (my_error_ptr) cinfo->err;
  (*cinfo->err->format_message)(cinfo, buffer);

  fprintf(stderr, "%s\n", buffer);  
}

/**************************************************************
 * JPEG Compression thingy.
 */

/****************************** compression destination mamager ***********************/
typedef struct {
  struct jpeg_destination_mgr pub; /* public fields */
  JOCTET *buffer;		   /* start of buffer */
  int     freebytes;
  int     nbytes;
} my_destination_mgr;
typedef my_destination_mgr * my_dest_ptr;

static void init_m_destination(j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = dest->freebytes;
  dest->nbytes=0;
}
static int empty_m_output_buffer(j_compress_ptr cinfo)
{
  /* should never be called */
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  fprintf(stderr, "Error during JPEG compression\n");
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = dest->freebytes;
  return TRUE;
}
static void term_m_destination(j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  dest->nbytes = dest->freebytes - dest->pub.free_in_buffer;
}
static void jpeg_my_dest(j_compress_ptr cinfo, unsigned char *buffer, int nbytes)
{
  my_dest_ptr dest;
  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(my_destination_mgr));
  }
  dest = (my_dest_ptr) cinfo->dest;
  dest->pub.init_destination = init_m_destination;
  dest->pub.empty_output_buffer = empty_m_output_buffer;
  dest->pub.term_destination = term_m_destination;
  dest->freebytes=nbytes;
  dest->buffer=buffer;
}
/****************************** decompression source manager ***********************/
static void init_source (j_decompress_ptr cinfo)
{
  /* NOOP */
}

const char fake_eoi[2] = { (JOCTET) 0xFF, (JOCTET) JPEG_EOI } ;

static boolean fill_input_buffer (j_decompress_ptr cinfo)
{
  struct jpeg_source_mgr *src = cinfo->src;

#if 1
  /* Insert a fake EOI marker */
  src->next_input_byte = fake_eoi;
  src->bytes_in_buffer = 2;

  WARNMS(cinfo, JWRN_JPEG_EOF);

  return TRUE;
#else
  return FALSE ;
#endif

}

static void skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  struct jpeg_source_mgr *src = cinfo->src;

  if (num_bytes > 0) {
    while (num_bytes > (long) src->bytes_in_buffer) {
      num_bytes -= (long) src->bytes_in_buffer;
      (void) fill_input_buffer(cinfo);
      /* note we assume that fill_input_buffer will never return FALSE,
       * so suspension need not be handled.
       */
    }
    src->next_input_byte += (size_t) num_bytes;
    src->bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}

static void jpeg_my_source(j_decompress_ptr cinfo, caddr_t buffer, size_t nbyte)
{
  struct jpeg_source_mgr *src;

  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(struct jpeg_source_mgr));
  }

  src = cinfo->src;
  src->next_input_byte = (JOCTET *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				nbyte * sizeof(JOCTET));
  src->init_source = init_source;
  src->fill_input_buffer = fill_input_buffer;
  src->skip_input_data = skip_input_data;
  src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->term_source = term_source;
  src->bytes_in_buffer = nbyte;                /* all image to do at once */
  memcpy((char *)src->next_input_byte, buffer, nbyte); /* copy image */
}

/**************************************************************/
int jpg_ReadJpegToRGB(  char *fname,
			       int *w_ret, int *h_ret,
			       unsigned char **rgb_return)
{
  //static unsigned char             *data = NULL;
  FILE                             *fp = NULL;
  struct jpeg_decompress_struct    cinfo;
  JSAMPROW                         rowptr[1];
  my_error_mgr                     merr;
  int                              w,h,bperpix;
  int size ;

  printf(" ReadJPG: 1.0 \n") ;
  if((fp = fopen(fname, "r")) == NULL)   return 0;
  /* Step 1: allocate and initialize JPEG decompression object */
  printf(" ReadJPG: 2.0 set err mgr\n") ;
  cinfo.err = jpeg_std_error(&merr.pub);
  merr.pub.error_exit     = jp_error_exit;
  merr.pub.output_message = jp_error_output;

  printf(" ReadJPG: 3.0 set error mgr\n") ;
  if(setjmp(merr.setjmp_buffer)) 
    {
      /* jpeg has signaled an error */
      jpeg_destroy_decompress(&cinfo);
      (void)fclose(fp); 
      return(0);
    }
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */
  printf(" ReadJPG: 4.0 data source file\n") ;
  jpeg_stdio_src(&cinfo, fp);

  /* Step 3: read file parameters with jpeg_read_header() */
  printf(" ReadJPG: 5.0 header \n") ;
  (void) jpeg_read_header(&cinfo, TRUE);

  printf(" ReadJPG: 6.0 header \n") ;
  jpeg_calc_output_dimensions(&cinfo);
  w = cinfo.output_width;
  h = cinfo.output_height;
 
  printf(" ReadJPG: 7.0 header \n") ;
  bperpix = cinfo.output_components;
  if(bperpix != 1 && bperpix != 3) 
    {
      fprintf(stderr, "%s:  can't read %d-plane JPEG file.",
	      fname, cinfo.output_components);
      jpeg_destroy_decompress(&cinfo);
      fclose(fp); 
      return 0;
    }

  printf(" ReadJPG: 8.0 malloc\n") ;
  
  size = 4 * w * h * sizeof(unsigned char) ;

  if (jpg_data == NULL) {
    if((jpg_data = (unsigned char *)malloc(size)) == NULL) {
      jpeg_destroy_decompress(&cinfo);
      fclose(fp); 
      fprintf(stderr,"ReadJPEG");      
      return 0;
    }
    jpg_data_size = size ;
  } else {
    if (size > jpg_data_size) {
      if((jpg_data = (unsigned char *)malloc(size)) == NULL) {
	jpeg_destroy_decompress(&cinfo);
	fclose(fp); 
	fprintf(stderr,"ReadJPEG");      
	return 0;
      }
      jpg_data_size = size ;
    }
  }
  /* Step 4: set parameters for decompression */
  printf(" ReadJPG: 9.0 start decompress\n") ;
  jpeg_start_decompress(&cinfo);

  /* Step 5: decompress */
  printf(" ReadJPG: 10.0 decompress\n") ;
  while(cinfo.output_scanline < cinfo.output_height) 
    {
      rowptr[0] = (JSAMPROW) &jpg_data[cinfo.output_scanline * w * bperpix];
      (void) jpeg_read_scanlines(&cinfo, rowptr, (JDIMENSION) 1);
    }

  printf(" ReadJPG: 11.0 isGray\n") ;
  if(cinfo.jpeg_color_space == JCS_GRAYSCALE) 
    {
      unsigned char *t1, *t2, c;
      int i,j;
      t1 = jpg_data;
      t2 = t1 + 3* w * h;
      memcpy(t2, t1, w * h);
      for(i = 0; i < h; i++)
	for(j = 0; j < w; j++)
	  {
	    c = *t2++;
	    *t1++ = c; *t1++ = c; *t1++ = c; 
	  }
    }
  /* Step 6: clean up */
  printf(" ReadJPG: 12.0 Clean up\n") ;
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(fp); 
  
  /* finally .... */
  printf(" ReadJPG: 13.0 Final\n") ;
  *w_ret = w;
  *h_ret = h; 
  if(rgb_return)
    {
      *rgb_return = jpg_data;
      return(1);
    }
  fprintf(stderr, "Wrong pointer to rgb buffer \n") ;
  return(0);
}
/**************************************************************/  
int jpg_JpegBufferToRGB(  char *obuffer, int lenght,
			       int *w_ret, int *h_ret,
			       unsigned char **rgb_return)
{
  struct jpeg_decompress_struct    cinfo;
  JSAMPROW                         rowptr[1];
  my_error_mgr                     merr;
  int                              w,h,bperpix;
  int                              size ;

  /* Step 1: allocate and initialize JPEG decompression object */
  cinfo.err = jpeg_std_error(&merr.pub);
  merr.pub.error_exit     = jp_error_exit;
  merr.pub.output_message = jp_error_output;

  if(setjmp(merr.setjmp_buffer)) 
    {
      /* jpeg has signaled an error */
      jpeg_destroy_decompress(&cinfo);
      return(0);
    }
  jpeg_create_decompress(&cinfo);
  /* Step 2: specify data source (eg, a file) */
  jpeg_my_source(&cinfo,obuffer,lenght ) ;

    /* Step 3: read file parameters with jpeg_read_header() */
  (void) jpeg_read_header(&cinfo, TRUE);

  jpeg_calc_output_dimensions(&cinfo);
  w = cinfo.output_width;
  h = cinfo.output_height;
 
  bperpix = cinfo.output_components;
  if(bperpix != 1 && bperpix != 3) 
    {
      fprintf(stderr, "can't read %d-plane JPEG buffer.",
	      cinfo.output_components);
      jpeg_destroy_decompress(&cinfo);
      return 0;
    }
  /*  
  if((data = (unsigned char *)malloc( 4 * w * h *
					 sizeof(unsigned char))) == NULL)
    {
      jpeg_destroy_decompress(&cinfo);
      fprintf(stderr,"ReadJPEG");      
      return 0;
    }
  */
  size = 4 * w * h * sizeof(unsigned char) ;
  
  if (jpg_data == NULL) {
    if((jpg_data = (unsigned char *)malloc(size)) == NULL) {
      jpeg_destroy_decompress(&cinfo);
      fprintf(stderr,"ReadJPEG");      
      return 0;
    }
    jpg_data_size = size ;
  } else {
    if (size > jpg_data_size) {
      if((jpg_data = (unsigned char *)malloc(size)) == NULL) {
	jpeg_destroy_decompress(&cinfo);
	fprintf(stderr,"ReadJPEG");      
	return 0;
      }
      jpg_data_size = size ;
    }
  }

  /* Step 4: set parameters for decompression */
  jpeg_start_decompress(&cinfo);

  /* Step 5: decompress */
  while(cinfo.output_scanline < cinfo.output_height) 
    {
      rowptr[0] = (JSAMPROW) &jpg_data[cinfo.output_scanline * w * bperpix];
      (void) jpeg_read_scanlines(&cinfo, rowptr, (JDIMENSION) 1);
    }

  if(cinfo.jpeg_color_space == JCS_GRAYSCALE) 
    {
      unsigned char *t1, *t2, c;
      int i,j;
      t1 = jpg_data;
      t2 = t1 + 3* w * h;
      memcpy(t2, t1, w * h);
      for(i = 0; i < h; i++)
	for(j = 0; j < w; j++)
	  {
	    c = *t2++;
	    *t1++ = c; *t1++ = c; *t1++ = c; 
	  }
    }
  /* Step 6: clean up */
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  
  /* finally .... */
  *w_ret = w;
  *h_ret = h; 
  if(rgb_return)
    {
      *rgb_return = jpg_data;
      return(1);
    }
  fprintf(stderr, "Wrong pointer to rgb buffer \n") ;
  return(0);
}
/***************************************************************************************
 * output to either outfile or obuffer. Caller is responsible to 
 * allocate a big enough obuffer if output to obuffer is desired.
 *  (w * h * 3 +1)
 *  return nbytes in buffer if out to buffer
 *  return 1 if out to file
 *  return 0 if error
 */
int jpg_output_JPEG(FILE *outfile, unsigned char *obuffer, 
			   JSAMPLE *image_buffer, int image_width, int image_height, int quality, int isColor)
{
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr_ jerr;
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */
  int nbytes;

  //printf("Inside: jpg_output_JPEG invoked \n") ;
  /* Step 1: allocate and initialize JPEG compression object */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit     = jp_error_exit;
  jerr.pub.output_message = jp_error_output;

  if(setjmp(jerr.setjmp_buffer))
    {
      /* jpeg has signaled an error */
      jpeg_destroy_compress (&cinfo);
      return(0);
    }
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);
  
  /* Step 2: specify data destination (eg, a file) */
  if(outfile==NULL) jpeg_my_dest(&cinfo, obuffer, image_width * image_height*3);
  else jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */
  cinfo.image_width = image_width; 	/* image width and height, in pixels */
  cinfo.image_height = image_height;

  if (isColor){
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  } else {
    //printf("Inside: jpg_output_JPEG 1\n") ;
  cinfo.input_components = 1;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_GRAYSCALE; 	/* colorspace of input image */
  }
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  cinfo.dct_method = JDCT_FASTEST;
  cinfo.optimize_coding=FALSE;

  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
  //jpeg_set_quality(&cinfo, 100, TRUE /* best quality <- max value*/);


  /* Step 4: Start compressor */
  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */

  if (isColor){
    row_stride = image_width * 3;	/* JSAMPLEs per row in image_buffer */
  } else {
    row_stride = image_width ;
  }
  //printf("Inside: jpg_output_JPEG 2 row: %d\n",row_stride) ;

  while(cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = &image_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  //printf("Inside: jpg_output_JPEG 3\n") ;

  /* Step 6: Finish compression */
  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  if(outfile) { fflush(outfile); nbytes = 1; }
  else 
    {
      my_dest_ptr dest = (my_dest_ptr) cinfo.dest;
      nbytes = dest->nbytes;
    }

  /* Step 7: release JPEG compression object */
  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);
  /* And we're done! */
  return(nbytes);
}

int jpg_save_JPEG(char* filename, unsigned char *obuffer, 
		  JSAMPLE *image, int width, int height, 
		  int quality, int isColor) {
  FILE * x;
  int err_count=0;
  while ( (!(x = fopen(filename, "w+"))) && (!(err_count++ > 200)) )
    usleep(25000);
  if (err_count>200) {
    return;
  }
  jpg_output_JPEG(x,obuffer,image,width,height,quality,isColor);
  
  fclose(x) ;
}

void jpg_exit() {

  if (jpg_data) 
    free(jpg_data) ;
  jpg_data_size = 0 ;
  jpg_data = NULL ;
}
