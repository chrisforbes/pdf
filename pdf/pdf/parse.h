#pragma once

#include "types.h"

PObject Parse( const char*& p, char const * end );
PDictionary ParseDict( const char*& p, char const * end );
PArray ParseArray( const char*& p, char const * end );
String ParseContent( const char *& p, char const * end, std::vector<PObject>& intoVector );
size_t UnescapeString( char * dest, char const * src, char const * srcend );