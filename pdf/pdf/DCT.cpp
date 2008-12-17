// Compile without PCH

#include <cstdlib>

extern "C"
{
#	include "../thirdparty/libjpeg/jpeglib.h"
}

#ifdef NDEBUG
#	pragma comment( lib, "../thirdparty/libjpeg/libjpeg.lib" )
#else
#	pragma comment( lib, "../thirdparty/libjpeg/libjpeg_d.lib" )
#endif

static unsigned char* bytes = NULL;

static void my_init_source( j_decompress_ptr cinfo )
{
}

static boolean my_fill_input_buffer( j_decompress_ptr cinfo )
{
	return FALSE;
}

static void my_skip_input_data( j_decompress_ptr cinfo, long skipLength )
{
	cinfo->src->next_input_byte += skipLength;
}

static void my_term_source( j_decompress_ptr cinfo )
{
}

static void jpeg_bytes_src( j_decompress_ptr cinfo, unsigned char* in, size_t length )
{
	cinfo->src = (struct jpeg_source_mgr *)
		cinfo->mem->alloc_small((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(jpeg_source_mgr));

	bytes = in;

	struct jpeg_source_mgr* src = cinfo->src;
	src->init_source = my_init_source;
	src->fill_input_buffer = my_fill_input_buffer;
	src->skip_input_data = my_skip_input_data;
	src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->term_source = my_term_source;
	src->bytes_in_buffer = length;
	src->next_input_byte = bytes;
}

char* DCTDecode( const char* input, size_t inputLength, size_t& outLength, size_t& width, size_t& height )
{
	unsigned char* uinput = (unsigned char*)input;
	struct jpeg_decompress_struct cinfo = {};
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress( &cinfo );

	jpeg_bytes_src( &cinfo, uinput, inputLength );
	jpeg_read_header( &cinfo, true );

	jpeg_start_decompress( &cinfo );
	width = cinfo.output_width;
	height = cinfo.output_height;

	size_t line_size = width * cinfo.output_components;
	size_t output_size = height * line_size;
	unsigned char* ret = new unsigned char[ output_size ];

	while( cinfo.output_scanline < height )
	{
		unsigned char* row[1] = { ret + line_size * cinfo.output_scanline };
		jpeg_read_scanlines( &cinfo, row, 1 );
	}

	outLength = output_size;
	return (char*)ret;
}
