#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <jpeglib.h>

#define PNG_PIXEL_DEPTH 8

typedef struct  {
    uint8_t *pixels;
    size_t width;
    size_t height;
} bitmap_t;

static int space = 10;
static int width = 2;

static int verbose_flag = 0;
static int ting_flag = 0;
static int half_dpi = 0;

unsigned char *raw_image = NULL;
int read_jpeg_file( char *filename, struct jpeg_decompress_struct *cinfo )
{
	/* these are standard libjpeg structures for reading(decompression) */
	struct jpeg_error_mgr jerr;
	/* libjpeg data structure for storing one row, that is, scanline of an image */
	JSAMPROW row_pointer[1];

	FILE *infile = fopen( filename, "rb" );
	unsigned long location = 0;
	unsigned int i = 0;

	if ( !infile )
	{
		printf("Error opening jpeg file %s\n!", filename );
		return -1;
	}
	/* here we set up the standard libjpeg error handler */
	cinfo->err = jpeg_std_error( &jerr );
	/* setup decompression process and source, then read JPEG header */
	jpeg_create_decompress( cinfo );
	/* this makes the library read from infile */
	jpeg_stdio_src( cinfo, infile );
	/* reading the image header which contains image information */
	jpeg_read_header( cinfo, TRUE );
	/* Uncomment the following to output image information, if needed. */
	if(verbose_flag){
		printf( "JPEG File Information: \n" );
		printf( "Image width and height: %d pixels and %d pixels->\n", cinfo->image_width, cinfo->image_height );
		printf( "Color components per pixel: %d.\n", cinfo->num_components );
		printf( "Color space: %d.\n", cinfo->jpeg_color_space );
	}
	/* Start decompression jpeg here */
	jpeg_start_decompress( cinfo );

	/* allocate memory to hold the uncompressed image */
	raw_image = (unsigned char*)malloc( cinfo->output_width*cinfo->output_height*cinfo->num_components );
	row_pointer[0] = (unsigned char *)malloc( cinfo->output_width*cinfo->num_components );
	while( cinfo->output_scanline < cinfo->image_height )
	{
		jpeg_read_scanlines( cinfo, row_pointer, 1 );
		for( i=0; i< cinfo->image_width*cinfo->num_components;i++) 
			raw_image[location++] = row_pointer[0][i];
	}
	/* wrap up decompression, destroy objects, free pointers and close open files */
	jpeg_finish_decompress( cinfo );
	jpeg_destroy_decompress( cinfo );
	free( row_pointer[0] );
	fclose( infile );
	return 1;
}

static uint8_t * pixel_at (bitmap_t * bitmap, int x, int y)
{
    return bitmap->pixels + bitmap->width * y + x;
}

static int save_png_to_file (bitmap_t *bitmap, const char *path)
{
    FILE * fp;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    size_t x, y;
    png_byte ** row_pointers = NULL;
    int status = -1;
    int pixel_size = 3;

    fp = fopen (path, "wb");
    if (! fp) {
        goto fopen_failed;
    }

    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        goto png_create_write_struct_failed;
    }

    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
        goto png_create_info_struct_failed;
    }

    if (setjmp (png_jmpbuf (png_ptr))) {
        goto png_failure;
    }

    /* Set image attributes. */

    png_set_IHDR (png_ptr,
                  info_ptr,
                  bitmap->width,
                  bitmap->height,
                  PNG_PIXEL_DEPTH,
                  PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT);
	/* Set the resolution */
	png_set_pHYs(png_ptr, info_ptr, 47244/(half_dpi?2:1), 47244/(half_dpi?2:1), PNG_RESOLUTION_METER)	;

    row_pointers = (unsigned char **) png_malloc (png_ptr, bitmap->height * sizeof (png_byte *));
    for (y = 0; y < bitmap->height; ++y) {
        png_byte *row = (unsigned char *) png_malloc (png_ptr, sizeof (uint8_t) * bitmap->width * pixel_size);
        row_pointers[y] = row;
        for (x = 0; x < bitmap->width; ++x) {
            uint8_t* pixel = pixel_at(bitmap, x, y);
            *row++ = (*pixel)?0:255;
            *row++ = (*pixel)?0:255;
            *row++ = (*pixel)?0:255;
        }
    }
    /* Write the image data to "fp". */
    png_init_io (png_ptr, fp);
    png_set_rows (png_ptr, info_ptr, row_pointers);
    png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    status = 0;
    for (y = 0; y < bitmap->height; y++) {
        png_free (png_ptr, row_pointers[y]);
    }
    png_free (png_ptr, row_pointers);

 png_failure:
 png_create_info_struct_failed:
    png_destroy_write_struct (&png_ptr, &info_ptr);
 png_create_write_struct_failed:
    fclose (fp);
 fopen_failed:
    return status;
}

typedef struct {
	uint8_t value[9];
} field;

