
/*
 * png_utils.h -- PNG support for PEIScam
 *
 * 2006 Jonas Melchert
 */

#ifndef _PEISCAM_PNGUTILS_H_
#define _PEISCAM_PNGUTILS_H_ 1

/*
 * max. size of internal PNG file buffer
 */
#define PNG_BUFFER_SIZE 1000000

/*
 * writes PNG file of image DATA (size WIDTH x HEIGHT) to BUFFER
 * (of max. SIZE bytes) and returns the size of the PNG file
 * DATA is a RGB888 color image (interleafed channels): RGBRGBRGB...
 */
size_t
png_write_file( unsigned char * buffer, size_t size, unsigned char * data,
                int width, int height );

/*
 * reads PNG file of image DATA (size WIDTH x HEIGHT) from BUFFER
 * (of max. SIZE bytes)
 * DATA is a RGB888 color image (interleafed channels): RGBRGBRGB...
 * (storage of WIDTHxHEIHTx3 bytes must already be allocated)
 */
int
png_read_file( unsigned char * buffer, size_t size, unsigned char * data,
               int width, int height );

#endif /* !_PEISCAM_PNGUTILS_H_ */
