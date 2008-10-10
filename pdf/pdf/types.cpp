#include "pch.h"

const char* Stream::ApplyFilter( const Name& filterName, const char* inputStart, const char* inputEnd, size_t * outputLength )
{
	if( filterName.str == String( "FlateDecode" ) )
	{
		return Inflate( inputStart, inputEnd, outputLength, realloc );
	}
	else
	{
		DebugBreak();
		return NULL;
	}
}
