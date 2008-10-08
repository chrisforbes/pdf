#include "pch.h"

const char* Stream::ApplyFilter( const Name& filterName, const char* inputStart, const char* inputEnd )
{
	if( filterName.str == String( "FlateDecode" ) )
	{
		size_t outputLength;
		return Inflate( inputStart, inputEnd, &outputLength, realloc );
	}
	else
	{
		DebugBreak();
		return NULL;
	}
}
