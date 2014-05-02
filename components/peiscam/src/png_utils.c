
/*
 * png_utils.h -- PNG support for PEIScam
 *
 * 2006 Jonas Melchert
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <png.h>

#include "png_utils.h"

/*
 * internal PNG file buffer data structure
 */
struct png_file_buffer {
    size_t size;
    size_t count;
    char * data;
};
typedef struct png_file_buffer * png_file_bufferp;

/*
 * reads LENGTH bytes of DATA from the PNG file buffer in PNG_PTR
 */
void
png_read_buffer( png_structp png_ptr, png_bytep data, png_size_t length ) {
    png_file_bufferp buffer;

    assert( png_ptr );
    assert( data );

    /* copy data from buffer */
    buffer = (png_file_bufferp) png_get_io_ptr( png_ptr );
    if( (buffer->count + length) >= buffer->size ) {
        fprintf( stderr,
                 "png_read_file: png_read_buffer: buffer underflow\n" );
        return;
    }
    memcpy( data, buffer->data + buffer->count, length );
    buffer->count = buffer->count + length;
}
/*
 * writes LENGTH bytes of DATA to the PNG file buffer in PNG_PTR
 */
void
png_write_buffer( png_structp png_ptr, png_bytep data, png_size_t length ) {
    png_file_bufferp buffer;

    assert( png_ptr );
    assert( data );

    /* copy data to buffer */
    buffer = (png_file_bufferp) png_get_io_ptr( png_ptr );
    if( (buffer->size - buffer->count) < length ) {
        fprintf( stderr, "png_write_file: png_write_data: buffer overflow\n" );
        return;
    }
    memcpy( buffer->data + buffer->count, data, length );
    buffer->count = buffer->count + length;
}

/*
 * flushes PNG file buffer in PNG_PTR
 */
void
png_flush_buffer( png_structp png_ptr ) {
    /* do nothing */
}

/*
 * writes PNG file of image DATA (size WIDTH x HEIGHT) to BUFFER
 * (of max. SIZE bytes) and returns the size of the PNG file
 * DATA is a RGB888 color image (interleafed channels): RGBRGBRGB...
 */
size_t
png_write_file( unsigned char * buffer, size_t size, unsigned char * data,
                int width, int height ) {
    png_structp writer;
    png_infop info;
    struct png_file_buffer file;
    time_t now;
    png_time modtime;
    png_bytepp rows;
    int i;

    assert( buffer );
    assert( data );

    /* initialize PNG data structures */
    writer = png_create_write_struct( PNG_LIBPNG_VER_STRING,
                                      NULL, NULL, NULL );
    if( !writer ) {
        fprintf( stderr, "png_write_file: png_create_write_struct failed\n" );
        return( 0 );
    }
    info = png_create_info_struct( writer );
    if( !info ) {
        png_destroy_write_struct( &writer, (png_infopp) NULL );
        fprintf( stderr, "png_write_file: png_create_info_struct failed\n" );
        return( 0 );
    }
    if( setjmp( png_jmpbuf( writer ) ) ) {
        png_destroy_write_struct( &writer, &info );
        fprintf( stderr, "png_write_file: setjmp(png_jmpbuf) failed\n" );
        return( 0 );
    }

    /* set up file buffer */
    file.size = size;
    file.count = 0;
    file.data = (char * ) buffer;
    png_set_write_fn( writer, &file, png_write_buffer, png_flush_buffer );

    /* initialize png_info_struct */
    png_set_IHDR( writer, info, width, height, 8, PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT );
    /* set file creation date and time */
    now = time( NULL );
    png_convert_from_time_t( &modtime, now );
    png_set_tIME( writer, info, &modtime );

    /* create PNG file image data and write to buffer */
    rows = (png_bytepp) malloc( sizeof( png_bytepp) * height );
    if( !rows ) {
        png_destroy_write_struct( &writer, &info );
        fprintf( stderr, "png_write_file: PNG row allocation failed\n ");
        return( 0 );
    }
    for( i = 0; i < height; i++ ) {
        rows[i] = data + (i * width * 3);
    }
    png_set_rows( writer, info, rows );
    png_write_png( writer, info, PNG_TRANSFORM_IDENTITY, NULL );

    /* clean up */
    free( rows );
    png_destroy_write_struct( &writer, &info );

    return( file.count );
}

/*
 * reads PNG file of image DATA (size WIDTH x HEIGHT) from BUFFER
 * (of max. SIZE bytes)
 * DATA is a RGB888 color image (interleafed channels): RGBRGBRGB...
 * (storage of WIDTHxHEIHTx3 bytes must already be allocated)
 */
int
png_read_file( unsigned char * buffer, size_t size, unsigned char * data,
               int width, int height ) {
    png_structp reader;
    png_infop info;
    struct png_file_buffer file;
    png_bytepp rows;
    int i;

    assert( buffer );
    assert( data );

    /* initialize PNG data structures */
    reader = png_create_read_struct( PNG_LIBPNG_VER_STRING,
                                     NULL, NULL, NULL );
    if( !reader ) {
        fprintf( stderr, "png_read_file: png_create_read_struct failed\n" );
        return( 0 );
    }
    info = png_create_info_struct( reader );
    if( !info ) {
        png_destroy_read_struct( &reader, (png_infopp) NULL, NULL );
        fprintf( stderr, "png_read_file: png_create_info_struct failed\n" );
        return( 0 );
    }
    if( setjmp( png_jmpbuf( reader ) ) ) {
        png_destroy_read_struct( &reader, &info, NULL );
        fprintf( stderr, "png_read_file: setjmp(png_jmpbuf) failed\n" );
        return( 0 );
    }
        
    /* set up file buffer */
    file.size = size;
    file.count = 0;
    file.data = (char * ) buffer;
    png_set_read_fn( reader, &file, png_read_buffer );

    /* read the PNG file */
    png_read_info( reader, info );
    /* check the image size */
    if( ((int) png_get_image_width( reader, info ) != width)
        || ((int) png_get_image_height( reader, info ) != height) ) {
        fprintf( stderr,
                 "png_read_file: image size does not match requested size\n" );
        png_destroy_read_struct( &reader, &info, NULL );
        return( 0 );
    }
    /* setup the data buffer and read in the image data */
    rows = (png_bytepp) malloc( sizeof( png_bytepp) * height );
    if( !rows ) {
        png_destroy_read_struct( &reader, &info, NULL );
        fprintf( stderr, "png_read_file: PNG row allocation failed\n ");
        return( 0 );
    }
    for( i = 0; i < height; i++ ) {
        rows[i] = data + (i * width * 3);
    }
    png_read_image( reader, rows );
    png_read_end( reader, NULL );

    /* clean up */
    png_destroy_read_struct( &reader, &info, NULL );
    free( rows );

    return( 1 );
}
