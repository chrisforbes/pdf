#define WIN32_ULTRA_LEAN
#include <windows.h>

#include <cstdlib>
#include "zlib/zlib.h"
#define FLATE_IMPL
#include "flate.h"

#ifdef NDEBUG
#pragma comment( lib, "zlib.lib" )
#else
#pragma comment( lib, "zlibd.lib" )
#endif

#define CHECK_ERR(err, msg) \
	{ \
		if (err != Z_OK) { \
			OutputDebugStringA( "(Flate) Fail: " msg ); \
			return NULL; \
		} \
	}

char* Inflate( const char* inputStart, const char* inputEnd, size_t* length, void* (*alloc)( void*, size_t ) )
{
	int err;

	z_stream d_stream;
	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in = (unsigned char*)inputStart;
	d_stream.avail_in = inputEnd - inputStart;

	err = inflateInit( &d_stream );
	CHECK_ERR(err, "inflateInit");

	size_t bufFilled = 0;
	size_t bufSize = 4096;
	unsigned char* buf = (unsigned char*)alloc( NULL, 4096 );
	for(;;)
	{
		d_stream.next_out = buf + bufFilled;
		d_stream.avail_out = bufSize - bufFilled;
		err = inflate( &d_stream, Z_NO_FLUSH );

		bufFilled = d_stream.next_out - buf;

		if( err == Z_STREAM_END )
			break;
		CHECK_ERR(err, "inflate");

		bufSize *= 4;
		buf = (unsigned char*)alloc( buf, bufSize );
	}

	if( bufFilled < bufSize )
		buf = (unsigned char*)alloc( buf, bufFilled );

	err = inflateEnd( &d_stream );
	CHECK_ERR(err, "inflateEnd");

	*length = bufFilled;
	return (char*)buf;
}
