#ifndef __JPG_UTILS_H__
#define __JPG_UTILS_H__

#include <sys/types.h>
#include <stdio.h> 
#include <setjmp.h>
#include <jerror.h>
#include <jpeglib.h>

#define JPG_DEFAULT_QUALITY     75
#define JPG_MAX_QUALITY         100
#define JPG_MIN_QUALITY         0

/**
 * Read Jpeg file into a RGB buffer
 * It reads a file with a JPG image and decompress it into a 
 * rgb buffer (it creates buffer).
 * @param fname File name where is a JPG image
 * @param w_ret Returned width of read image
 * @param h_ret Returned height of read image
 * @param rgb_return created buffer with a RGB image (3 components per pixel)
 * Return 0 in case of error, otherwise 1
 */
extern int jpg_ReadJpegToRGB(  char *fname,
			       int *w_ret, int *h_ret,
			       unsigned char **rgb_return) ;

extern int jpg_JpegBufferToRGB(  char *obuffer, int lenght,
			       int *w_ret, int *h_ret,
			  unsigned char **rgb_return) ;

extern void jpg_exit() ;

/*! \fn int jpg_output_JPEG(FILE *outfile, unsigned char *obuffer, JSAMPLE *image_buffer, int image_width, int image_height, int quality, int isColor) 
 *  \brief Compress a buffer and save it into file or an other buffer
 *
 * Output a RGB/GRAYSCALE buffer to either outfile or obuffer. 
 * Caller is responsible to 
 * allocate a big enough obuffer if output to obuffer is desired.
 * sizeof(obuffer) = (w * h * 3 +1)
 *  return nbytes in buffer if out to buffer
 *  return 1 if out to file
 *  return 0 if error
 * \param outfile Pointer to a file structure
 * \param obuffer Pointer to a buffer where should be a compressed image
 * \param image_buffer Pointer to a source image
 * \param image_width Width of image
 * \param image_height Height of image
 * \param quality Quality of compression Range: 0 - worst, 100 - best, default - 75
 * \param isColor Flag indicating if source image is a color or grayscale. 1 for color, 0 - grayscale
 */
extern int jpg_output_JPEG(FILE *outfile, unsigned char *obuffer, 
		    JSAMPLE *image_buffer, int image_width, int image_height, int quality, int isColor) ;

extern int jpg_save_JPEG(char* filename, unsigned char *obuffer, 
			 JSAMPLE *image, int width, int height, 
			 int quality, int isColor) ;

// Macros
#define jpg_GrayToQFile(outfile,source,width,height,quality) jpg_save_JPEG(outfile,NULL,source,width,height,quality,0)
#define jpg_GrayToFile(outfile,source,width,height) jpg_save_JPEG(outfile,NULL,source,width,height,JPG_DEFAULT_QUALITY,0)
#define jpg_RGBToQFile(outfile,source,width,height,quality) jpg_save_JPEG(outfile,NULL,source,width,height,quality,1)
#define jpg_RGBToFile(outfile,source,width,height) jpg_save_JPEG(outfile,NULL,source,width,height,JPG_DEFAULT_QUALITY,1)

#define jpg_compress_QGray(obuffer,source,width,height,quality) jpg_output_JPEG(NULL,obuffer,source,width,height,quality,0)
#define jpg_compress_GrayImage(obuffer,source,width,height) jpg_output_JPEG(NULL,obuffer,source,width,height,JPG_DEFAULT_QUALITY,0)
#define jpg_compress_QRgbImage(obuffer,source,width,height,quality) jpg_output_JPEG(NULL,obuffer,source,width,height,quality,1)
#define jpg_compress_RgbImage(obuffer,source,width,height) jpg_output_JPEG(NULL,obuffer,source,width,height,JPG_DEFAULT_QUALITY,1)



#endif