static int calculateChecksum(int dec) {
	int checksum = 0;
	if(ting_flag){
		checksum  = (((dec >> 2) ^ (dec >> 8) ^ (dec >> 12) ^ (dec >> 14)) & 0x01) << 1;
		checksum |= (((dec) ^ (dec >> 4) ^ (dec >>  6) ^ (dec >> 10)) & 0x01);
	}else{
		checksum  = (((dec >> 2) ^ (dec >> 8) ^ (dec >> 12) ^ (dec >> 14)) & 0x01) << 1;
		checksum |= (((dec) ^ (dec >> 4) ^ (dec >>  6) ^ (dec >> 10)) & 0x01);
		checksum ^= 0x02; //needs to be tested
	}
	return checksum;
}

field create_field(int value){
	field tmp = {.value = 0};
	int counter = 0;
	tmp.value[8] = calculateChecksum(value);
	while (value){
		tmp.value[counter] = value%4;
		value = value/4;
		counter++;
	}
	return tmp;
}
int get_distance(){
	return (width + space);
}
/* Sets a single pixel to the absolute x and y coordinate */
void set_pixel(bitmap_t* image, int x, int y){
	uint8_t* pixel = pixel_at (image, x, y);
	if(verbose_flag){
		printf("Set pixel at %i, %i\n", x,y);
	}
	*pixel = 1;
}
/* Sets a set of 4 pixel, x and y are absolute */
void set_pixels(bitmap_t* image, int x, int y){
	set_pixel(image, x, y);
	if(!half_dpi){
		set_pixel(image, x+1, y);
		set_pixel(image, x, y+1);
		set_pixel(image, x+1, y+1);
	}
}

void makeAbsoluteCoordinates(int* x, int* y){
	*x*=get_distance();
	*y*=get_distance();
}

void set_frame(bitmap_t* image, int x, int y){
	makeAbsoluteCoordinates(&x, &y);
	set_pixels(image, x, y);
}
void set_frame_marker(bitmap_t* image, int x, int y){
	makeAbsoluteCoordinates(&x, &y);
	set_pixels(image, x+1, y);
}

void set_point(bitmap_t* image, int value, int x, int y){
	makeAbsoluteCoordinates(&x, &y);
	x += ((value == 0 || value == 3)?1:-1)*2;
	y += ((value == 0 || value == 1)?1:-1)*2;
	set_pixels(image, x, y);
}

int main (int argc, char* argv[])
{
    bitmap_t image;
    unsigned long x;
    unsigned long y;
	field myfield;
	char *filename;
	char *imagefilename = NULL;
	int c;
	struct jpeg_decompress_struct cinfo;

    image.width = 100;
    image.height = 100;

    int option_index = 0;
	while (1)
    {
      static struct option long_options[] = {
		  {"sizex",required_argument,0,'x'},
		  {"sizey",required_argument,0,'y'},
		  {"file",required_argument,0,'f'},
		  {"jpg",required_argument,0,'j'},
		  {"halfdpi",no_argument,&half_dpi,1},
		  {"verbose",no_argument,&verbose_flag,1},
		  {0,0,0,0}};
	  /* getopt_long stores the option index here. */

      c = getopt_long (argc, argv, "x:y:f:j:",
                       long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c)
        {
        case 'x':
          image.width = atoi(optarg);
          break;

        case 'y':
          image.height = atoi(optarg);
          break;

        case 'f':
          filename = optarg;
          break;

        case 'j':
          imagefilename = optarg;
          break;

		case 0:
		  break;
        case '?':
          /* getopt_long already printed an error message. */
          break;

        default:
		  printf ("Unrecognized option: %c\n", c);
          abort ();
        }
    }

	if(half_dpi){
		image.height = image.height/2;
		image.width = image.width /2;
	}

	if(imagefilename != NULL){
		read_jpeg_file(imagefilename, &cinfo);
		image.height = cinfo.image_height;
		image.width = cinfo.image_width;
	}
	
	myfield = create_field(atoi(argv[option_index]));
	printf("Value is : %i\n", atoi(argv[argc-1]));
	printf("Checksum is: %i\n", myfield.value[8]);

    /* Create an image. */
    image.pixels = calloc (sizeof (uint8_t), image.width * image.height);

    for (y = 0; y < image.height/get_distance(); y++) {
        for (x = 0; x < image.width/get_distance(); x++) {
			if(raw_image){
				if(raw_image[y*get_distance()*image.width + x*get_distance()] >= 250 &&
						(cinfo.num_components == 1 || (
						raw_image[y*get_distance()*image.width + x*get_distance() + 1] >= 250 &
						raw_image[y*get_distance()*image.width + x*get_distance() + 2] >= 250))){
					continue;	
				}
			}
			int fieldx = x & 3;
			int fieldy = y & 3;
			if(fieldx == 0 && fieldy == 2){
				set_frame_marker(&image, x,y);
			}else
			if(fieldx == 0 || fieldy == 0){
				set_frame(&image, x,y);
			}
			else{
				set_point(&image, myfield.value[8-(fieldx-1)-((fieldy-1)*3)], x,y);
			}
        }
    }

    /* Write the image to a file. */
    save_png_to_file (&image, filename);

    return 0;
}
